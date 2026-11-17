/* Execute the production C state machines on an emulated Cortex-M4.
 * GPIO, HAL tick, PWM registers and DWT are controlled test doubles.
 */
#define __MAIN_H
#include <stdint.h>
#include <stddef.h>
typedef struct { uint32_t input, output; } GPIO_TypeDef;
typedef struct { uint32_t EGR, ARR, CCR1, CCR3; } TIM_TypeDef;
typedef struct { TIM_TypeDef *Instance; } TIM_HandleTypeDef;
typedef enum { GPIO_PIN_RESET, GPIO_PIN_SET } GPIO_PinState;
static GPIO_TypeDef gpio_a, gpio_b, gpio_c;
#define GPIOA (&gpio_a)
#define GPIOB (&gpio_b)
#define GPIOC (&gpio_c)
#define GPIO_PIN_0 (1U << 0)
#define GPIO_PIN_3 (1U << 3)
#define GPIO_PIN_4 (1U << 4)
#define GPIO_PIN_5 (1U << 5)
#define GPIO_PIN_6 (1U << 6)
#define GPIO_PIN_7 (1U << 7)
#define GPIO_PIN_8 (1U << 8)
#define GPIO_PIN_10 (1U << 10)
#define TIM_CHANNEL_1 0U
#define TIM_CHANNEL_3 8U
#define TIM_EGR_UG 1U
#define HAL_OK 0
static struct { uint32_t DEMCR; } core_debug;
static struct { uint32_t CYCCNT, CTRL; } dwt;
#define CoreDebug (&core_debug)
#define DWT (&dwt)
#define CoreDebug_DEMCR_TRCENA_Msk 1U
#define DWT_CTRL_CYCCNTENA_Msk 1U
uint32_t SystemCoreClock = 100000000U;
static uint32_t tick;
static uint32_t hal_error;
#define __HAL_TIM_SET_COMPARE(t, ch, v) do { \
    if ((ch) == TIM_CHANNEL_1) (t)->Instance->CCR1 = (v); \
    else (t)->Instance->CCR3 = (v); } while (0)
#define __HAL_TIM_GET_AUTORELOAD(t) ((t)->Instance->ARR)
static uint32_t HAL_GetTick(void) { return tick; }
static int HAL_TIM_PWM_Start(TIM_HandleTypeDef *t, uint32_t ch) { (void)t; (void)ch; return HAL_OK; }
static void HAL_GPIO_WritePin(GPIO_TypeDef *g, uint16_t pin, GPIO_PinState state)
{
    if (state) g->output |= pin;
    else g->output &= ~pin;
}
static GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *g, uint16_t pin)
{
    return (g->input & pin) ? GPIO_PIN_SET : GPIO_PIN_RESET;
}
void Error_Handler(void) { hal_error = 1; }
#include "../Core/Src/autonomous.c"
#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)

uint32_t tests_main(void)
{
    navigation_t nav;
    distance_t d[3] = {{150, 1, 1000}, {150, 1, 1000}, {150, 1, 1000}};
    Navigation_Reset(&nav);
    CHECK(Navigation_Update(&nav, d, 0, 1000) == MOTION_STOP);
    CHECK(Navigation_Update(&nav, d, 1, 1000) == MOTION_FORWARD);
    d[1].cm = 20; d[0].cm = 100; d[2].cm = 40;
    CHECK(Navigation_Update(&nav, d, 1, 1000) == MOTION_LEFT);
    d[0].cm = 30; d[2].cm = 120;
    CHECK(Navigation_Update(&nav, d, 1, 1100) == MOTION_LEFT);
    CHECK(Navigation_Update(&nav, d, 1, 1250) == MOTION_RIGHT);
    d[0].cm = 70; d[2].cm = 75;
    CHECK(Navigation_Update(&nav, d, 1, 1251) == MOTION_RIGHT);
    for (unsigned i = 0; i < 3; ++i) d[i].measured_ms = 1500;
    CHECK(Navigation_Update(&nav, d, 1, 1500) == MOTION_RIGHT); /* tie keeps direction */
    d[1].cm = 70;
    CHECK(Navigation_Update(&nav, d, 1, 1501) == MOTION_RIGHT); /* hysteresis */
    d[1].cm = 90;
    CHECK(Navigation_Update(&nav, d, 1, 1502) == MOTION_FORWARD);
    d[0].cm = 20;
    CHECK(Navigation_Update(&nav, d, 1, 1503) == MOTION_RIGHT); /* diagonal obstacle */
    d[1].valid = 0;
    CHECK(Navigation_Update(&nav, d, 1, 1504) == MOTION_STOP); /* overrides min turn */
    d[1].valid = 1;
    CHECK(Navigation_Update(&nav, d, 1, 1951) == MOTION_STOP); /* stale */
    d[0].cm = 90;
    for (unsigned i = 0; i < 3; ++i) d[i].measured_ms = 0xfffffff0U;
    CHECK(Navigation_Update(&nav, d, 1, 0x10U) == MOTION_FORWARD); /* tick wrap */

    TIM_TypeDef tim = {.ARR = 999};
    TIM_HandleTypeDef timer = {&tim};
    gpio_c.input = GPIO_PIN_0;
    Autonomous_Init(&timer);
    CHECK(!hal_error && !autonomous_running && tim.CCR1 == 0 && tim.CCR3 == 0);
    gpio_c.input = 0; tick = 1; button_update(tick);
    tick = 20; button_update(tick);
    CHECK(!autonomous_running);
    tick = 31; button_update(tick);
    CHECK(autonomous_running);
    tick = 100; button_update(tick);
    CHECK(autonomous_running); /* held button toggles only once */
    CHECK(!distances[0].valid && !distances[1].valid && !distances[2].valid);

    motor_update(MOTION_FORWARD, 100);
    CHECK(actual_motion == MOTION_STOP && tim.CCR1 == 0);
    motor_update(MOTION_FORWARD, 103);
    CHECK(actual_motion == MOTION_FORWARD && tim.CCR1 == 1000 && tim.CCR3 == 1000);
    CHECK((gpio_b.output & GPIO_PIN_6) && (gpio_a.output & GPIO_PIN_6));
    motor_update(MOTION_LEFT, 104);
    CHECK(actual_motion == MOTION_STOP && tim.CCR1 == 0 && tim.CCR3 == 0);
    motor_update(MOTION_LEFT, 106);
    CHECK(actual_motion == MOTION_STOP);
    motor_update(MOTION_LEFT, 107);
    CHECK(actual_motion == MOTION_LEFT && !(gpio_b.output & GPIO_PIN_6));
    CHECK((gpio_a.output & GPIO_PIN_7) && (gpio_a.output & GPIO_PIN_6));
    motor_update(MOTION_RIGHT, 108);
    motor_update(MOTION_RIGHT, 111);
    CHECK(actual_motion == MOTION_RIGHT && (gpio_b.output & GPIO_PIN_6));
    CHECK((gpio_a.output & GPIO_PIN_5) && !(gpio_a.output & GPIO_PIN_6));
    motor_update(MOTION_STOP, 112);
    CHECK(actual_motion == MOTION_STOP && tim.CCR1 == 0 && tim.CCR3 == 0);

    /* Full scan: assert the actual TRIG and ECHO mappings, pulse conversion. */
    scan_index = 0; scan_phase = SCAN_IDLE; last_trigger_ms = 0;
    for (unsigned slot = 0; slot < 4; ++slot) {
        uint32_t now = 200 + slot * 65;
        sensor_id_t id = scan_order[slot];
        uint16_t expected_trig = id == SENSOR_LEFT ? GPIO_PIN_8 :
                                 id == SENSOR_FRONT ? GPIO_PIN_4 : GPIO_PIN_10;
        uint16_t expected_echo = id == SENSOR_LEFT ? GPIO_PIN_10 :
                                 id == SENSOR_FRONT ? GPIO_PIN_5 : GPIO_PIN_3;
        GPIO_TypeDef *trig_port = id == SENSOR_FRONT ? GPIOB : GPIOA;
        sensors_update(now);
        CHECK(scan_phase == TRIGGER_HIGH && (trig_port->output & expected_trig));
        dwt.CYCCNT += 1200; sensors_update(now);
        CHECK(scan_phase == WAIT_RISE && !(trig_port->output & expected_trig));
        gpio_b.input = expected_echo; sensors_update(now);
        CHECK(scan_phase == WAIT_FALL);
        dwt.CYCCNT += 580000; gpio_b.input = 0; sensors_update(now + 6);
        CHECK(scan_phase == SCAN_IDLE && distances[id].valid && distances[id].cm == 100);
    }
    sensors_update(500); dwt.CYCCNT += 1200; sensors_update(500);
    dwt.CYCCNT += 2500000; sensors_update(525);
    CHECK(!distances[SENSOR_FRONT].valid); /* no rising edge */
    sensors_update(565); dwt.CYCCNT += 1200; sensors_update(565);
    gpio_b.input = GPIO_PIN_10; sensors_update(565);
    dwt.CYCCNT += 2500000; sensors_update(590);
    CHECK(!distances[SENSOR_LEFT].valid); /* stuck-high echo */
    gpio_b.input = 0;

    gpio_c.input = GPIO_PIN_0; tick = 600; button_update(tick);
    tick = 630; button_update(tick);
    gpio_c.input = 0; tick = 650; button_update(tick);
    tick = 680; button_update(tick);
    CHECK(!autonomous_running && actual_motion == MOTION_STOP);
    CHECK(!distances[0].valid && !distances[1].valid && !distances[2].valid);
    return 0;
}
