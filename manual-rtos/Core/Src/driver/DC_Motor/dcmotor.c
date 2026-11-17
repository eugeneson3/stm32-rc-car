/*
 * dcmotor.c
 *
 *  Created on: 2026. 11. 11.
 *      Author: kccistc
 */
#include "dcmotor.h"
#include "main.h"
#include <stdlib.h>
static int motor_leftSpeed = 0;
static int motor_rightSpeed = 0;

static void motor_direction_leds(int left, int right)
{
	/* Sum: forward/reverse. Difference: turn direction, including pivots. */
	int forward = left + right;
	HAL_GPIO_WritePin(LED_FORWARD_GPIO_Port, LED_FORWARD_Pin,
		forward > 0 ? GPIO_PIN_SET : GPIO_PIN_RESET);
	HAL_GPIO_WritePin(LED_REVERSE_GPIO_Port, LED_REVERSE_Pin,
		forward < 0 ? GPIO_PIN_SET : GPIO_PIN_RESET);
	HAL_GPIO_WritePin(LED_LEFT_GPIO_Port, LED_LEFT_Pin,
		right > left ? GPIO_PIN_SET : GPIO_PIN_RESET);
	HAL_GPIO_WritePin(LED_RIGHT_GPIO_Port, LED_RIGHT_Pin,
		left > right ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void motor_init(motor_t *motor, TIM_HandleTypeDef *htim,
		uint32_t Ch,
		GPIO_TypeDef* GPIO1, uint16_t GPIO1_Pin,
		GPIO_TypeDef* GPIO2, uint16_t GPIO2_Pin)
{
	motor->htim = htim;
	motor->Ch= Ch;
	motor->GPIO1 = GPIO1;
	motor->GPIO1_Pin= GPIO1_Pin;
	motor->GPIO2 = GPIO2;
	motor->GPIO2_Pin= GPIO2_Pin;
}
void car_Stop(motor_t *motor_left, motor_t *motor_right)
{

	HAL_TIM_PWM_Stop(motor_left->htim,motor_left->Ch);
	HAL_TIM_PWM_Stop(motor_right->htim,motor_right->Ch);
	motor_direction_leds(0, 0);
}
void motor_set_speed(motor_t *motor_left, motor_t *motor_right,int8_t x_value,int8_t y_value)
{
	if (x_value == 0 && y_value == 0)
	{
		car_Stop(motor_left, motor_right);
		return;
	}
	if (x_value>100) 		x_value = 100;
	else if (x_value<-100) 	x_value = -100;
	if (y_value>100) 		y_value = 100;
	else if (y_value<-100) 	y_value = -100;

	int leftSpeed 	= y_value + x_value;
	int rightSpeed 	= y_value - x_value;

	int peak = 100;
	if (abs(leftSpeed) > peak)
		peak = abs(leftSpeed);

	if (abs(rightSpeed) > peak)
		peak = abs(rightSpeed);

	leftSpeed  = leftSpeed  * 700 / peak;
	rightSpeed = rightSpeed * 700 / peak;

	motor_leftSpeed = leftSpeed;
	motor_rightSpeed = rightSpeed;

	HAL_TIM_PWM_Start(motor_left->htim,motor_left->Ch);
	HAL_TIM_PWM_Start(motor_right->htim,motor_right->Ch);
	//left
	__HAL_TIM_SET_COMPARE(motor_left->htim,motor_left->Ch,abs(motor_leftSpeed) + 300 -1);
	if(motor_leftSpeed >=0)
	{
		HAL_GPIO_WritePin(motor_left->GPIO1, motor_left->GPIO1_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(motor_left->GPIO2, motor_left->GPIO2_Pin, GPIO_PIN_RESET);
	}
	else
	{
		HAL_GPIO_WritePin(motor_left->GPIO1, motor_left->GPIO1_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(motor_left->GPIO2, motor_left->GPIO2_Pin, GPIO_PIN_SET);
	}

	//RIGHT
	__HAL_TIM_SET_COMPARE(motor_right->htim,motor_right->Ch,abs(motor_rightSpeed) + 300 -1);
	if(motor_rightSpeed >= 0)
	{
		HAL_GPIO_WritePin(motor_right->GPIO1, motor_right->GPIO1_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(motor_right->GPIO2, motor_right->GPIO2_Pin, GPIO_PIN_RESET);
	}
	else
	{
		HAL_GPIO_WritePin(motor_right->GPIO1, motor_right->GPIO1_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(motor_right->GPIO2, motor_right->GPIO2_Pin, GPIO_PIN_SET);
	}
	motor_direction_leds(motor_leftSpeed, motor_rightSpeed);
}
