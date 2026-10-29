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

/*
** File: coveragetest_esp32_bridge_app.c
**
** Purpose:
** Coverage Unit Test cases for the Sample Application
**
** Notes:
** This implements various test cases to exercise all code
** paths through all functions defined in the Sample application.
**
** It is primarily focused at providing examples of the various
** stub configurations, hook functions, and wrapper calls that
** are often needed when coercing certain code paths through
** complex functions.
*/

/*
 * Includes
 */

#include "esp32_bridge_app_coveragetest_common.h"

/*
**********************************************************************************
**          TEST CASE FUNCTIONS
**********************************************************************************
*/


void Test_ESP32_BRIDGE_APP_Init(void)
{
    UtAssert_INT32_EQ(ESP32_BRIDGE_APP_Init(), CFE_SUCCESS);
 
    UtAssert_UINT32_EQ(ESP32_BRIDGE_APP_Data.RunStatus, CFE_ES_RunStatus_APP_RUN);
    UtAssert_INT32_EQ(ESP32_BRIDGE_APP_Data.SerialFd, 0); /* stub default return */
 
    /* Sensor packet and HK packet both initialized */
    UtAssert_STUB_COUNT(CFE_MSG_Init, 2);
    UtAssert_STUB_COUNT(ESP32_BRIDGE_APP_OpenSerial, 1);
    UtAssert_STUB_COUNT(CFE_EVS_SendEvent, 1); /* "Initialized" */
}

/*
 * One complete frame: sync (AA 55), little-endian length (14),
 * then a 14-byte CCSDS packet. Payload content doesn't matter here
 * because the parser is stubbed -- it's tested in coveragetest_esp32_bridge_app_utils.c
 */
static const uint8 UT_GoodFrame[] = {
    0xAA, 0x55,                         /* sync */
    0x0E, 0x00,                         /* length = 14, native LE */
    0x08, 0x95, 0xC0, 0x00, 0x00, 0x07, /* primary header */
    0x00, 0x00, 0x00, 0x00,             /* time */
    0x09, 0x29,                         /* temp 23.45 C */
    0x16, 0x2E                          /* humidity 56.78 % */
};

void Test_ESP32_BRIDGE_APP_Init_ESP32_Disconnect(void)
{
    UT_CheckEvent_t EventTest;

    /* ESP32 not plugged in: init should work but main loop will keep polling to connect */
    UT_SetDefaultReturnValue(UT_KEY(ESP32_BRIDGE_APP_OpenSerial), -1);
    UT_CHECKEVENT_SETUP(&EventTest, ESP32_BRIDGE_APP_CR_PIPE_ERR_EID, 
    "ESP32_BRIDGE_APP: Serial port %s not available (will retry)");

    UtAssert_INT32_EQ(ESP32_BRIDGE_APP_Init(), CFE_SUCCESS);

    UtAssert_INT32_EQ(ESP32_BRIDGE_APP_Data.SerialFd, -1);
    UtAssert_UINT32_EQ(EventTest.MatchCount, 1);
}

void Test_ESP32Bridge_GoodFrame(void)
{
    UT_CheckEvent_t EventTest;
    UT_SetDataBuffer(UT_KEY(ESP32_BRIDGE_APP_ReadSerial), (void *)UT_GoodFrame, sizeof(UT_GoodFrame), true);

    for (int i = 0; i < 2; ++i)
    {
        UT_SetDeferredRetcode(UT_KEY(CFE_ES_RunLoop), 1, true);
    }

    UT_CHECKEVENT_SETUP(&EventTest, ESP32_BRIDGE_APP_INIT_INF_EID,
                        "ESP32_BRIDGE_APP: Seq=%d Temp=%.1fC Humidity=%.1f%%");

    ESP32Bridge_Main();
    
    /* 1 sync byte + 1 sync byte + 2 length bytes + 1 payload read */
    UtAssert_STUB_COUNT(ESP32_BRIDGE_APP_ReadSerial, 4);
    UtAssert_STUB_COUNT(ESP32_BRIDGE_APP_ParseSensorPacket, 1);
    UtAssert_STUB_COUNT(CFE_SB_TimeStampMsg, 1);
    UtAssert_STUB_COUNT(CFE_SB_TransmitMsg, 1);
    UtAssert_UINT32_EQ(ESP32_BRIDGE_APP_Data.SensorData.Payload.SequenceCount, 1);
    UtAssert_UINT32_EQ(EventTest.MatchCount, 1);
    UtAssert_STUB_COUNT(CFE_ES_ExitApp, 1);
}

/*
 * Feed one frame whose length field is Len (payload bytes are zeros), and
 * run the main loop twice: iteration 1 reads 0xAA, iteration 2 reads 0x55,
 * the length, and (if accepted) the payload.
 */
static void UT_RunFrameWithLength(uint16 Len)
{
    static uint8 Frame[4 + 65];
    int32        i;

    memset(Frame, 0, sizeof(Frame));
    Frame[0] = 0xAA;
    Frame[1] = 0x55;
    Frame[2] = (uint8)(Len & 0xFF); /* length is little-endian on the wire */
    Frame[3] = (uint8)(Len >> 8);

    UT_SetDataBuffer(UT_KEY(ESP32_BRIDGE_APP_ReadSerial), Frame, 4 + Len, true);

    for (i = 0; i < 2; ++i)
    {
        UT_SetDeferredRetcode(UT_KEY(CFE_ES_RunLoop), 1, true);
    }

    ESP32Bridge_Main();
}

/* Loop boundary: CFE_ES_RunLoop returns false immediately -> body runs 0 times */
void Test_ESP32Bridge_Main_ZeroIterations(void)
{
    ESP32Bridge_Main();

    UtAssert_STUB_COUNT(ESP32_BRIDGE_APP_ReadSerial, 0);
    UtAssert_STUB_COUNT(CFE_SB_TransmitMsg, 0);
    UtAssert_STUB_COUNT(CFE_ES_ExitApp, 1);
}

/*
 * Length guard: if (pkt_len < MIN_LEN || pkt_len > sizeof(buf))   MIN_LEN = 14, sizeof(buf) = 64
 *
 *   Len | A: < 14 | B: > 64 | Decision | Role
 *   ----+---------+---------+----------+------------------------------------
 *    13 |    T    |    F    |  reject  | MC/DC: A alone flips the decision; lower boundary - 1
 *    14 |    F    |    F    |  accept  | MC/DC: baseline; lower boundary
 *    64 |    F    |    F    |  accept  | upper boundary
 *    65 |    F    |    T    |  reject  | MC/DC: B alone flips the decision; upper boundary + 1
 */
void Test_ESP32Bridge_Main_Len13_Rejected(void)
{
    UT_RunFrameWithLength(13);

    UtAssert_STUB_COUNT(ESP32_BRIDGE_APP_ReadSerial, 3); /* sync, sync, length -- no payload read */
    UtAssert_STUB_COUNT(CFE_SB_TransmitMsg, 0);
}

void Test_ESP32Bridge_Main_Len14_Accepted(void)
{
    UT_RunFrameWithLength(14);

    UtAssert_STUB_COUNT(ESP32_BRIDGE_APP_ReadSerial, 4); /* sync, sync, length, payload */
    UtAssert_STUB_COUNT(CFE_SB_TransmitMsg, 1);
}

void Test_ESP32Bridge_Main_Len64_Accepted(void)
{
    UT_RunFrameWithLength(64);

    UtAssert_STUB_COUNT(ESP32_BRIDGE_APP_ReadSerial, 4);
    UtAssert_STUB_COUNT(CFE_SB_TransmitMsg, 1);
}

void Test_ESP32Bridge_Main_Len65_Rejected(void)
{
    UT_RunFrameWithLength(65);

    UtAssert_STUB_COUNT(ESP32_BRIDGE_APP_ReadSerial, 3);
    UtAssert_STUB_COUNT(CFE_SB_TransmitMsg, 0);
}

/*
 * Register the test cases to execute with the unit test tool
 */
void UtTest_Setup(void)
{
    ADD_TEST(ESP32_BRIDGE_APP_Init);
    ADD_TEST(ESP32Bridge_GoodFrame);
    ADD_TEST(ESP32_BRIDGE_APP_Init_ESP32_Disconnect);
    ADD_TEST(ESP32Bridge_Main_ZeroIterations);
    ADD_TEST(ESP32Bridge_Main_Len13_Rejected);
    ADD_TEST(ESP32Bridge_Main_Len14_Accepted);
    ADD_TEST(ESP32Bridge_Main_Len64_Accepted);
    ADD_TEST(ESP32Bridge_Main_Len65_Rejected);
}
