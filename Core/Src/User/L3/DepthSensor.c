/*
 * DepthSensorController.c
 *
 *  Created on: Oct. 21, 2022
 *      Author: Andre Hendricks / Dr. JF Bousquet / Evan Lowe
 */

#include <stdbool.h>
#include <stdio.h>

#include "User/L2/Comm_Datalink.h"
#include "User/L3/DepthSensor.h"

//Required FreeRTOS header files
#include "FreeRTOS.h"
#include "Timers.h"

#define MaxDepth 10000 //100 m
#define StartDepth 20
#define RobotSpeed 20 //cm/s

/******************************************************************************
This is a software callback function.
******************************************************************************/
void RunDepthSensor(TimerHandle_t xTimer)
{
	static uint32_t depth = StartDepth;
	static uint8_t down = true;
	if (down)
		depth += RobotSpeed;
	else
		depth -= RobotSpeed;

	if (depth == MaxDepth)
		down = 0;
	if (depth == StartDepth)
		down = 1;

	send_sensorData_message(Depth, depth);
}

