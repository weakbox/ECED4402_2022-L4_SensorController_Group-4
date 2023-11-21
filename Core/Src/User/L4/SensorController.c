/*
 * SensorController.c
 *
 *  Created on: Oct 24, 2022
 *      Author: kadh1
 */


#include <stdio.h>

#include "main.h"
#include "User/L2/Comm_Datalink.h"
#include "User/L3/AcousticSensor.h"
#include "User/L3/DepthSensor.h"
#include "User/L4/SensorPlatform.h"
#include "User/L4/SensorController.h"
#include "User/util.h"

//Required FreeRTOS header files
#include "FreeRTOS.h"
#include "Timers.h"
#include "semphr.h"


QueueHandle_t Queue_Sensor_Data;
QueueHandle_t Queue_HostPC_Data;

static void ResetMessageStruct(struct CommMessage* currentRxMessage){

	static const struct CommMessage EmptyMessage = {0};
	*currentRxMessage = EmptyMessage;
}

/******************************************************************************
This task is created from the main.
******************************************************************************/
void SensorControllerTask(void *params)
{
	char message[100];
	do {
		printf("SensorController\r\n");
		request_sensor_read();  // requests a usart read (through the callback)

		struct CommMessage currentRxMessage = {0};

		parse_sensor_message(&currentRxMessage);

		if(currentRxMessage.IsMessageReady == true && currentRxMessage.IsCheckSumValid == true){

			switch(currentRxMessage.SensorID){
				case Controller:
					switch(currentRxMessage.messageId){
						case 0:
							xTimerStop(TimerID_DepthSensor, portMAX_DELAY);
							xTimerStop(TimerID_AcousticSensor, portMAX_DELAY);
							send_ack_message(RemoteSensingPlatformReset);
							break;
						case 1: //Do Nothing
							break;
						case 3: //Do Nothing
							break;
						}
					break;
				case Acoustic:
					switch(currentRxMessage.messageId){
						case 0:
							xTimerChangePeriod(TimerID_AcousticSensor, currentRxMessage.params, portMAX_DELAY);
							xTimerStart(TimerID_AcousticSensor, portMAX_DELAY);
							send_ack_message(AcousticSensorEnable);
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
					default://Should not get here
						break;
			}
			ResetMessageStruct(&currentRxMessage);
		}

		vTaskDelay(1000 / portTICK_RATE_MS);
	} while(1);
}

/*
 * This task reads the queue of characters from the Sensor Platform when available
 * It then sends the processed data to the Sensor Controller Task
 */
void SensorPlatform_RX_Task(){
	struct CommMessage currentRxMessage = {0};
	Queue_Sensor_Data = xQueueCreate(80, sizeof(struct CommMessage));

	request_sensor_read();  // requests a usart read (through the callback)

	while(1){
		parse_sensor_message(&currentRxMessage);

		if(currentRxMessage.IsMessageReady == true && currentRxMessage.IsCheckSumValid == true){

			xQueueSendToBack(Queue_Sensor_Data, &currentRxMessage, 0);
			ResetMessageStruct(&currentRxMessage);
		}
	}
}



/*
 * This task reads the queue of characters from the Host PC when available
 * It then sends the processed data to the Sensor Controller Task
 */
void HostPC_RX_Task(){

	enum HostPCCommands HostPCCommand = PC_Command_NONE;

	Queue_HostPC_Data = xQueueCreate(80, sizeof(enum HostPCCommands));

	request_hostPC_read();

	while(1){
		HostPCCommand = parse_hostPC_message();

		if (HostPCCommand != PC_Command_NONE){
			xQueueSendToBack(Queue_HostPC_Data, &HostPCCommand, 0);
		}

	}
}
