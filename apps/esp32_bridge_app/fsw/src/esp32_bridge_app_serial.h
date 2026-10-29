/**
 * \file
 *   Thin wrapper around the POSIX serial calls used by the ESP32 bridge.
 *
 *   Keeping open/read/close behind these functions gives the unit tests a
 *   seam: the coverage test for esp32_bridge_app.c links against stubs of
 *   these functions instead of touching a real /dev/ttyUSB0.
 */

#ifndef ESP32_BRIDGE_APP_SERIAL_H
#define ESP32_BRIDGE_APP_SERIAL_H

#include "common_types.h"

#include <sys/types.h>
#include <termios.h>

int ESP32_BRIDGE_APP_OpenSerial(const char *port, speed_t baud);
ssize_t ESP32_BRIDGE_APP_ReadSerial(int fd, void *buf, size_t n);
void ESP32_BRIDGE_APP_CloseSerial(int fd);


#endif