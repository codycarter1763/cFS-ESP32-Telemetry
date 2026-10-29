#ifndef ESP32_BRIDGE_APP_COVERAGETEST_COMMON_H
#define ESP32_BRIDGE_APP_COVERAGETEST_COMMON_H

/*
 * Includes
 */

#include "utassert.h"
#include "uttest.h"
#include "utstubs.h"

#include "setup.h"
#include "eventcheck.h"

#include "cfe.h"
#include "esp32_bridge_app_eventids.h"
#include "esp32_bridge_app.h"
#include "esp32_bridge_app_dispatch.h"
#include "esp32_bridge_app_cmds.h"
#include "esp32_bridge_app_utils.h"
#include "esp32_bridge_app_serial.h"
#include "esp32_bridge_app_msgids.h"
#include "esp32_bridge_app_msg.h"
#include "esp32_bridge_app_tbl.h"

/*
 * Macro to add a test case to the list of tests to execute
 */
#define ADD_TEST(test) UtTest_Add((Test_##test), Sample_UT_Setup, Sample_UT_TearDown, #test)

#endif /* ESP32_BRIDGE_APP_COVERAGETEST_COMMON_H */