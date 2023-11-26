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

#define MAXDEPTH 100

/******************************************************************************
This is a software callback function.
******************************************************************************/
void RunDepthSensor(TimerHandle_t xTimer)
{
	static uint16_t depth = 0.2;
	static uint8_t down = true;
	if(down)
		depth += 0.2;
	else
		depth -=0.2;

	if(depth == MAXDEPTH)
		down = false;
	if(depth == 0)
		down = true;

	send_sensorData_message(Depth, depth);
}

