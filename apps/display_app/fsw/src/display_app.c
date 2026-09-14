#include "display_app.h"
#include "display_app_cmds.h"
#include "display_app_utils.h"
#include "display_app_eventids.h"
#include "display_app_dispatch.h"
#include "display_app_tbl.h"
#include "display_app_version.h"
#include "stm32_sensor_interface.h"

#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

DISPLAY_APP_Data_t DISPLAY_APP_Data;

/* UDP output for Python GUI */
static int GuiSocket = -1;
static struct sockaddr_in GuiAddr;

void DISPLAY_APP_Main(void)
{
    CFE_Status_t     status;
    CFE_SB_Buffer_t *SBBufPtr;
    CFE_SB_MsgId_t   MsgId;

    CFE_ES_PerfLogEntry(DISPLAY_APP_PERF_ID);

    status = DISPLAY_APP_Init();
    if (status != CFE_SUCCESS)
    {
        DISPLAY_APP_Data.RunStatus = CFE_ES_RunStatus_APP_ERROR;
    }

    while (CFE_ES_RunLoop(&DISPLAY_APP_Data.RunStatus) == true)
    {
        CFE_ES_PerfLogExit(DISPLAY_APP_PERF_ID);

        /* Wait for sensor data on Software Bus */
        status = CFE_SB_ReceiveBuffer(
            &SBBufPtr,
            DISPLAY_APP_Data.CommandPipe,
            CFE_SB_PEND_FOREVER);

        CFE_ES_PerfLogEntry(DISPLAY_APP_PERF_ID);

        if (status == CFE_SUCCESS)
        {
            CFE_MSG_GetMsgId(&SBBufPtr->Msg, &MsgId);

            if (CFE_SB_MsgIdToValue(MsgId) == STM32_BRIDGE_APP_SENSOR_DATA_MID)
            {
                STM32_BRIDGE_APP_SensorData_t *sensor =
                    (STM32_BRIDGE_APP_SensorData_t *)SBBufPtr;

                printf("<<<<< DISPLAY_APP: Seq=%u Temp=%.2fC Humidity=%.2f%% >>>>>\n",
                       sensor->Payload.SequenceCount,
                       sensor->Payload.Temperature,
                       sensor->Payload.Humidity);

                /* Send telemetry to Python GUI */
                if (GuiSocket >= 0)
                {
                    char udp_buffer[128];

                    snprintf(udp_buffer,
                             sizeof(udp_buffer),
                             "%u,%.2f,%.2f",
                             sensor->Payload.SequenceCount,
                             sensor->Payload.Temperature,
                             sensor->Payload.Humidity);

                    sendto(GuiSocket,
                           udp_buffer,
                           strlen(udp_buffer),
                           0,
                           (struct sockaddr *)&GuiAddr,
                           sizeof(GuiAddr));
                }

                CFE_EVS_SendEvent(
                    DISPLAY_APP_INIT_INF_EID,
                    CFE_EVS_EventType_INFORMATION,
                    "DISPLAY_APP: Seq=%u Temp=%.2fC Humidity=%.2f%%",
                    sensor->Payload.SequenceCount,
                    sensor->Payload.Temperature,
                    sensor->Payload.Humidity);
            }
        }
        else
        {
            CFE_EVS_SendEvent(
                DISPLAY_APP_PIPE_ERR_EID,
                CFE_EVS_EventType_ERROR,
                "DISPLAY_APP: SB Pipe Read Error RC=0x%08lX",
                (unsigned long)status);

            DISPLAY_APP_Data.RunStatus = CFE_ES_RunStatus_APP_ERROR;
        }
    }

    if (GuiSocket >= 0)
    {
        close(GuiSocket);
    }

    CFE_ES_PerfLogExit(DISPLAY_APP_PERF_ID);
    CFE_ES_ExitApp(DISPLAY_APP_Data.RunStatus);
}

CFE_Status_t DISPLAY_APP_Init(void)
{
    CFE_Status_t status;
    char VersionString[DISPLAY_APP_CFG_MAX_VERSION_STR_LEN];

    printf("[DISPLAY_APP] Initialization starting...\n");

    memset(&DISPLAY_APP_Data, 0, sizeof(DISPLAY_APP_Data));
    DISPLAY_APP_Data.RunStatus = CFE_ES_RunStatus_APP_RUN;

    status = CFE_EVS_Register(NULL, 0, CFE_EVS_EventFilter_BINARY);
    if (status != CFE_SUCCESS)
    {
        CFE_ES_WriteToSysLog(
            "DISPLAY_APP: Error Registering Events, RC = 0x%08lX\n",
            (unsigned long)status);
        return status;
    }

    printf("[DISPLAY_APP] Events registered successfully\n");

    /* Initialize housekeeping packet */
    CFE_MSG_Init(
        CFE_MSG_PTR(DISPLAY_APP_Data.HkTlm.TelemetryHeader),
        CFE_SB_ValueToMsgId(DISPLAY_APP_HK_TLM_MID),
        sizeof(DISPLAY_APP_Data.HkTlm));

    /* Create Software Bus pipe */
    status = CFE_SB_CreatePipe(
        &DISPLAY_APP_Data.CommandPipe,
        DISPLAY_APP_PLATFORM_PIPE_DEPTH,
        DISPLAY_APP_PLATFORM_PIPE_NAME);

    if (status != CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(
            DISPLAY_APP_CR_PIPE_ERR_EID,
            CFE_EVS_EventType_ERROR,
            "DISPLAY_APP: Error creating SB pipe, RC = 0x%08lX",
            (unsigned long)status);

        return status;
    }

    /* Subscribe to sensor data from stm32_bridge_app */
    status = CFE_SB_Subscribe(
        CFE_SB_ValueToMsgId(STM32_BRIDGE_APP_SENSOR_DATA_MID),
        DISPLAY_APP_Data.CommandPipe);

    if (status != CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(
            DISPLAY_APP_SUB_HK_ERR_EID,
            CFE_EVS_EventType_ERROR,
            "DISPLAY_APP: Error subscribing to sensor data, RC = 0x%08lX",
            (unsigned long)status);

        return status;
    }

    printf("[DISPLAY_APP] Subscribed to STM32 sensor data (MID=0x%X)\n",
           STM32_BRIDGE_APP_SENSOR_DATA_MID);

    /* Setup UDP connection to Python GUI */
    GuiSocket = socket(AF_INET, SOCK_DGRAM, 0);

    if (GuiSocket < 0)
    {
        perror("[DISPLAY_APP] socket");
    }
    else
    {
        memset(&GuiAddr, 0, sizeof(GuiAddr));

        GuiAddr.sin_family = AF_INET;
        GuiAddr.sin_port = htons(5000);

        if (inet_pton(AF_INET, "127.0.0.1", &GuiAddr.sin_addr) != 1)
        {
            printf("[DISPLAY_APP] Failed to configure GUI IP address\n");
        }
        else
        {
            printf("[DISPLAY_APP] UDP GUI output enabled on 127.0.0.1:5000\n");
        }
    }

    CFE_Config_GetVersionString(
        VersionString,
        DISPLAY_APP_CFG_MAX_VERSION_STR_LEN,
        "DISPLAY_APP",
        DISPLAY_APP_VERSION,
        DISPLAY_APP_BUILD_CODENAME,
        DISPLAY_APP_LAST_OFFICIAL);

    printf("[DISPLAY_APP] Initialization complete.%s\n",
           VersionString);

    CFE_EVS_SendEvent(
        DISPLAY_APP_INIT_INF_EID,
        CFE_EVS_EventType_INFORMATION,
        "DISPLAY_APP Initialized.%s",
        VersionString);

    return CFE_SUCCESS;
}