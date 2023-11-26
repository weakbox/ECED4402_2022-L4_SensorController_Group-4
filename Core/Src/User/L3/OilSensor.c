/*
 * OilSensor.c
 *
 *  Created on: Nov 25, 2023
 *      Author: evanl
 */

#include <stdlib.h>

#include "User/L2/Comm_Datalink.h"
#include "User/L3/OilSensor.h"

//Required FreeRTOS header files
#include "FreeRTOS.h"
#include "Timers.h"


/******************************************************************************
This is a software callback function.
******************************************************************************/
void RunOilSensor(TimerHandle_t xTimer) //Default 1000 ms
{
	const uint16_t variance = 50;
	const uint16_t mean = 100;

	send_sensorData_message(Oil, (rand() % variance) + mean);
}
