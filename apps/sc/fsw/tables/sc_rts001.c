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
 *   CFS Stored Command (SC) sample RTS table 1
 *
 * SC NOOP command, execution wakeup count relative to start of RTS = 0
 * SC Enable RTS #2 command, execution wakeup count relative to prev cmd = 5
 * SC Start RTS #2 command, execution wakeup count relative to prev cmd = 5
 * SC Enable RTS #3 command, execution wakeup count relative to prev cmd = 5
 * SC Start RTS #3 command, execution wakeup count relative to prev cmd = 5
 * SC Enable RTS #4 command, execution wakeup count relative to prev cmd = 5
 * (RTS #4 is only ever Started by LC itself, as the humidity re-arm response)
 */

#include "cfe.h"
#include "cfe_tbl_filedef.h"

#include "sc_tbldefs.h"
#include "sc_platform_cfg.h"
#include "sc_msgdefs.h"
#include "sc_msgids.h"
#include "sc_msg.h"

#ifndef SC_NOOP_CKSUM
#define SC_NOOP_CKSUM (0x3E ^ ((SC_CMD_MID & 0xFF00) >> 8u) ^ ((SC_CMD_MID & 0x00FF)))
#endif
#ifndef SC_ENABLE_RTS2_CKSUM
#define SC_ENABLE_RTS2_CKSUM (0x3F ^ ((SC_CMD_MID & 0xFF00) >> 8u) ^ ((SC_CMD_MID & 0x00FF)))
#endif
#ifndef SC_START_RTS2_CKSUM
#define SC_START_RTS2_CKSUM (0x3C ^ ((SC_CMD_MID & 0xFF00) >> 8u) ^ ((SC_CMD_MID & 0x00FF)))
#endif
#ifndef SC_ENABLE_RTS3_CKSUM
#define SC_ENABLE_RTS3_CKSUM (SC_ENABLE_RTS2_CKSUM ^ 0x01)
#endif
#ifndef SC_START_RTS3_CKSUM
#define SC_START_RTS3_CKSUM (SC_START_RTS2_CKSUM ^ 0x01)
#endif
/* RtsNum low byte: 2 (base) -> 4 = XOR delta 0x06 */
#ifndef SC_ENABLE_RTS4_CKSUM
#define SC_ENABLE_RTS4_CKSUM (SC_ENABLE_RTS2_CKSUM ^ 0x06)
#endif

typedef struct
{
    SC_RtsEntryHeader_t hdr1;
    SC_NoopCmd_t        cmd1;
    SC_RtsEntryHeader_t hdr2;
    SC_EnableRtsCmd_t   cmd2;
    SC_RtsEntryHeader_t hdr3;
    SC_StartRtsCmd_t    cmd3;
    SC_RtsEntryHeader_t hdr4;
    SC_EnableRtsCmd_t   cmd4;
    SC_RtsEntryHeader_t hdr5;
    SC_StartRtsCmd_t    cmd5;
    SC_RtsEntryHeader_t hdr6;
    SC_EnableRtsCmd_t   cmd6;
} SC_RtsStruct001_t;

typedef union
{
    SC_RtsStruct001_t rts;
    uint16            buf[SC_RTS_BUFF_SIZE];
} SC_RtsTable001_t;

#define SC_MEMBER_SIZE(member) (sizeof(((SC_RtsStruct001_t *)0)->member))

SC_RtsTable001_t SC_Rts001 = {
    /* 1 */
    .rts.hdr1.WakeupCount = 0,
    .rts.cmd1             = { CFE_MSG_CMD_HDR_INIT(SC_CMD_MID, SC_MEMBER_SIZE(cmd1), SC_NOOP_CC, SC_NOOP_CKSUM) },

    /* 2 */
    .rts.hdr4.WakeupCount = 5,
    .rts.cmd4 = { CFE_MSG_CMD_HDR_INIT(SC_CMD_MID, SC_MEMBER_SIZE(cmd4), SC_ENABLE_RTS_CC, SC_ENABLE_RTS3_CKSUM) },
    .rts.cmd4.Payload.RtsNum = SC_RTS_NUM_INITIALIZER(3),

    /* 3 */
    .rts.hdr5.WakeupCount = 5,
    .rts.cmd5 = { CFE_MSG_CMD_HDR_INIT(SC_CMD_MID, SC_MEMBER_SIZE(cmd5), SC_START_RTS_CC, SC_START_RTS3_CKSUM) },
    .rts.cmd5.Payload.RtsNum = SC_RTS_NUM_INITIALIZER(3),

    /* 4 -- enable RTS4 so LC can start it on demand*/
    .rts.hdr6.WakeupCount = 5,
    .rts.cmd6 = { CFE_MSG_CMD_HDR_INIT(SC_CMD_MID, SC_MEMBER_SIZE(cmd6), SC_ENABLE_RTS_CC, SC_ENABLE_RTS4_CKSUM) },
    .rts.cmd6.Payload.RtsNum = SC_RTS_NUM_INITIALIZER(4)
};

CFE_TBL_FILEDEF(SC_Rts001, SC.RTS_TBL001, SC Example RTS_TBL001, sc_rts001.tbl)