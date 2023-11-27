/*
 * OilSensor.c
 *
 *  Created on: Nov 25, 2023
 *      Author: Evan Lowe
 */

#include <stdlib.h>
#include <stdio.h>

#include "User/L2/Comm_Datalink.h"
#include "User/L3/OilSensor.h"

//Required FreeRTOS header files
#include "FreeRTOS.h"
#include "Timers.h"

#define precision 100 //Sensor is accurate to 100 units
#define incVariance 5
 //Oil will decrease at a faster rate on average because oil floats to surface
#define decVariance 10
#define steadyVariance 2
#define steadyMean 100
#define highVariance 40
#define highMean 500

#define maxCount 5

enum oilStates {steady, oilIncrease, oilDecrease};

char buffer[64];

/******************************************************************************
This is a software callback function.
******************************************************************************/
void RunOilSensor(TimerHandle_t xTimer) //Default 1000 ms
{
	uint8_t highOilChance;
	static uint32_t oil = 1000;
	static enum oilStates oilState = oilDecrease; //Oil pools at surface so the start is highest concentration
	static uint8_t incCount = maxCount;

	// print_str("Polling Oil Sensor...\r\n");

	if(oilState == oilIncrease)
	{
		oil += (rand() % incVariance) * precision;
		incCount++;
		if(incCount == maxCount)
		{
			oilState = oilDecrease;
		}
	}
	else if (oilState == oilDecrease)
	{
		oil -= (rand() % decVariance)  * precision;
		incCount--;
		if (oil > 50000)
		{
			incCount = 0;
			oilState = steady;
			oil = steadyMean;
		}
	}
	else if (oilState == steady)
	{
		//10% chance of high oil level, 0 (significantly higher than actual chance to show how a leak is detected)
		highOilChance = (rand() % 10);
		//%50 chance high oil is lower or higher than mean
		if (!highOilChance)
		{
			oil = (highMean + ((rand() % highVariance) * precision));
			oilState = oilIncrease;
		}
		else if (highOilChance % 2)
		{
			oil = (steadyMean + ((rand() % steadyVariance) * precision));
		}
		else
		{
			oil = (steadyMean - ((rand() % steadyVariance) * precision));
		}
	}
//	sprintf(buffer, "oil raw: %u\r\n", oil);
//	print_str(buffer);
	send_sensorData_message(Oil, oil);
}
