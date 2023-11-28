/*
 * SensorController.c
 *
 *  Created on: Oct 24, 2022
 *      Author: kadh1 / Connor McLeod / Evan Lowe
 */


#include <stdio.h>
#include <stdlib.h>

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

TimerHandle_t xTimer[1];

int state = STARTSTATE; //Current state of controller
char* Sensor_Data_Buffer;
char SBL_ack = 0; //SBL1 sensor enabled
char SBL2_ack = 0; //SBL2 sensor enabled
char SBL3_ack = 0; //SBL3 sensor enabled
char Depth_ack = 0;
char Oil_ack = 0;

// Carries raw data from the sensors to the process task.
typedef struct DataPacket
{
	int sensor_type;
	int data;
}	DataPacket_t;

typedef struct Location
{
	int x;
	int y;
	int z;
	uint8_t locationTrue; //Set when the location has been calculated
};

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
		xTimerStop( xTimer[0], 0 ); //No need to wait any longer
		state = PARSESTATE;
	}
}

struct Location calculateLocation(uint32_t sbl_1, uint32_t sbl_2, uint32_t sbl_3, uint32_t depth)
{
	//Assume oil rig is a 100m x 100m square
	struct Location sbl_1_pos;
	struct Location sbl_2_pos;
	struct Location sbl_3_pos;
	struct Location Loc;

	sbl_1_pos.x = -50;
	sbl_1_pos.y = 0;
	sbl_2_pos.x = 50;
	sbl_2_pos.y = 50;
	sbl_3_pos.x = 50;
	sbl_3_pos.y = -50;
	float sbl_1_dist;
	float sbl_2_dist;
	float sbl_3_dist;

	unsigned char variance = 50;
	unsigned char precision = 10;

	uint8_t surfacePressure = 1; //Atmosphere
	uint16_t initialSpeed = 1450; //m/s speed at water surface
	float avgSpeed;

	avgSpeed = (depth * 1.677 * 10 + surfacePressure) + initialSpeed;
	sbl_1_dist = avgSpeed*sbl_1/1000000; //Convert microsecond to second. Calculate meters
	sbl_2_dist = avgSpeed*sbl_2/1000000;
	sbl_3_dist = avgSpeed*sbl_3/1000000;

	Loc.z = depth;
	if ((sbl_1 <= sbl_2) && (sbl_1 <= sbl_3))
	{
		Loc.x = ((rand() % variance)/precision) + sbl_1_pos.x;
		Loc.y = ((rand() % variance)/precision) + sbl_1_pos.y;
	}
	else if ((sbl_2 <= sbl_1) && (sbl_2 <= sbl_3))
	{
		Loc.x = ((rand() % variance)/precision) + sbl_2_pos.x;
		Loc.y = ((rand() % variance)/precision) + sbl_2_pos.y;
	}
	else if ((sbl_3 <= sbl_1) && (sbl_3 <= sbl_1))
	{
		Loc.x = ((rand() % variance)/precision) + sbl_3_pos.x;
		Loc.y = ((rand() % variance)/precision) + sbl_3_pos.y;
	}
	Loc.locationTrue = true;
	return Loc;
}

struct Location process(DataPacket_t packet)
{
	static uint32_t sbl_1;
	static uint32_t sbl_2;
	static uint32_t sbl_3;
	static uint32_t depth;
	static uint8_t sbl_1_rec = false;
	static uint8_t sbl_2_rec = false;
	static uint8_t sbl_3_rec = false;
	static uint8_t depth_rec = false;

	struct Location Loc;

	switch (packet.sensor_type)
	{
	case (SBL1):
		sbl_1 = packet.data;
		sbl_1_rec = true;
		break;
	case (SBL2):
		sbl_2 = packet.data;
		sbl_2_rec = true;
		break;
	case (SBL3):
		sbl_3 = packet.data;
		sbl_3_rec = true;
		break;
	case (Depth):
		depth = packet.data;
		depth_rec = true;
		break;
	}

	if (sbl_1_rec && sbl_2_rec && sbl_3_rec && depth_rec)
	{
		Loc = calculateLocation(sbl_1, sbl_2, sbl_3, depth);
		sbl_1_rec = false;
		sbl_2_rec = false;
		sbl_3_rec = false;
		depth_rec = false;
	}
	else
	{
		Loc.locationTrue = false;
	}
	return Loc;
}


void disable_sensors()
{
	send_sensorReset_message();//Reset
	SBL_ack = 0; //Allow new acknowledgments to be received
	SBL2_ack = 0;
	SBL3_ack = 0;
	Depth_ack = 0;
	Oil_ack = 0;
}

/**
 * Encodes sensor data based on the sensor type, allowing a maximum data output of 16383.
 *
 * The function encodes sensor data, considering the following sensor types:
* -011: SBL1 Sensor
* -100 : SBL2 Sensor
* -101 : SBL3 Sensor
* -110 : Depth Sensor
* -100 : Oil Sensor
 *
 * Returns the encoded 16-bit value where the first two bits represent the sensor type,
 * and the remaining bits represent the sensor data. Returns 0 in case of invalid
 * sensor type or data outside the valid range, serving as an indicator of an error.
 */
uint16_t compress_data(int sensor_type, int data)
{
	char dec_buffer[100];
	uint32_t comp = 0;
	if (data <= 16383)
	{
		if (sensor_type)
		{
			comp = (sensor_type) << 13;
			comp |= data;
		}
	}
	switch (sensor_type)
	{
	case SBL1:
		sprintf(dec_buffer, "Compressed SBL1 Sensor Reading: %u\r\n", comp);
		break;
	case SBL2:
		sprintf(dec_buffer, "Compressed SBL2 Sensor Reading: %u\r\n", comp);
		break;
	case SBL3:
		sprintf(dec_buffer, "Compressed SBL3 Sensor Reading: %u\r\n", comp);
		break;
	case Depth:
		sprintf(dec_buffer, "Compressed Depth Sensor Reading: %u\r\n", comp);
		break;
	case Oil:
		sprintf(dec_buffer, "Compressed Oil Sensor Reading: %u\r\n", comp);
		break;
	default:
		sprintf(dec_buffer, "Error compressing data. Was input too large?\r\n");
		break;
	}
	print_str(dec_buffer);
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
	char dec_buffer[100];
	int sensor_type = (input >> 13);
	int data = input & 0x1FFF;

	switch (sensor_type)
		{
		case SBL1:
			sprintf(dec_buffer, "Decompressed SBL1 Sensor Reading: %d\r\n", data);
			break;
		case SBL2:
			sprintf(dec_buffer, "Decompressed SBL2 Sensor Reading: %d\r\n", data);
			break;
		case SBL3:
			sprintf(dec_buffer, "Decompressed SBL3 Sensor Reading: %d\r\n", data);
			break;
		case Depth:
			sprintf(dec_buffer, "Decompressed Depth Sensor Reading: %d\r\n", data);
			break;
		case Oil:
			sprintf(dec_buffer, "Decompressed Oil Sensor Reading: %d\r\n", data);
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
void ProcessSendDataTask(void* params)
{
	print_str("Data Processing and Sending Task\r\n");
	char dec_buffer[100];
	DataPacket_t packet;
	Queue_Process_Data = xQueueCreate(16, sizeof(DataPacket_t));

	struct Location Loc;
	Loc.locationTrue = false;

	do {

		if (xQueueReceive(Queue_Process_Data, &packet, 0))
		{
			switch (packet.sensor_type)
				{
			//Process location data from SBL and Depth only
				case SBL1:
				case SBL2:
				case SBL3:
				case Depth:
					Loc = process(packet);
					if (Loc.locationTrue)
					{
						sprintf(dec_buffer, "Location: x:%d, y:%d, z:%d\r\n", Loc.x, Loc.y, Loc.z);
						print_str(dec_buffer);
					}
					break;
				case Oil:
					sprintf(dec_buffer, "Oil Sensor Reading: %d\r\n", packet.data);
					print_str(dec_buffer);
					break;
				default:
					sprintf(dec_buffer, "Error\r\n");
					break;
				}

/*			switch (packet.sensor_type)
				{
				case SBL1:
					sprintf(dec_buffer, "SBL1 Sensor Reading: %d\r\n", packet.data);
					break;
				case SBL2:
					sprintf(dec_buffer, "SBL2 Sensor Reading: %d\r\n", packet.data);
					break;
				case SBL3:
					sprintf(dec_buffer, "SBL3 Sensor Reading: %d\r\n", packet.data);
					break;
				case Depth:
					sprintf(dec_buffer, "Depth Sensor Reading: %d\r\n", packet.data);
					break;
				case Oil:
					sprintf(dec_buffer, "Oil Sensor Reading: %d\r\n", packet.data);
					break;
				default:
					sprintf(dec_buffer, "Error decompressing data. Was input too large?\r\n");
					break;
				}*/

				//decompress_data(compress_data(packet.sensor_type, packet.data));
		}
	vTaskDelay(10 / portTICK_RATE_MS);
	} while (1);
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
	xTimer[0] = xTimerCreate("Timer1",100,pdTRUE,( void * ) 0, ack_wait);

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
				vTaskDelay(100 / portTICK_RATE_MS);
				send_sensorEnable_message(Depth, TimerDefaultPeriod);
				vTaskDelay(100 / portTICK_RATE_MS);
				send_sensorEnable_message(Oil, TimerDefaultPeriod);
				xTimerStart( xTimer[0], 0 ); //Start timer to check if sensors are enabled
			}
			break;
		case ENABLESTATE:
			//print_str("Enabled check state\r\n");
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
			for (int i = 0; i < 5; i++)
			{
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
					case SBL1:
						if (currentRxMessage.messageId == 3)
						{
							//Only reads the first SBL so far
							packet.sensor_type = currentRxMessage.SensorID;
							packet.data = currentRxMessage.params;
							xQueueSendToBack(Queue_Process_Data, &packet, 0);
						}
						break;
					case SBL2:
						if (currentRxMessage.messageId == 3)
						{
							packet.sensor_type = currentRxMessage.SensorID;
							packet.data = currentRxMessage.params;
							xQueueSendToBack(Queue_Process_Data, &packet, 0);
						}
						break;
					case SBL3:
						if (currentRxMessage.messageId == 3)
						{
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
		}
		//Reset so new command can be received
		HostPC_Command = 0;
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
