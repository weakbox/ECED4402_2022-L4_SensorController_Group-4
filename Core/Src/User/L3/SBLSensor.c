/*
 * SBLSensor.c
 *
 *  Created on: Nov 25, 2023
 *      Author: Evan Lowe
 */
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <User/L3/SBLSensor.h>

#include "User/L2/Comm_Datalink.h"
#include "FreeRTOS.h"
#include "Timers.h"

#define variance 50
#define mean 267 //Sensors run every 2 seconds, The robot decends at 0.2 m/s
#define startTime 1000
#define endTime 100 * 1000000 / 1500

uint16_t changeInTime()
{
	uint16_t time;

	//If the robot moves away or towards the drill
	if (rand() % 2)
	{
		time = mean + (rand() % variance);
	}
	else
	{
		time = mean - (rand() % variance);
	}

	return time;
}

/******************************************************************************
This is a software callback function.
******************************************************************************/
void RunSBLSensor(TimerHandle_t xTimer)
{
	//Three SBL station ping times, equal start distance from robot
	static uint32_t sbl_1 = startTime;
	static uint32_t sbl_2 = startTime;
	static uint32_t sbl_3 = startTime;
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

	send_sensorData_message(SBL1, sbl_1);
	send_sensorData_message(SBL2, sbl_2);
	send_sensorData_message(SBL3, sbl_3);
}

