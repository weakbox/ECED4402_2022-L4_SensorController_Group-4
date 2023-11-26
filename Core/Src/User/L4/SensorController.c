/*
 * SensorController.c
 *
 *  Created on: Oct 24, 2022
 *      Author: kadh1 / Connor McLeod / Evan Lowe
 */


#include <stdio.h>

#include "main.h"
#include "User/L2/Comm_Datalink.h"
#include "User/L4/SensorPlatform.h"
#include "User/L4/SensorController.h"
#include "User/util.h"

//Required FreeRTOS header files
#include "FreeRTOS.h"
#include "Timers.h"
#include "semphr.h"

#define STARTSTATE 0 //States of the controller
#define ENABLESTATE 1
#define PARSESTATE 2

QueueHandle_t Queue_Sensor_Data;
QueueHandle_t Queue_HostPC_Data;

TimerHandle_t xTimer;

int state = STARTSTATE; //Current state of controller
char* Sensor_Data_Buffer;
char SBL_ack = 0; //SBL sensor enabled
char Depth_ack = 0;
char Oil_ack = 0;

static void ResetMessageStruct(struct CommMessage* currentRxMessage){

	static const struct CommMessage EmptyMessage = {0};
	*currentRxMessage = EmptyMessage;
}

//Wait for all sensor acknowledgments
void ack_wait()
{
	struct CommMessage currentRxMessage = {0};
	// Receive data from Sensor Platform_RX_Task
	if (xQueueReceive(Queue_Sensor_Data, &currentRxMessage, 0) && (currentRxMessage.messageId == 1))
	{
		switch (currentRxMessage.SensorID)
		{
		case None:
			break;
		//Should not happen
		case Controller:
			break;
		case SBL:
			print_str("SBL sensor enabled!\r\n");
			SBL_ack = 1;
			break;
		case Depth:
			print_str("Depth sensor enabled!\r\n");
			Depth_ack = 1;
			break;
		case Oil:
			print_str("Oil sensor enabled!\r\n");
			Oil_ack = 1;
			break;
		}
		ResetMessageStruct(&currentRxMessage);
	}
	//Both sensors enabled, move to next state
	if (SBL_ack && Depth_ack && Oil_ack)
	{
		xTimerStop( xTimer, 0 ); //No need to wait any longer
		state = PARSESTATE;
	}
}

void disable_sensors()
{
	send_sensorReset_message();//Reset
	SBL_ack = 0; //Allow new acknowledgments to be received
	Depth_ack = 0;
	Oil_ack = 0;
}

/******************************************************************************
This task is created from the main.
******************************************************************************/
void SensorControllerTask(void *params)
{
	print_str("Sensor Controller Task\r\n");
	const TickType_t TimerDefaultPeriod = 1000;
	struct CommMessage currentRxMessage = {0};

	int HostPC_Command = 0;
	char buffer[64];
	xTimer = xTimerCreate("Timer1",100,pdTRUE,( void * ) 0, ack_wait);

	do {
		switch (state)
		{
		case STARTSTATE:
			// Receive command from host PC and check if START command is entered
			if ((xQueueReceive(Queue_HostPC_Data, &HostPC_Command, 0)) && (HostPC_Command == PC_Command_START))
			{
				print_str("Command received from Host PC: START\r\n");
				state = ENABLESTATE;
				send_sensorEnable_message(SBL, TimerDefaultPeriod);
				send_sensorEnable_message(Depth, TimerDefaultPeriod);
				send_sensorEnable_message(Oil, TimerDefaultPeriod);
				xTimerStart( xTimer, 0 ); //Start timer to check if sensors are enabled
			}
			break;
		case ENABLESTATE:
			print_str("Enabled check state\r\n");
			break;
		case PARSESTATE:
			//Reset sensors
			if ((xQueueReceive(Queue_HostPC_Data, &HostPC_Command, 0)) && (HostPC_Command == PC_Command_RESET))
			{
				print_str("Command received from Host PC: RESET\r\n");
				disable_sensors();
				state = STARTSTATE;
				break;
			}
			// Receive data from Sensor Platform_RX_Task
			if (xQueueReceive(Queue_Sensor_Data, &currentRxMessage, 0))
			{
				switch (currentRxMessage.SensorID)
				{
				case None:
					break;
				case Controller:
					if (currentRxMessage.messageId == 1)
					{
						print_str("Sensor disabled!\r\n");
					}
					break;
				case SBL:
					if (currentRxMessage.messageId == 3)
					{
						//Only reads the first SBL so far
						sprintf(buffer, "SBL Sensor Reading: %d\r\n", currentRxMessage.params);
						print_str(buffer);
					}
					break;
				case Depth:
					if (currentRxMessage.messageId == 3)
					{
						sprintf(buffer, "Depth Sensor Reading: %d\r\n", currentRxMessage.params);
						print_str(buffer);
					}
					break;
				case Oil:
					if (currentRxMessage.messageId == 3)
					{
						sprintf(buffer, "Oil Sensor Reading: %d\r\n", currentRxMessage.params);
						print_str(buffer);
					}
					break;
				}
				ResetMessageStruct(&currentRxMessage);
			}
		}
		//Reset so new command can be received
		HostPC_Command = 0;
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
