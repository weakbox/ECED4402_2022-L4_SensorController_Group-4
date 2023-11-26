/*
 * HydraulicSensorController.c
 *
 *  Created on: Oct. 21, 2022
 *      Author: Andre Hendricks / Dr. JF Bousquet
 */

#include <stdbool.h>

#include "User/L2/Comm_Datalink.h"
#include "User/L3/HydraulicSensor.h"

//Required FreeRTOS header files
#include "FreeRTOS.h"
#include "Timers.h"


/******************************************************************************
This is a software callback function.
******************************************************************************/
void RunHydraulicSensor(TimerHandle_t xTimer)
{
	static uint16_t hydraulic = 10;
	static uint8_t down = true;
	if(down)
		hydraulic += 10;
	else
		hydraulic -=10;

	if(hydraulic == 300)
		down = false;
	if(hydraulic== 0)
		down = true;

	send_sensorData_message(Hydraulic, hydraulic);
}

