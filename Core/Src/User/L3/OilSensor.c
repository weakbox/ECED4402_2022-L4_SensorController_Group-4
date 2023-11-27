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

#define precision 10 //Sensor is accurate to ten units
#define incVariance 5
//Oil will decrease at a faster rate on average because oil floats to surface
#define decVariance 20
#define steadyVariance 3
#define steadyMean 50
#define highVariance 50
#define highMean 1000

#define maxCount 5

enum oilStates {steady, oilIncrease, oilDecrease};

/******************************************************************************
This is a software callback function.
******************************************************************************/
void RunOilSensor(TimerHandle_t xTimer) //Default 1000 ms
{
	uint8_t highOilChance;
	static uint32_t oil = 1000;
	static enum oilStates oilState = oilDecrease; //Oil pools at surface so the start is highest concentration
	static uint8_t incCount = maxCount;

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
		if (oil <= 0)
		{
			incCount = 0;
			oilState = steady;
			oil = steadyMean;
		}
	}
	else if (oilState == steady)
	{
		//10% chance of high oil level, 0 or 1 (significantly higher than actual chance to show how a leak is detected)
		highOilChance = (rand() % 20);
		//%50 chance high oil is lower or higher than mean
		if (highOilChance == 1)
		{
			oil = (highMean + (rand() % highVariance)  * precision);
			oilState = oilIncrease;
		}
		else if (!highOilChance)
		{
			oil = (highMean - (rand() % highVariance)  * precision);
			oilState = oilIncrease;
		}
		else if (highOilChance % 2)
		{
			oil = (steadyMean + (rand() % steadyVariance)  * precision);
		}
		else
		{
			oil = (steadyMean - (rand() % steadyVariance)  * precision);
		}
	}

	send_sensorData_message(Oil, oil);
}
