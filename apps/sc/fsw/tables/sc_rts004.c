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
 *   CFS Stored Command (SC) RTS table 4 -- actionpoint re-arm response
 *
 * Both AP0 (temperature) and AP1 (humidity) are configured to trigger
 * this same RTS on fault. Since an RTS runs a fixed script regardless
 * of which actionpoint called it, this re-arms BOTH back to ACTIVE --
 * harmless no-op for whichever one didn't actually fail.
 */

#include "cfe.h"
#include "cfe_tbl_filedef.h"

#include "sc_tbldefs.h"
#include "sc_platform_cfg.h"
#include "sc_msgdefs.h"
#include "sc_msgids.h"
#include "sc_msg.h"

#define LC_CMD_MID_VALUE 0x18A4
#define LC_SET_AP_STATE_CC 3
#define LC_APSTATE_ACTIVE 1

/* Both checksums computed from the real algorithm and confirmed live
   via LC's own acknowledgment events for each AP number */
#ifndef LC_SET_AP_STATE_AP0_CKSUM
#define LC_SET_AP_STATE_AP0_CKSUM 0x84
#endif
#ifndef LC_SET_AP_STATE_AP1_CKSUM
#define LC_SET_AP_STATE_AP1_CKSUM 0x85
#endif

typedef struct
{
    uint16 APNumber;
    uint16 NewAPState;
} LocalSetAPState_Payload_t;

typedef struct
{
    CFE_MSG_CommandHeader_t   CommandHeader;
    LocalSetAPState_Payload_t Payload;
} LocalSetAPStateCmd_t;

typedef struct
{
    SC_RtsEntryHeader_t  hdr1;
    LocalSetAPStateCmd_t cmd1;
    SC_RtsEntryHeader_t  hdr2;
    LocalSetAPStateCmd_t cmd2;
} SC_RtsStruct004_t;

typedef union
{
    SC_RtsStruct004_t rts;
    uint16            buf[SC_RTS_BUFF_SIZE];
} SC_RtsTable004_t;

#define SC_MEMBER_SIZE(member) (sizeof(((SC_RtsStruct004_t *)0)->member))

SC_RtsTable004_t SC_Rts004 = {
    /* 1: re-arm AP0 (temperature) */
    .rts.hdr1.WakeupCount = 0,
    .rts.cmd1 = { CFE_MSG_CMD_HDR_INIT(LC_CMD_MID_VALUE, SC_MEMBER_SIZE(cmd1), LC_SET_AP_STATE_CC, LC_SET_AP_STATE_AP0_CKSUM) },
    .rts.cmd1.Payload.APNumber   = 0,
    .rts.cmd1.Payload.NewAPState = LC_APSTATE_ACTIVE,

    /* 2: re-arm AP1 (humidity) */
    .rts.hdr2.WakeupCount = 1,
    .rts.cmd2 = { CFE_MSG_CMD_HDR_INIT(LC_CMD_MID_VALUE, SC_MEMBER_SIZE(cmd2), LC_SET_AP_STATE_CC, LC_SET_AP_STATE_AP1_CKSUM) },
    .rts.cmd2.Payload.APNumber   = 1,
    .rts.cmd2.Payload.NewAPState = LC_APSTATE_ACTIVE
};

CFE_TBL_FILEDEF(SC_Rts004, SC.RTS_TBL004, SC AP Rearm Both, sc_rts004.tbl)