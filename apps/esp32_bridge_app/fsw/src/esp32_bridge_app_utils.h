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
 *   This file contains the prototypes for the Sample App utility functions
 */

#ifndef ESP32_BRIDGE_APP_UTILS_H
#define ESP32_BRIDGE_APP_UTILS_H

/*
** Required header files.
*/
#include "esp32_bridge_app.h"

CFE_Status_t ESP32_BRIDGE_APP_TblValidationFunc(void *TblData);
void         ESP32_BRIDGE_APP_GetCrc(const char *TableName);

/** Minimum CCSDS payload length (bytes) carrying temperature + humidity */
#define ESP32_BRIDGE_APP_SENSOR_PKT_MIN_LEN 14

/**
 * Decode temperature and humidity from a raw CCSDS packet received from the ESP32.
 *
 * Layout (big-endian): [0-5] primary hdr, [6-9] time, [10-11] temp_C*100 (int16),
 * [12-13] humidity*100 (uint16).
 *
 * \returns CFE_SUCCESS, CFE_STATUS_WRONG_MSG_LENGTH if Len is too short,
 *          or CFE_ES_BAD_ARGUMENT on a NULL pointer
 */
CFE_Status_t ESP32_BRIDGE_APP_ParseSensorPacket(const uint8 *Buf, size_t Len, float *TempC, float *HumPct);

#endif /* ESP32_BRIDGE_APP_UTILS_H */