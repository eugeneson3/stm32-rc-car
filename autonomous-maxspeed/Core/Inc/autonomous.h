#ifndef AUTONOMOUS_H
#define AUTONOMOUS_H

#include "main.h"
#include "navigation.h"

/* Live Expressions: distances[0]=left, [1]=front, [2]=right. */
extern volatile distance_t distances[SENSOR_COUNT];
extern volatile uint8_t autonomous_running;
extern volatile motion_t requested_motion;
extern volatile motion_t actual_motion;

void Autonomous_Init(TIM_HandleTypeDef *motor_timer);
void Autonomous_Update(void);
void Autonomous_EmergencyStop(void);

#endif
