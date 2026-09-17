/************************************************************************
 * NASA Docket No. GSC-19,200-1, and identified as "cFS Draco"
 *
 * Copyright (c) 2023 United States Government as represented by the
 * Administrator of the National Aeronautics and Space Administration.
 * All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ************************************************************************/

/**
 * @file
 *
 * Main header file for the Sample application
 */

#ifndef ESP32_BRIDGE_APP_H
#define ESP32_BRIDGE_APP_H

/*
** Required header files.
*/
#include "cfe.h"
#include "cfe_config.h"

#include "esp32_bridge_app_mission_cfg.h"
#include "esp32_bridge_app_platform_cfg.h"

#include "esp32_bridge_app_perfids.h"
#include "esp32_bridge_app_msgids.h"
#include "esp32_bridge_app_msg.h"

#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>

/************************************************************************
** Type Definitions
*************************************************************************/

/*
** Global Data
*/
typedef struct
{
    /*
    ** Command interface counters...
    */
    uint8 CommandCounter;
    uint8 CommandErrorCounter;

    /*
    ** Housekeeping telemetry packet...
    */
    ESP32_BRIDGE_APP_HkTlm_t HkTlm;

    /*
    ** Sensor data telemetry packet...
    */
    ESP32_BRIDGE_APP_SensorData_t SensorData;

    /*
    ** Run Status variable used in the main processing loop
    */
    uint32 RunStatus;

    /*
    ** Operational data (not reported in housekeeping)...
    */
    CFE_SB_PipeId_t CommandPipe;

    /*
    ** Serial port file descriptor for ESP32 sensor data
    */
    int SerialFd;

    CFE_TBL_Handle_t TblHandles[ESP32_BRIDGE_APP_PLATFORM_NUMBER_OF_TABLES];
} ESP32_BRIDGE_APP_Data_t;

/*
** Global data structure
*/
extern ESP32_BRIDGE_APP_Data_t ESP32_BRIDGE_APP_Data;

/****************************************************************************/
/*
** Local function prototypes.
**
** Note: Except for the entry point (ESP32Bridge_Main), these
**       functions are not called from any other source module.
*/
void         ESP32Bridge_Main(void);
CFE_Status_t ESP32_BRIDGE_APP_Init(void);

#endif /* ESP32_BRIDGE_APP_H */
