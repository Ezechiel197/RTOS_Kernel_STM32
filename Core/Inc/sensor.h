/*
 * sensor.h
 *
 *  Created on: Jun 28, 2026
 *      Author: ezechiel
 */

#ifndef INC_SENSOR_H_
#define INC_SENSOR_H_

typedef enum {
	Okay,
	I2C_Error,
	Sensor_Timeout
}SensorStatus_t;
typedef struct{
	float temperature;
	float humidity;
	SensorStatus_t status;
	uint32_t hardwareError;  // Speichert den EXAKTEN HAL-Fehlercode
}SensorMsg_t;

#endif /* INC_SENSOR_H_ */
