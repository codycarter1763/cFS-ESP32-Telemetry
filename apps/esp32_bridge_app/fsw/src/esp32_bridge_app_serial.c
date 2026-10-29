/**
 * \file
 *   POSIX serial helpers for the ESP32 bridge app.
 */
#define _GNU_SOURCE
#include "esp32_bridge_app_serial.h"
#include "esp32_bridge_app_eventids.h"
#include "cfe.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

int ESP32_BRIDGE_APP_OpenSerial(const char *port, speed_t baud)
{
    struct termios tty;
    int            modem_status;

    int fd = open(port, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        printf("[ESP32_BRIDGE_APP] Failed to open port %s: errno=%d (%s)\n", port, errno, strerror(errno));

        CFE_EVS_SendEvent(ESP32_BRIDGE_APP_SERIAL_LOST_ERR_EID, CFE_EVS_EventType_ERROR,
                          "ESP32_BRIDGE_APP: Failed to open port %s:", port);
        return -1;
    }

    printf("[ESP32_BRIDGE_APP] Opened port %s (fd=%d)\n", port, fd);

    /* Explicitly de-assert DTR/RTS -- opening the port can otherwise trigger
       the board's auto-reset circuit into bootloader/download mode instead
       of a normal run-mode reset, leaving the ESP32 silently stuck */
    if (ioctl(fd, TIOCMGET, &modem_status) == 0)
    {
        modem_status &= ~(TIOCM_DTR | TIOCM_RTS);
        ioctl(fd, TIOCMSET, &modem_status);
    }

    memset(&tty, 0, sizeof(tty));

    if (tcgetattr(fd, &tty) != 0)
    {
        printf("[ESP32_BRIDGE_APP] tcgetattr failed: errno=%d (%s)\n", errno, strerror(errno));
        close(fd);
        return -1;
    }

    cfsetospeed(&tty, baud);
    cfsetispeed(&tty, baud);

    /* 8N1, no flow control */
    tty.c_cflag |= (CS8 | CREAD | CLOCAL);
    tty.c_cflag &= ~(PARENB | CSTOPB);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON | IXOFF);
    tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tty.c_oflag &= ~OPOST;

    /* Block until at least 1 byte, timeout after 2 seconds */
    tty.c_cc[VMIN]  = 1;
    tty.c_cc[VTIME] = 20; /* 2 seconds (units of 0.1s) */

    if (tcsetattr(fd, TCSANOW, &tty) != 0)
    {
        printf("[ESP32_BRIDGE_APP] tcsetattr failed: errno=%d (%s)\n", errno, strerror(errno));
        close(fd);
        return -1;
    }

    /* Flush any stale data */
    tcflush(fd, TCIOFLUSH);

    printf("[ESP32_BRIDGE_APP] Serial configured: 8N1, VMIN=1, VTIME=20 (2s timeout)\n");

    return fd;
}

ssize_t ESP32_BRIDGE_APP_ReadSerial(int fd, void *buf, size_t n)
{
    return read(fd, buf, n);
}

void ESP32_BRIDGE_APP_CloseSerial(int fd)
{
    close(fd);
}