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
	static uint8_t going_down = true;

	// print_str("Polling Depth Sensor...\r\n");

	// Calculate the movement of the robot.
	if (going_down)
	{
		depth += RobotSpeed;
	}
	else // Going up
	{
		depth -= RobotSpeed;
	}

	if (depth >= MaxDepth)
	{
		going_down = false;
	}
	if (depth <= StartDepth)
	{
		going_down = true;
	}

	send_sensorData_message(Depth, depth);
}

