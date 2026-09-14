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
 *   DISPLAY_APP Application Topic IDs
 */
#ifndef DISPLAY_APP_TOPICIDS_H
#define DISPLAY_APP_TOPICIDS_H

#include "display_app_topicid_values.h"

#define DISPLAY_APP_MISSION_CMD_TOPICID             DISPLAY_APP_MISSION_TIDVAL(CMD)
#define DEFAULT_DISPLAY_APP_MISSION_CMD_TOPICID         0xA2
#define DISPLAY_APP_MISSION_SEND_HK_TOPICID         DISPLAY_APP_MISSION_TIDVAL(SEND_HK)
#define DEFAULT_DISPLAY_APP_MISSION_SEND_HK_TOPICID     0xA3
#define DISPLAY_APP_MISSION_HK_TLM_TOPICID          DISPLAY_APP_MISSION_TIDVAL(HK_TLM)
#define DEFAULT_DISPLAY_APP_MISSION_HK_TLM_TOPICID      0xA4
#define DISPLAY_APP_MISSION_SENSOR_DATA_TOPICID     DISPLAY_APP_MISSION_TIDVAL(SENSOR_DATA)
#define DEFAULT_DISPLAY_APP_MISSION_SENSOR_DATA_TOPICID  0x95  

#endif
