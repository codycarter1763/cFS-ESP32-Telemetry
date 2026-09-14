#ifndef STM32_SENSOR_INTERFACE_H
#define STM32_SENSOR_INTERFACE_H

#include "cfe_msg_hdr.h"

#define STM32_BRIDGE_APP_SENSOR_DATA_MID 0x895

typedef struct
{
    float Temperature;
    float Humidity;
    uint32 SequenceCount;
    uint32 Status;
} STM32_BRIDGE_APP_SensorData_Payload_t;

typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    STM32_BRIDGE_APP_SensorData_Payload_t Payload;
} STM32_BRIDGE_APP_SensorData_t;

#endif
