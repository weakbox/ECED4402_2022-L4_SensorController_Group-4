/*
 * DepthSensor.h
 *
 *  Created on: Oct 22, 2022
 *      Author: kadh1
 */

#ifndef INC_USER_L3_HYDRAULICSENSOR_H_
#define INC_USER_L3_HYDRAULICSENSOR_H_

#include "FreeRTOS.h"
#include "timers.h"

void RunHydraulicSensor(TimerHandle_t xTimer);

#endif /* INC_USER_L3_HYDRAULICSENSOR_H_ */
