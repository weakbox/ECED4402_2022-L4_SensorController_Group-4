/*
 * remoteSensingPlatform.c
 *
 *  Created on: Oct. 21, 2022
 *      Author: Andre Hendricks / Dr. JF Bousquet / Evan Lowe
 */
#include <stdio.h>

#include "User/L2/Comm_Datalink.h"
#include "User/L3/SBLSensor.h"
#include "User/L3/DepthSensor.h"
#include "User/L3/OilSensor.h"
#include "User/L4/SensorPlatform.h"
#include "User/util.h"

//Required FreeRTOS header files
#include "FreeRTOS.h"
#include "Timers.h"
#include "semphr.h"

static void ResetMessageStruct(struct CommMessage* currentRxMessage){

	static const struct CommMessage EmptyMessage = {0};
	*currentRxMessage = EmptyMessage;
}

/******************************************************************************
This task is created from the main.
It is responsible for managing the messages from the datalink.
It is also responsible for starting the timers for each sensor
******************************************************************************/
void SensorPlatformTask(void *params)
{
	const TickType_t SBLTimerPeriod = 2000; //Needs to be slower than other sensors
	const TickType_t DepthTimerPeriod = 2000;
	const TickType_t OilTimerPeriod = 1000;
	TimerHandle_t TimerID_SBLSensor, TimerID_DepthSensor, TimerID_OilSensor;

	print_str("Sensor Platform Task\r\n");

	TimerID_SBLSensor = xTimerCreate(
		"SBL Sensor Task",
		SBLTimerPeriod,		// Period: Needed to be changed based on parameter
		pdTRUE,		// Autoreload: Continue running till deleted or stopped
		(void*)1,
		RunSBLSensor
		);

	TimerID_DepthSensor = xTimerCreate(
		"Depth Sensor Task",
		DepthTimerPeriod,		// Period: Needed to be changed based on parameter
		pdTRUE,		// Autoreload: Continue running till deleted or stopped
		(void*)0,
		RunDepthSensor
		);

	TimerID_OilSensor = xTimerCreate(
		"Oil Sensor Task",
		OilTimerPeriod,		// Period: Needed to be changed based on parameter
		pdTRUE,		// Autoreload: Continue running till deleted or stopped
		(void*)1,
		RunOilSensor
		);

	request_sensor_read();  // requests a usart read (through the callback)

	struct CommMessage currentRxMessage = {0};

	do {

		parse_sensor_message(&currentRxMessage);

		if(currentRxMessage.IsMessageReady == true && currentRxMessage.IsCheckSumValid == true){
			switch(currentRxMessage.SensorID){
				case Controller:
					switch(currentRxMessage.messageId){
						case 0:
							xTimerStop(TimerID_DepthSensor, portMAX_DELAY);
							xTimerStop(TimerID_SBLSensor, portMAX_DELAY);
							xTimerStop(TimerID_OilSensor, portMAX_DELAY);
							send_ack_message(RemoteSensingPlatformReset);
							break;
						case 1: //Do Nothing
							break;
						case 3: //Do Nothing
							break;
						}
					break;
				case SBL:
					switch(currentRxMessage.messageId){
						case 0:
							//Timer will only be started for one SBL station because all three are produced by the same function
							xTimerChangePeriod(TimerID_SBLSensor, currentRxMessage.params, portMAX_DELAY);
							xTimerStart(TimerID_SBLSensor, portMAX_DELAY);
							send_ack_message(SBLSensorEnable);
							break;
						case 1: //Do Nothing
							break;
						case 3: //Do Nothing
							break;
					}
					break;
				case Depth:
					switch(currentRxMessage.messageId){
						case 0:
							xTimerChangePeriod(TimerID_DepthSensor, currentRxMessage.params, portMAX_DELAY);
							xTimerStart(TimerID_DepthSensor, portMAX_DELAY);
							send_ack_message(DepthSensorEnable);
							break;
						case 1: //Do Nothing
							break;
						case 3: //Do Nothing
							break;
					}
					break;
				case Oil:
					switch(currentRxMessage.messageId){
						case 0:
							xTimerChangePeriod(TimerID_OilSensor, currentRxMessage.params, portMAX_DELAY);
							xTimerStart(TimerID_OilSensor, portMAX_DELAY);
							send_ack_message(OilSensorEnable);
							break;
						case 1: //Do Nothing
							break;
						case 3: //Do Nothing
							break;
					}
					break;
				default://Should not get here
					break;
			}
			ResetMessageStruct(&currentRxMessage);
		}
	} while(1);
}
