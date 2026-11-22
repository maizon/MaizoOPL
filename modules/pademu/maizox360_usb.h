#ifndef MAIZOX360_USB_H
#define MAIZOX360_USB_H
#include "maizox360.h"

static UsbEndpointDescriptor *maizox360_find_input(int devId, UsbDeviceDescriptor *device)
{
    UsbConfigDescriptor *config;
    int off;
    if (!device)
        return NULL;
    config = (UsbConfigDescriptor *)UsbGetDeviceStaticDescriptor(devId, device, USB_DT_CONFIG);
    if (!config || config->wTotalLength < 9 || config->wTotalLength > 4096)
        return NULL;
    off = maizox360_input_offset((const uint8_t *)config, config->wTotalLength);
    return off < 0 ? NULL : (UsbEndpointDescriptor *)((u8 *)config + off);
}
#endif
