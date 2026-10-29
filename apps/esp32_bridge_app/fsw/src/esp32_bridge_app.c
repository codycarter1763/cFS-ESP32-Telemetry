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
 * \file
 *   ESP32 Bridge App - reads CCSDS packets from ESP32 via serial
 *   and publishes sensor data to the cFS Software Bus.
 */
#define _GNU_SOURCE
#include "esp32_bridge_app.h"
#include "esp32_bridge_app_cmds.h"
#include "esp32_bridge_app_utils.h"
#include "esp32_bridge_app_serial.h"
#include "esp32_bridge_app_eventids.h"
#include "esp32_bridge_app_dispatch.h"
#include "esp32_bridge_app_tbl.h"
#include "esp32_bridge_app_version.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>


/*
** Global data
*/
ESP32_BRIDGE_APP_Data_t ESP32_BRIDGE_APP_Data;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                            */
/* Application entry point and main process loop                              */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
void ESP32Bridge_Main(void)
{
    CFE_Status_t status;
    uint8        sync_byte;
    uint16       pkt_len;
    uint8        buf[64];
    ssize_t      read_result;
    int          packet_count = 0;
    const uint8  SYNC_BYTES[2] = {0xAA, 0x55};
    uint8        sync_window[2] = {0, 0};
    float        temp_c  = 0.0f;
    float        hum_pct = 0.0f;

    CFE_ES_PerfLogEntry(ESP32_BRIDGE_APP_PERF_ID);

    status = ESP32_BRIDGE_APP_Init();
    if (status != CFE_SUCCESS)
    {
        ESP32_BRIDGE_APP_Data.RunStatus = CFE_ES_RunStatus_APP_ERROR;
    }

    while (CFE_ES_RunLoop(&ESP32_BRIDGE_APP_Data.RunStatus) == true)
    {
        CFE_ES_PerfLogExit(ESP32_BRIDGE_APP_PERF_ID);

        if (ESP32_BRIDGE_APP_Data.SerialFd < 0)
        {
            OS_TaskDelay(2000);
            ESP32_BRIDGE_APP_Data.SerialFd = ESP32_BRIDGE_APP_OpenSerial(
                ESP32_BRIDGE_APP_PLATFORM_SERIAL_PORT,
                ESP32_BRIDGE_APP_PLATFORM_SERIAL_BAUD);
            if (ESP32_BRIDGE_APP_Data.SerialFd >= 0)
            {
                printf("[ESP32_BRIDGE_APP] Serial port opened successfully: fd=%d\n",
                       ESP32_BRIDGE_APP_Data.SerialFd);
            }
            CFE_ES_PerfLogEntry(ESP32_BRIDGE_APP_PERF_ID);
            continue;
        }

        /* Scan one byte at a time for the 2-byte sync marker before trusting anything */
        read_result = ESP32_BRIDGE_APP_ReadSerial(ESP32_BRIDGE_APP_Data.SerialFd, &sync_byte, 1);
        if (read_result == 0 || (read_result < 0 && (errno == ENODEV || errno == EIO || errno == ENXIO)))
        {
        /* Device genuinely disappeared -- either a clean EOF/hangup (read_result == 0,
        the common case for a USB-serial adapter physically unplugged) or a specific
        device error, not just "no data yet" */
            printf("[ESP32_BRIDGE_APP] Serial device lost: read_result=%d errno=%d (%s)\n",
            (int)read_result, errno, strerror(errno));
            ESP32_BRIDGE_APP_CloseSerial(ESP32_BRIDGE_APP_Data.SerialFd);
            ESP32_BRIDGE_APP_Data.SerialFd = -1;

            CFE_EVS_SendEvent(ESP32_BRIDGE_APP_SERIAL_LOST_ERR_EID,
                      CFE_EVS_EventType_ERROR,
                      "ESP32_BRIDGE_APP: Serial connection lost, read_result=%d errno=%d",
                      (int)read_result, errno);

            CFE_ES_PerfLogEntry(ESP32_BRIDGE_APP_PERF_ID);
                continue;
        }
        if (read_result != 1)
        {
            CFE_ES_PerfLogEntry(ESP32_BRIDGE_APP_PERF_ID);
            continue;
        }

        sync_window[0] = sync_window[1];
        sync_window[1] = sync_byte;
        if (sync_window[0] != SYNC_BYTES[0] || sync_window[1] != SYNC_BYTES[1])
        {
            /* Not synced yet -- keep scanning */
            CFE_ES_PerfLogEntry(ESP32_BRIDGE_APP_PERF_ID);
            continue;
        }

        /* Read 2-byte length prefix -- native little-endian, NOT byte-swapped */
        read_result = ESP32_BRIDGE_APP_ReadSerial(ESP32_BRIDGE_APP_Data.SerialFd, (uint8 *)&pkt_len, 2);
        if (read_result != 2)
        {
            CFE_ES_PerfLogEntry(ESP32_BRIDGE_APP_PERF_ID);
            continue;
        }

        /* Validate the length before trusting it: too large would let read() write
           past the end of buf, too small can't hold a sensor packet (and 0 would
           "succeed" as a 0-byte read). Either way it's a corrupted length or a
           false sync match in line noise, so drop it and resync. */
        if (pkt_len < ESP32_BRIDGE_APP_SENSOR_PKT_MIN_LEN || pkt_len > sizeof(buf))
        {
            printf("[ESP32_BRIDGE_APP] Bad packet length %u (valid %u-%u), resyncing\n",
                   pkt_len, (unsigned)ESP32_BRIDGE_APP_SENSOR_PKT_MIN_LEN, (unsigned)sizeof(buf));
            sync_window[0] = 0;
            sync_window[1] = 0;
            CFE_ES_PerfLogEntry(ESP32_BRIDGE_APP_PERF_ID);
            continue;
        }

        read_result = ESP32_BRIDGE_APP_ReadSerial(ESP32_BRIDGE_APP_Data.SerialFd, buf, pkt_len);
        if (read_result != (int)pkt_len)
        {
            printf("[ESP32_BRIDGE_APP] Short packet read: got %d, expected %u\n",
                   (int)read_result, pkt_len);
            CFE_ES_PerfLogEntry(ESP32_BRIDGE_APP_PERF_ID);
            continue;
        }

        /* Parse CCSDS packet payload -- ESP32 sends everything big-endian (hton16 applied) */
        if (ESP32_BRIDGE_APP_ParseSensorPacket(buf, pkt_len, &temp_c, &hum_pct) == CFE_SUCCESS)
        {
            packet_count++;

            ESP32_BRIDGE_APP_Data.SensorData.Payload.Temperature   = temp_c;
            ESP32_BRIDGE_APP_Data.SensorData.Payload.Humidity      = hum_pct;
            ESP32_BRIDGE_APP_Data.SensorData.Payload.SequenceCount = packet_count;
            ESP32_BRIDGE_APP_Data.SensorData.Payload.Status        = 0;

            CFE_SB_TimeStampMsg(CFE_MSG_PTR(ESP32_BRIDGE_APP_Data.SensorData.TelemetryHeader));
            CFE_SB_TransmitMsg(CFE_MSG_PTR(ESP32_BRIDGE_APP_Data.SensorData.TelemetryHeader), true);

            CFE_EVS_SendEvent(ESP32_BRIDGE_APP_INIT_INF_EID,
                              CFE_EVS_EventType_INFORMATION,
                              "ESP32_BRIDGE_APP: Seq=%d Temp=%.1fC Humidity=%.1f%%",
                              packet_count, temp_c, hum_pct);
        }
        else
        {
            printf("[ESP32_BRIDGE_APP] Packet too short: %u bytes (need >= %u)\n", pkt_len,
                   (unsigned)ESP32_BRIDGE_APP_SENSOR_PKT_MIN_LEN);
        }

        OS_TaskDelay(2000);

        CFE_ES_PerfLogEntry(ESP32_BRIDGE_APP_PERF_ID);
    }

    CFE_ES_PerfLogExit(ESP32_BRIDGE_APP_PERF_ID);
    CFE_ES_ExitApp(ESP32_BRIDGE_APP_Data.RunStatus);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                            */
/* Initialization                                                             */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
CFE_Status_t ESP32_BRIDGE_APP_Init(void)
{
    CFE_Status_t status;
    char         VersionString[ESP32_BRIDGE_APP_CFG_MAX_VERSION_STR_LEN];

    printf("[ESP32_BRIDGE_APP] Initialization starting...\n");

    memset(&ESP32_BRIDGE_APP_Data, 0, sizeof(ESP32_BRIDGE_APP_Data));
    ESP32_BRIDGE_APP_Data.RunStatus = CFE_ES_RunStatus_APP_RUN;
    ESP32_BRIDGE_APP_Data.SerialFd  = -1;

    /* Register events */
    status = CFE_EVS_Register(NULL, 0, CFE_EVS_EventFilter_BINARY);
    if (status != CFE_SUCCESS)
    {
        CFE_ES_WriteToSysLog("ESP32_BRIDGE_APP: Error Registering Events, RC = 0x%08lX\n",
                             (unsigned long)status);
        return status;
    }

    /* Initialize sensor data telemetry packet */
    CFE_MSG_Init(CFE_MSG_PTR(ESP32_BRIDGE_APP_Data.SensorData.TelemetryHeader),
                 CFE_SB_ValueToMsgId(ESP32_BRIDGE_APP_SENSOR_DATA_MID),
                 sizeof(ESP32_BRIDGE_APP_Data.SensorData));

    printf("[ESP32_BRIDGE_APP] Sensor data message initialized (MID=0x%X, size=%lu)\n",
           ESP32_BRIDGE_APP_SENSOR_DATA_MID,
           sizeof(ESP32_BRIDGE_APP_Data.SensorData));

    /* Initialize housekeeping packet */
    CFE_MSG_Init(CFE_MSG_PTR(ESP32_BRIDGE_APP_Data.HkTlm.TelemetryHeader),
                 CFE_SB_ValueToMsgId(ESP32_BRIDGE_APP_HK_TLM_MID),
                 sizeof(ESP32_BRIDGE_APP_Data.HkTlm));

    /* Open serial port */
    printf("[ESP32_BRIDGE_APP] Attempting to open serial port: %s @ %u baud\n",
           ESP32_BRIDGE_APP_PLATFORM_SERIAL_PORT,
           (unsigned)ESP32_BRIDGE_APP_PLATFORM_SERIAL_BAUD);

    ESP32_BRIDGE_APP_Data.SerialFd = ESP32_BRIDGE_APP_OpenSerial(
        ESP32_BRIDGE_APP_PLATFORM_SERIAL_PORT,
        ESP32_BRIDGE_APP_PLATFORM_SERIAL_BAUD);

    if (ESP32_BRIDGE_APP_Data.SerialFd < 0)
    {
        printf("[ESP32_BRIDGE_APP] WARNING: Serial port not available at init (will retry)\n");
        CFE_EVS_SendEvent(ESP32_BRIDGE_APP_CR_PIPE_ERR_EID,
                          CFE_EVS_EventType_INFORMATION,
                          "ESP32_BRIDGE_APP: Serial port %s not available (will retry)",
                          ESP32_BRIDGE_APP_PLATFORM_SERIAL_PORT);
    }
    else
    {
        printf("[ESP32_BRIDGE_APP] Serial port opened successfully\n");
    }

    CFE_Config_GetVersionString(VersionString,
                                ESP32_BRIDGE_APP_CFG_MAX_VERSION_STR_LEN,
                                "ESP32_BRIDGE_APP",
                                ESP32_BRIDGE_APP_VERSION,
                                ESP32_BRIDGE_APP_BUILD_CODENAME,
                                ESP32_BRIDGE_APP_LAST_OFFICIAL);

    printf("[ESP32_BRIDGE_APP] Initialization complete.%s\n", VersionString);

    CFE_EVS_SendEvent(ESP32_BRIDGE_APP_INIT_INF_EID,
                      CFE_EVS_EventType_INFORMATION,
                      "ESP32_BRIDGE_APP Initialized.%s",
                      VersionString);

    return CFE_SUCCESS;
}