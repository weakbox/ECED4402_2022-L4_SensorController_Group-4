/*
 * OilSensor.h
 *
 *  Created on: Nov 25, 2023
 *      Author: evanl
 */

#ifndef INC_USER_L3_OILSENSOR_H_
#define INC_USER_L3_OILSENSOR_H_

#include "FreeRTOS.h"
#include "timers.h"

void RunOilSensor(TimerHandle_t xTimer);

#endif /* INC_USER_L3_DEPTHSENSOR_H_ */
