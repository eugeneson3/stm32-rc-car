/*
 * motor_control_task.c
 *
 *  Created on: 2026. 11. 12.
 *      Author: kccistc
 */
#include "motor_control_task.h"
#include "../../../driver/DC_Motor/dcmotor.h"
#include "../../message.h"
#include "../../../driver/lcd/lcd.h"


extern TIM_HandleTypeDef htim3;



static motor_t motor_L, motor_R;
typedef enum {STOP,RUN,LEFT,RIGHT} motor_state_t;

static btn_status_t btn_status = BTN_NO;
static comm_data_t *pcomm_data;

void motor_control_task_init()
{
	motor_init(&motor_L, &htim3, TIM_CHANNEL_1, GPIOB, GPIO_PIN_6, GPIOA, GPIO_PIN_7);
	motor_init(&motor_R, &htim3, TIM_CHANNEL_3, GPIOA, GPIO_PIN_6, GPIOA, GPIO_PIN_5);
	motor_set_speed(&motor_L, &motor_R, 0, 0);
}
void motor_control_task()
{
	osEvent btnEvent;
	btnEvent = osMessageGet(btnQueHandle, 0);
	if(btnEvent.status == osEventMessage)
	{
		btn_status = btnEvent.value.v;
	}
	else
	{
		btn_status = BTN_NO;
	}

	osEvent commEvent;
	commEvent = osMessageGet(commQueueHandle, 0);
	if(commEvent.status == osEventMessage)
	{
		pcomm_data = commEvent.value.p;
	}
	if(pcomm_data->status == COMM_STOP)
	{
		car_Stop(&motor_L, &motor_R);
	}
	else if(pcomm_data->status == COMM_SPEED)
	{
		if(pcomm_data->comm_x_value == 0 && pcomm_data->comm_y_value == 0)
		{
			car_Stop(&motor_L, &motor_R);
		}
		else
		{
			motor_set_speed(&motor_L, &motor_R,pcomm_data->comm_x_value,pcomm_data->comm_y_value);
		}

		//	default:
		//		car_Stop(&motor_L, &motor_R);
		//		break;
	}
}
