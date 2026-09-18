#include "cfe.h"
#include "cfe_tbl_filedef.h"

#include "sc_tbldefs.h"
#include "sc_platform_cfg.h"
#include "sc_msgdefs.h"
#include "sc_msgids.h"
#include "sc_msg.h"

/* LC_SAMPLE_AP_MID's resolved value, confirmed directly from EVS output
   (ID = 0x000018A6) -- avoids pulling in LC's internal topicid headers,
   which aren't meant to be included from outside the LC app itself */
#define LC_SAMPLE_AP_MID_VALUE 0x18A6

/* Local copy of LC's Sample AP payload shape -- matches LC_SampleAP_Payload_t
   exactly (StartIndex, EndIndex, UpdateAge, Padding, all uint16) */
typedef struct
{
    uint16 StartIndex;
    uint16 EndIndex;
    uint16 UpdateAge;
    uint16 Padding;
} LocalSampleAP_Payload_t;

typedef struct
{
    CFE_MSG_CommandHeader_t CommandHeader;
    LocalSampleAP_Payload_t Payload;
} LocalSampleAPCmd_t;

#ifndef LC_SAMPLE_AP_CKSUM
#define LC_SAMPLE_AP_CKSUM 0x88
#endif
#ifndef SC_START_RTS2_CKSUM
#define SC_START_RTS2_CKSUM (0x3C ^ ((SC_CMD_MID & 0xFF00) >> 8u) ^ ((SC_CMD_MID & 0x00FF)))
#endif
#ifndef SC_START_RTS3_CKSUM
#define SC_START_RTS3_CKSUM (SC_START_RTS2_CKSUM ^ 0x01)
#endif

typedef struct
{
    SC_RtsEntryHeader_t hdr1;
    LocalSampleAPCmd_t  cmd1;
    SC_RtsEntryHeader_t hdr2;
    SC_StartRtsCmd_t    cmd2;
} SC_RtsStruct003_t;

typedef union
{
    SC_RtsStruct003_t rts;
    uint16            buf[SC_RTS_BUFF_SIZE];
} SC_RtsTable003_t;

#define SC_MEMBER_SIZE(member) (sizeof(((SC_RtsStruct003_t *)0)->member))

SC_RtsTable003_t SC_Rts003 = {
    /* 1: sample both actionpoints (index 0 and 1) */
    .rts.hdr1.WakeupCount = 0,
    .rts.cmd1 = { CFE_MSG_CMD_HDR_INIT(LC_SAMPLE_AP_MID_VALUE, SC_MEMBER_SIZE(cmd1), 0, LC_SAMPLE_AP_CKSUM) },
    .rts.cmd1.Payload.StartIndex = 0,
    .rts.cmd1.Payload.EndIndex   = 1,
    .rts.cmd1.Payload.UpdateAge  = 1,

    /* 2: loop back -- restart RTS 3, ~2 seconds later */
    .rts.hdr2.WakeupCount = 2,
    .rts.cmd2 = { CFE_MSG_CMD_HDR_INIT(SC_CMD_MID, SC_MEMBER_SIZE(cmd2), SC_START_RTS_CC, SC_START_RTS3_CKSUM) },
    .rts.cmd2.Payload.RtsNum = SC_RTS_NUM_INITIALIZER(3)
};

CFE_TBL_FILEDEF(SC_Rts003, SC.RTS_TBL003, SC LC Sample AP Loop, sc_rts003.tbl)