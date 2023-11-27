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
QueueHandle_t Queue_Process_Data;

TimerHandle_t xTimer;

int state = STARTSTATE; //Current state of controller
char* Sensor_Data_Buffer;
char SBL_ack = 0; //SBL sensor enabled
char Depth_ack = 0;
char Oil_ack = 0;

// Carries raw data from the sensors to the process task.
typedef struct DataPacket
{
	int sensor_type;
	int data;
}	DataPacket_t;

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

/**
 * Encodes sensor data based on the sensor type, allowing a maximum data output of 16383.
 *
 * The function encodes sensor data, considering the following sensor types:
 * - 01: SBL Sensor
 * - 10: Depth Sensor
 * - 11: Oil Sensor
 *
 * Returns the encoded 16-bit value where the first two bits represent the sensor type,
 * and the remaining bits represent the sensor data. Returns 0 in case of invalid
 * sensor type or data outside the valid range, serving as an indicator of an error.
 */
uint16_t compress_data(int sensor_type, int data)
{
	uint16_t comp = 0;
	if (data <= 16383)
	{
		if (sensor_type)
		{
			comp = (sensor_type - 1) << 14;
			comp |= data;
		}
	}
	return comp;
}

/**
* Decompresses the encoded data and prints the correct string depending on the sensor type.
*
* On a real system, the compressed data would be decompressed after transmission, but for
* our system, we are spoofing the transmission through the `print_str` function.
*/
void decompress_data(uint16_t input)
{
	char dec_buffer[64];
	int sensor_type = (input >> 14) + 1;
	int data = input & 0x3FFF;

	switch (sensor_type)
	{
	case 2:
		sprintf(dec_buffer, "SBL Sensor Reading: %d\r\n", data);
		break;
	case 3:
		sprintf(dec_buffer, "Depth Sensor Reading: %d\r\n", data);
		break;
	case 4:
		sprintf(dec_buffer, "Oil Sensor Reading: %d\r\n", data);
		break;
	default:
		sprintf(dec_buffer, "Error decompressing data. Was input too large?\r\n");
		break;
	}
	print_str(dec_buffer);
}

// Processes the data obtained by the sensors and transfers it to the Host PC.
// Note: The transfer to the Host PC in this situation is emulated via the use of the 'print_str' function.
// Note: To minimize data transfer, we want to compress this string to be as short as possible.
void ProcessSendDataTask(void *params)
{
	print_str("Data Processing and Sending Task\r\n");

	DataPacket_t packet;
	Queue_Process_Data = xQueueCreate(16, sizeof(DataPacket_t));

	do {
		if (xQueueReceive(Queue_Process_Data, &packet, 0))
		{
			// Process data before sending!
			decompress_data(compress_data(packet.sensor_type, packet.data));
		}
	} while(1);
}

/******************************************************************************
This task is created from the main.
******************************************************************************/
void SensorControllerTask(void *params)
{
	print_str("Sensor Controller Task\r\n");
	const TickType_t TimerDefaultPeriod = 1000;
	struct CommMessage currentRxMessage = {0};
	DataPacket_t packet;

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
						packet.sensor_type = currentRxMessage.SensorID;
						packet.data = currentRxMessage.params;
						xQueueSendToBack(Queue_Process_Data, &packet, 0);
					}
					break;
				case Depth:
					if (currentRxMessage.messageId == 3)
					{
						packet.sensor_type = currentRxMessage.SensorID;
						packet.data = currentRxMessage.params;
						xQueueSendToBack(Queue_Process_Data, &packet, 0);
					}
					break;
				case Oil:
					if (currentRxMessage.messageId == 3)
					{
						packet.sensor_type = currentRxMessage.SensorID;
						packet.data = currentRxMessage.params;
						xQueueSendToBack(Queue_Process_Data, &packet, 0);
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
