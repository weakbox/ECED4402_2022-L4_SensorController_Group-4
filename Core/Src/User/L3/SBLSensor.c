/*
 * SBLSensor.c
 *
 *  Created on: Nov 25, 2023
 *      Author: evanl
 */

#include <stdbool.h>
#include <User/L3/SBLSensor.h>

#include "User/L2/Comm_Datalink.h"
#include "FreeRTOS.h"
#include "Timers.h"


/******************************************************************************
This is a software callback function.
******************************************************************************/
void RunSBLSensor(TimerHandle_t xTimer)
{
	static uint16_t sbl = 10;
	static uint8_t down = true;
	if(down)
		sbl += 10;
	else
		sbl -=10;

	if(sbl == 300)
		down = false;
	if(sbl== 0)
		down = true;

	send_sensorData_message(SBL, sbl);
}

