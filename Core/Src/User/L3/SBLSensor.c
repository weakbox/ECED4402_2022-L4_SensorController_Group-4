/*
 * SBLSensor.c
 *
 *  Created on: Nov 25, 2023
 *      Author: evanl
 */
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <User/L3/SBLSensor.h>

#include "User/L2/Comm_Datalink.h"
#include "FreeRTOS.h"
#include "Timers.h"

#define variance 2
#define mean 7
#define precision 100 //Sensor is accurate to 100 units

uint16_t changeInTime()
{
	uint16_t time;

	//If the robot moves away or towards the drill
	if (rand() % 2)
	{
		time = mean - ((rand() % variance) * precision);
	}
	else
	{
		time = mean + ((rand() % variance) * precision);
	}

	return time;
}

/******************************************************************************
This is a software callback function.
******************************************************************************/
void RunSBLSensor(TimerHandle_t xTimer)
{
	//Three SBL station ping times, equal start distance from robot
	const uint16_t startTime = 100;
	static uint16_t sbl_1 = startTime;
	static uint16_t sbl_2 = startTime;
	static uint16_t sbl_3 = startTime;
	const int endTime = 100*1000000/1500;
	static uint8_t down = true;

	if(down)
	{
		sbl_1 += changeInTime();
		sbl_2 += changeInTime();
		sbl_3 += changeInTime();
	}
	else
	{
		sbl_1 -= changeInTime();
		sbl_2 -= changeInTime();
		sbl_3 -= changeInTime();
	}

	if((sbl_1 >= endTime) || (sbl_2 >= endTime) || (sbl_3 >= endTime))
		down = false;
	if ((sbl_1 <= startTime) || (sbl_2 <= startTime) || (sbl_3 <= startTime))
		down = true;

	send_sensorData_message(SBL, sbl_1);
	send_sensorData_message(SBL, sbl_2);
	send_sensorData_message(SBL, sbl_3);
}

