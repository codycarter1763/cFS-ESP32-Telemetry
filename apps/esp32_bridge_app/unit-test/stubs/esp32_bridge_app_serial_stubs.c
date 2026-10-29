

/**
 * @file
 *
 * Auto-Generated stub implementations for functions defined in esp32_bridge_app_serial header
 */

#include "esp32_bridge_app_serial.h"
#include "utgenstub.h"


/*
 * ----------------------------------------------------
 * Generated stub function for ESP32_BRIDGE_APP_CloseSerial()
 * ----------------------------------------------------
 */
void ESP32_BRIDGE_APP_CloseSerial(int fd)
{
UT_GenStub_AddParam(ESP32_BRIDGE_APP_CloseSerial, int , fd);

UT_GenStub_Execute(ESP32_BRIDGE_APP_CloseSerial, Basic, NULL);

}

/*
 * ----------------------------------------------------
 * Generated stub function for ESP32_BRIDGE_APP_OpenSerial()
 * ----------------------------------------------------
 */
int ESP32_BRIDGE_APP_OpenSerial(const char *port, speed_t baud)
{
UT_GenStub_SetupReturnBuffer(ESP32_BRIDGE_APP_OpenSerial, int);

UT_GenStub_AddParam(ESP32_BRIDGE_APP_OpenSerial, const char *, port);
UT_GenStub_AddParam(ESP32_BRIDGE_APP_OpenSerial, speed_t , baud);

UT_GenStub_Execute(ESP32_BRIDGE_APP_OpenSerial, Basic, NULL);

return UT_GenStub_GetReturnValue(ESP32_BRIDGE_APP_OpenSerial, int);
}

/*
 * ----------------------------------------------------
 * Generated stub function for ESP32_BRIDGE_APP_ReadSerial()
 * ----------------------------------------------------
 */
ssize_t ESP32_BRIDGE_APP_ReadSerial(int fd, void *buf, size_t n)
{
    int32 Status;

    Status = UT_DEFAULT_IMPL_RC(ESP32_BRIDGE_APP_ReadSerial, INT32_MAX);
    if (Status != INT32_MAX)
    {
        return Status;
    }

    return (ssize_t)UT_Stub_CopyToLocal(UT_KEY(ESP32_BRIDGE_APP_ReadSerial), buf, n);
}
