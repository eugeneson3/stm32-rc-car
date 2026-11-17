#include "autonomous.h"

#define SENSOR_SLOT_MS 65U
#define ECHO_TIMEOUT_US 25000U
#define TRIGGER_US 12U
#define BUTTON_DEBOUNCE_MS 30U
#define MOTOR_DEADTIME_MS 3U

typedef struct {
    GPIO_TypeDef *trig_port;
    uint16_t trig_pin;
    uint16_t echo_pin; /* All three ECHO inputs are on GPIOB. */
} sensor_pins_t;
static const sensor_pins_t pins[SENSOR_COUNT] = {
    {GPIOA, GPIO_PIN_8, GPIO_PIN_10},
    {GPIOB, GPIO_PIN_4, GPIO_PIN_5},
    {GPIOA, GPIO_PIN_10, GPIO_PIN_3}
};
/* Give the forward sensor twice the update rate of either diagonal sensor. */
static const sensor_id_t scan_order[] = {SENSOR_FRONT, SENSOR_LEFT, SENSOR_FRONT, SENSOR_RIGHT};
typedef enum { SCAN_IDLE, TRIGGER_HIGH, WAIT_RISE, WAIT_FALL } scan_phase_t;
static scan_phase_t scan_phase;
static uint8_t scan_index;
static uint32_t phase_cycles, last_trigger_ms;
static uint8_t button_raw = 1, button_stable = 1;
static uint32_t button_changed_ms, motors_disabled_ms;
static TIM_HandleTypeDef *pwm;
static navigation_t navigation;
static motion_t pending_motion;

volatile distance_t distances[SENSOR_COUNT];
volatile uint8_t autonomous_running;
volatile motion_t requested_motion;
volatile motion_t actual_motion;

static uint32_t elapsed_us(uint32_t started)
{
    return (uint32_t)(DWT->CYCCNT - started) / (SystemCoreClock / 1000000U);
}

void Autonomous_EmergencyStop(void)
{
    if (pwm == NULL) return;
    __HAL_TIM_SET_COMPARE(pwm, TIM_CHANNEL_1, 0);
    __HAL_TIM_SET_COMPARE(pwm, TIM_CHANNEL_3, 0);
    /* Flush BOTH compare preloads before changing the bridge inputs. */
    pwm->Instance->EGR = TIM_EGR_UG;
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_RESET);
    actual_motion = MOTION_STOP;
}

static void motor_update(motion_t next, uint32_t now)
{
    if (next != pending_motion) {
        Autonomous_EmergencyStop();
        pending_motion = next;
        motors_disabled_ms = now;
    }
    if (next == MOTION_STOP || actual_motion == next ||
        (uint32_t)(now - motors_disabled_ms) < MOTOR_DEADTIME_MS) return;

    uint8_t left_forward = next != MOTION_LEFT;
    uint8_t right_forward = next != MOTION_RIGHT;
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, left_forward ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, left_forward ? GPIO_PIN_RESET : GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, right_forward ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, right_forward ? GPIO_PIN_RESET : GPIO_PIN_SET);
    /* PWM1: compare > ARR is a continuous high output (100% duty). */
    uint32_t full_duty = __HAL_TIM_GET_AUTORELOAD(pwm) + 1U;
    __HAL_TIM_SET_COMPARE(pwm, TIM_CHANNEL_1, full_duty);
    __HAL_TIM_SET_COMPARE(pwm, TIM_CHANNEL_3, full_duty);
    pwm->Instance->EGR = TIM_EGR_UG;
    actual_motion = next;
}

static void button_update(uint32_t now)
{
    uint8_t raw = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_0) == GPIO_PIN_SET;
    if (raw != button_raw) {
        button_raw = raw;
        button_changed_ms = now;
    }
    if (raw == button_stable || (uint32_t)(now - button_changed_ms) < BUTTON_DEBOUNCE_MS)
        return;
    button_stable = raw;
    if (raw) return; /* Active-low button; one toggle per stable press. */

    autonomous_running = !autonomous_running;
    Autonomous_EmergencyStop();
    pending_motion = MOTION_STOP;
    requested_motion = MOTION_STOP;
    Navigation_Reset(&navigation);
    /* Cancel in-flight echoes and require fresh readings after each start. */
    for (unsigned i = 0; i < SENSOR_COUNT; ++i) {
        distances[i].valid = 0;
        HAL_GPIO_WritePin(pins[i].trig_port, pins[i].trig_pin, GPIO_PIN_RESET);
    }
    scan_index = 0;
    scan_phase = SCAN_IDLE;
}

static void finish_measurement(uint8_t valid, uint32_t width_us, uint32_t now)
{
    volatile distance_t *d = &distances[scan_order[scan_index]];
    /* A timeout is unknown, never a fictitious clear path. */
    d->valid = valid && width_us >= 116U && width_us <= 23200U;
    d->cm = d->valid ? (uint16_t)((width_us + 29U) / 58U) : 0U;
    d->measured_ms = now;
    scan_index = (scan_index + 1U) % (sizeof(scan_order) / sizeof(scan_order[0]));
    scan_phase = SCAN_IDLE;
}

static void sensors_update(uint32_t now)
{
    const sensor_pins_t *s = &pins[scan_order[scan_index]];
    uint32_t duration = elapsed_us(phase_cycles);
    switch (scan_phase) {
    case SCAN_IDLE:
        if ((uint32_t)(now - last_trigger_ms) < SENSOR_SLOT_MS) return;
        last_trigger_ms = now;
        if (HAL_GPIO_ReadPin(GPIOB, s->echo_pin) == GPIO_PIN_SET) {
            finish_measurement(0, 0, now);
            return;
        }
        HAL_GPIO_WritePin(s->trig_port, s->trig_pin, GPIO_PIN_SET);
        phase_cycles = DWT->CYCCNT;
        scan_phase = TRIGGER_HIGH;
        break;
    case TRIGGER_HIGH:
        if (duration < TRIGGER_US) return;
        HAL_GPIO_WritePin(s->trig_port, s->trig_pin, GPIO_PIN_RESET);
        phase_cycles = DWT->CYCCNT;
        scan_phase = WAIT_RISE;
        break;
    case WAIT_RISE:
        if (duration >= ECHO_TIMEOUT_US) finish_measurement(0, 0, now);
        else if (HAL_GPIO_ReadPin(GPIOB, s->echo_pin) == GPIO_PIN_SET) {
            phase_cycles = DWT->CYCCNT;
            scan_phase = WAIT_FALL;
        }
        break;
    case WAIT_FALL:
        if (duration >= ECHO_TIMEOUT_US) finish_measurement(0, 0, now);
        else if (HAL_GPIO_ReadPin(GPIOB, s->echo_pin) == GPIO_PIN_RESET)
            finish_measurement(1, duration, now);
        break;
    default:
        Autonomous_EmergencyStop();
        scan_phase = SCAN_IDLE;
        break;
    }
}

void Autonomous_Init(TIM_HandleTypeDef *motor_timer)
{
    pwm = motor_timer;
    Navigation_Reset(&navigation);
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    Autonomous_EmergencyStop();
    if (HAL_TIM_PWM_Start(pwm, TIM_CHANNEL_1) != HAL_OK ||
        HAL_TIM_PWM_Start(pwm, TIM_CHANNEL_3) != HAL_OK) Error_Handler();
    last_trigger_ms = HAL_GetTick() - SENSOR_SLOT_MS;
}

void Autonomous_Update(void)
{
    uint32_t now = HAL_GetTick();
    button_update(now);
    sensors_update(now);
    requested_motion = Navigation_Update(&navigation, distances, autonomous_running, now);
    motor_update(requested_motion, now);
    /* Keep this loop nonblocking: ECHO edges are polled with DWT timestamps. */
}
