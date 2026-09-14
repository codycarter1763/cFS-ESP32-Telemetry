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
 *   STM32 Bridge App - reads CCSDS packets from STM32 via serial
 *   and publishes sensor data to the cFS Software Bus.
 */

#include "stm32_bridge_app.h"
#include "stm32_bridge_app_cmds.h"
#include "stm32_bridge_app_utils.h"
#include "stm32_bridge_app_eventids.h"
#include "stm32_bridge_app_dispatch.h"
#include "stm32_bridge_app_tbl.h"
#include "stm32_bridge_app_version.h"

#include <errno.h>
#include <termios.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

/* Forward declarations */
static int  STM32_BRIDGE_APP_OpenSerial(const char *port, speed_t baud);
static bool STM32_BRIDGE_APP_ReadExact(int fd, uint8 *buf, size_t n) __attribute__((unused));

/*
** Global data
*/
STM32_BRIDGE_APP_Data_t STM32_BRIDGE_APP_Data;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                            */
/* Application entry point and main process loop                              */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
void STM32Bridge_Main(void)
{
    CFE_Status_t status;
    uint16       pkt_len;
    uint8        buf[64];
    ssize_t      read_result;
    int          i;
    int          packet_count = 0;

    CFE_ES_PerfLogEntry(STM32_BRIDGE_APP_PERF_ID);

    status = STM32_BRIDGE_APP_Init();
    if (status != CFE_SUCCESS)
    {
        STM32_BRIDGE_APP_Data.RunStatus = CFE_ES_RunStatus_APP_ERROR;
    }

    while (CFE_ES_RunLoop(&STM32_BRIDGE_APP_Data.RunStatus) == true)
    {
        CFE_ES_PerfLogExit(STM32_BRIDGE_APP_PERF_ID);

        /* If serial port not open, retry every 2 seconds */
        if (STM32_BRIDGE_APP_Data.SerialFd < 0)
        {
            OS_TaskDelay(2000);
            STM32_BRIDGE_APP_Data.SerialFd = STM32_BRIDGE_APP_OpenSerial(
                STM32_BRIDGE_APP_PLATFORM_SERIAL_PORT,
                STM32_BRIDGE_APP_PLATFORM_SERIAL_BAUD);
            if (STM32_BRIDGE_APP_Data.SerialFd >= 0)
            {
                printf("[STM32_BRIDGE_APP] Serial port opened successfully: fd=%d\n",
                       STM32_BRIDGE_APP_Data.SerialFd);
            }
            CFE_ES_PerfLogEntry(STM32_BRIDGE_APP_PERF_ID);
            continue;
        }

        /* Read 2-byte length prefix — blocks up to 2 seconds (VTIME=20) */
        read_result = read(STM32_BRIDGE_APP_Data.SerialFd, (uint8 *)&pkt_len, 2);
        if (read_result != 2)
        {
            if (read_result < 0 && errno != EAGAIN)
            {
                printf("[STM32_BRIDGE_APP] Serial read error: %d (errno=%d: %s)\n",
                       (int)read_result, errno, strerror(errno));
            }
            /* No data yet — loop back and try again */
            CFE_ES_PerfLogEntry(STM32_BRIDGE_APP_PERF_ID);
            continue;
        }

        /* Validate length */
        if (pkt_len < 8 || pkt_len > sizeof(buf))
        {
            printf("[STM32_BRIDGE_APP] Invalid packet length: %u (min=8, max=%lu) — flushing\n",
                   pkt_len, sizeof(buf));
            tcflush(STM32_BRIDGE_APP_Data.SerialFd, TCIFLUSH);
            CFE_ES_PerfLogEntry(STM32_BRIDGE_APP_PERF_ID);
            continue;
        }

        /* Read rest of packet */
        read_result = read(STM32_BRIDGE_APP_Data.SerialFd, buf, pkt_len);
        if (read_result != (int)pkt_len)
        {
            printf("[STM32_BRIDGE_APP] Short packet read: got %d, expected %u\n",
                   (int)read_result, pkt_len);
            CFE_ES_PerfLogEntry(STM32_BRIDGE_APP_PERF_ID);
            continue;
        }

        /* Print raw hex dump */
        printf("[STM32_BRIDGE_APP] Raw packet bytes: ");
        for (i = 0; i < pkt_len; i++)
        {
            printf("%02X ", buf[i]);
        }
        printf("\n");

        /* Parse CCSDS packet payload (skip 6-byte primary header) */
        if (pkt_len >= 10)
        {
            int16  temp_raw    = (int16)((buf[7] << 8) | buf[6]);   /* little-endian */
            uint16 hum_raw     = (uint16)((buf[9] << 8) | buf[8]);  /* little-endian */
            float  temp_c      = temp_raw / 100.0f;
            float  hum_pct     = hum_raw  / 100.0f;

            packet_count++;

            printf("[STM32_BRIDGE_APP] Seq=%d Temp=%.2f C  Humidity=%.2f%%\n",
                   packet_count, temp_c, hum_pct);

            /* Update sensor data packet */
            STM32_BRIDGE_APP_Data.SensorData.Payload.Temperature   = temp_c;
            STM32_BRIDGE_APP_Data.SensorData.Payload.Humidity       = hum_pct;
            STM32_BRIDGE_APP_Data.SensorData.Payload.SequenceCount  = packet_count;
            STM32_BRIDGE_APP_Data.SensorData.Payload.Status         = 0;

            /* Timestamp and publish to Software Bus */
            CFE_SB_TimeStampMsg(CFE_MSG_PTR(STM32_BRIDGE_APP_Data.SensorData.TelemetryHeader));
            CFE_SB_TransmitMsg(CFE_MSG_PTR(STM32_BRIDGE_APP_Data.SensorData.TelemetryHeader), true);

            CFE_EVS_SendEvent(STM32_BRIDGE_APP_INIT_INF_EID,
                              CFE_EVS_EventType_INFORMATION,
                              "STM32_BRIDGE_APP: Seq=%d Temp=%.2fC Humidity=%.2f%%",
                              packet_count, temp_c, hum_pct);
        }
        else
        {
            printf("[STM32_BRIDGE_APP] Packet too short: %u bytes (need >= 10)\n", pkt_len);
        }

        /* Wait 2 seconds before reading next packet */
        OS_TaskDelay(2000);

        CFE_ES_PerfLogEntry(STM32_BRIDGE_APP_PERF_ID);
    }

    CFE_ES_PerfLogExit(STM32_BRIDGE_APP_PERF_ID);
    CFE_ES_ExitApp(STM32_BRIDGE_APP_Data.RunStatus);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                            */
/* Serial Port Helper Functions                                               */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static int STM32_BRIDGE_APP_OpenSerial(const char *port, speed_t baud)
{
    struct termios tty;

    int fd = open(port, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        printf("[STM32_BRIDGE_APP] Failed to open port %s: errno=%d (%s)\n",
               port, errno, strerror(errno));
        return -1;
    }

    printf("[STM32_BRIDGE_APP] Opened port %s (fd=%d)\n", port, fd);

    memset(&tty, 0, sizeof(tty));

    if (tcgetattr(fd, &tty) != 0)
    {
        printf("[STM32_BRIDGE_APP] tcgetattr failed: errno=%d (%s)\n",
               errno, strerror(errno));
        close(fd);
        return -1;
    }

    cfsetospeed(&tty, baud);
    cfsetispeed(&tty, baud);

    /* 8N1, no flow control */
    tty.c_cflag |=  (CS8 | CREAD | CLOCAL);
    tty.c_cflag &= ~(PARENB | CSTOPB);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON | IXOFF);
    tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tty.c_oflag &= ~OPOST;

    /* Block until at least 1 byte, timeout after 2 seconds */
    tty.c_cc[VMIN]  = 1;
    tty.c_cc[VTIME] = 20;  /* 2 seconds (units of 0.1s) */

    if (tcsetattr(fd, TCSANOW, &tty) != 0)
    {
        printf("[STM32_BRIDGE_APP] tcsetattr failed: errno=%d (%s)\n",
               errno, strerror(errno));
        close(fd);
        return -1;
    }

    /* Flush any stale data */
    tcflush(fd, TCIOFLUSH);

    printf("[STM32_BRIDGE_APP] Serial configured: 8N1, VMIN=1, VTIME=20 (2s timeout)\n");

    return fd;
}

static bool STM32_BRIDGE_APP_ReadExact(int fd, uint8 *buf, size_t n)
{
    size_t got = 0;
    while (got < n)
    {
        ssize_t r = read(fd, buf + got, n - got);
        if (r < 0 && errno != EAGAIN)
            return false;
        if (r > 0)
            got += r;
    }
    return true;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                            */
/* Initialization                                                             */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
CFE_Status_t STM32_BRIDGE_APP_Init(void)
{
    CFE_Status_t status;
    char         VersionString[STM32_BRIDGE_APP_CFG_MAX_VERSION_STR_LEN];

    printf("[STM32_BRIDGE_APP] Initialization starting...\n");

    memset(&STM32_BRIDGE_APP_Data, 0, sizeof(STM32_BRIDGE_APP_Data));
    STM32_BRIDGE_APP_Data.RunStatus = CFE_ES_RunStatus_APP_RUN;
    STM32_BRIDGE_APP_Data.SerialFd  = -1;

    /* Register events */
    status = CFE_EVS_Register(NULL, 0, CFE_EVS_EventFilter_BINARY);
    if (status != CFE_SUCCESS)
    {
        CFE_ES_WriteToSysLog("STM32_BRIDGE_APP: Error Registering Events, RC = 0x%08lX\n",
                             (unsigned long)status);
        return status;
    }

    /* Initialize sensor data telemetry packet */
    CFE_MSG_Init(CFE_MSG_PTR(STM32_BRIDGE_APP_Data.SensorData.TelemetryHeader),
                 CFE_SB_ValueToMsgId(STM32_BRIDGE_APP_SENSOR_DATA_MID),
                 sizeof(STM32_BRIDGE_APP_Data.SensorData));

    printf("[STM32_BRIDGE_APP] Sensor data message initialized (MID=0x%X, size=%lu)\n",
           STM32_BRIDGE_APP_SENSOR_DATA_MID,
           sizeof(STM32_BRIDGE_APP_Data.SensorData));

    /* Initialize housekeeping packet */
    CFE_MSG_Init(CFE_MSG_PTR(STM32_BRIDGE_APP_Data.HkTlm.TelemetryHeader),
                 CFE_SB_ValueToMsgId(STM32_BRIDGE_APP_HK_TLM_MID),
                 sizeof(STM32_BRIDGE_APP_Data.HkTlm));

    /* Open serial port */
    printf("[STM32_BRIDGE_APP] Attempting to open serial port: %s @ %u baud\n",
           STM32_BRIDGE_APP_PLATFORM_SERIAL_PORT,
           (unsigned)STM32_BRIDGE_APP_PLATFORM_SERIAL_BAUD);

    STM32_BRIDGE_APP_Data.SerialFd = STM32_BRIDGE_APP_OpenSerial(
        STM32_BRIDGE_APP_PLATFORM_SERIAL_PORT,
        STM32_BRIDGE_APP_PLATFORM_SERIAL_BAUD);

    if (STM32_BRIDGE_APP_Data.SerialFd < 0)
    {
        printf("[STM32_BRIDGE_APP] WARNING: Serial port not available at init (will retry)\n");
        CFE_EVS_SendEvent(STM32_BRIDGE_APP_CR_PIPE_ERR_EID,
                          CFE_EVS_EventType_INFORMATION,
                          "STM32_BRIDGE_APP: Serial port %s not available (will retry)",
                          STM32_BRIDGE_APP_PLATFORM_SERIAL_PORT);
    }
    else
    {
        printf("[STM32_BRIDGE_APP] Serial port opened successfully\n");
    }

    CFE_Config_GetVersionString(VersionString,
                                STM32_BRIDGE_APP_CFG_MAX_VERSION_STR_LEN,
                                "STM32_BRIDGE_APP",
                                STM32_BRIDGE_APP_VERSION,
                                STM32_BRIDGE_APP_BUILD_CODENAME,
                                STM32_BRIDGE_APP_LAST_OFFICIAL);

    printf("[STM32_BRIDGE_APP] Initialization complete.%s\n", VersionString);

    CFE_EVS_SendEvent(STM32_BRIDGE_APP_INIT_INF_EID,
                      CFE_EVS_EventType_INFORMATION,
                      "STM32_BRIDGE_APP Initialized.%s",
                      VersionString);

    return CFE_SUCCESS;
}