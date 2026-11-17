/*
 * comm_task.c
 *
 *  Created on: 2026. 11. 12.
 *      Author: kccistc
 */


#include <string.h>
#include "comm_task.h"
#include "usart.h"
#include "stm32f4xx_hal.h"
#include "../../message.h"
#include "../../../driver/lcd/lcd.h"
#include <stdio.h>
#include <stdlib.h>

static uint8_t rx_data;
static uint8_t rx_idx = 0;
static uint8_t rx_cplt_flag = 0;
static char rx_buff[32];

static comm_data_t comm_data;
static int x_value;
static int y_value;


void comm_task_init(void)
{
	HAL_UART_Receive_IT(&huart6, &rx_data, 1);
}

void comm_task(void)
{
	if(rx_cplt_flag == 1)
	{
		char buf[30];
		rx_cplt_flag = 0;
		if(strncmp(rx_buff,"RUN",3) == 0)
		{
			comm_data.status = COMM_RUN;
			osMessagePut(commQueueHandle,(uint32_t )&comm_data, 10);
			if(osMutexWait(lcdMutexHandle, osWaitForever)==osOK){
				Lcd_writeStringXY(1, 7, "COM_RUN");
				osMutexRelease(lcdMutexHandle);
			}
		}
		else if(strncmp(rx_buff,"STOP",4) == 0)
		{
			comm_data.status = COMM_STOP;

			osMessagePut(commQueueHandle,(uint32_t )&comm_data, 10);
			if(osMutexWait(lcdMutexHandle, osWaitForever)==osOK){
				Lcd_writeStringXY(1, 7, "COM_STOP");
				osMutexRelease(lcdMutexHandle);
			}
		}
		else if (sscanf(rx_buff,"X%dY%d",&x_value,&y_value) == 2)
		{
			comm_data.status = COMM_SPEED;
			comm_data.comm_x_value= x_value;
			comm_data.comm_y_value= y_value;


			osMessagePut(commQueueHandle,(uint32_t )&comm_data, 10);
			if(osMutexWait(lcdMutexHandle, osWaitForever)==osOK){
				sprintf(buf,"COMM_LSPD %d",comm_data.comm_x_value);
				Lcd_writeStringXY(0, 0, buf);
				sprintf(buf,"COMM_RSPD %d",comm_data.comm_y_value);
				Lcd_writeStringXY(1, 0, buf);
				osMutexRelease(lcdMutexHandle);
			}
		}
		else
		{
			osMessagePut(commQueueHandle,COMM_NO, 10);
			if(osMutexWait(lcdMutexHandle, osWaitForever)==osOK){
				Lcd_writeStringXY(1, 7, "COM_NO");
				osMutexRelease(lcdMutexHandle);
			}
		}
		rx_idx = 0;
	}

}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if(huart->Instance == USART6)
	{
		if(rx_data == '*')
		{
			rx_buff[rx_idx++] = '\0'; // or same as rx_buff[rx_idx++] = 0;(null)
			rx_cplt_flag = 1;
		}
		else
		{
			rx_buff[rx_idx++] = rx_data;
		}

		HAL_UART_Receive_IT(&huart6, &rx_data, 1);
	}
}
