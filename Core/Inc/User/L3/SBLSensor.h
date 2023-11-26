/*
 * GPSSensor.h
 *
 *  Created on: Nov 25, 2023
 *      Author: evanl
 */

#ifndef INC_USER_L3_SBLSENSOR_H_
#define INC_USER_L3_SBLSENSOR_H_

#include "FreeRTOS.h"
#include "timers.h"

void RunSBLSensor(TimerHandle_t xTimer);

#endif /* INC_USER_L3_SBLSENSOR_H_ */
