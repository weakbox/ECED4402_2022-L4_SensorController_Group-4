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
	print_str("Sensor Controller Task\r\n");
	const TickType_t TimerDefaultPeriod = 1000;

	request_sensor_read();  // requests a usart read (through the callback)

	struct CommMessage currentRxMessage = {0};

	do {
		// Receive command from host PC
		switch (parse_hostPC_message())
		{
		case PC_Command_START:
			print_str("Command received from Host PC: START\r\n");
			send_sensorEnable_message(Acoustic, TimerDefaultPeriod);
			send_sensorEnable_message(Depth, TimerDefaultPeriod);
			break;
		case PC_Command_RESET:
			print_str("Command received from Host PC: RESET\r\n");
			send_sensorReset_message();
			break;
		default:
			print_str("No Command Recieved.\r\n");
			break;
		}

		// Receive data from Sensor Platform
		parse_sensor_message(&currentRxMessage);
		if(currentRxMessage.IsMessageReady == true && currentRxMessage.IsCheckSumValid == true)
		{
			switch (currentRxMessage.SensorID)
			{
			case Controller:
				print_str("Controller Answered!\r\n");
				break;
			case Acoustic:
				print_str("Acoustic Sensor Answered!\r\n");
				break;
			case Depth:
				print_str("Depth Sensor Answered!\r\n");
				break;
			default:
				print_str("Garbage Data!\r\n");
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
