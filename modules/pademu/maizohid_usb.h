#ifndef _MAIZOHID_USB_H_
#define _MAIZOHID_USB_H_

/* Read only the USB descriptors already enumerated by ps2sdk.  Do not
 * assume the first interface/endpoint of a composite device is the pad. */
static int maizohid_find_interface(int devId,
                                   UsbInterfaceDescriptor **found_interface,
                                   UsbEndpointDescriptor **found_input)
{
    UsbDeviceDescriptor *device;
    UsbConfigDescriptor *config;
    UsbInterfaceDescriptor *candidate = NULL;
    const u8 *cursor, *end;

    if (found_interface)
        *found_interface = NULL;
    if (found_input)
        *found_input = NULL;
    device = (UsbDeviceDescriptor *)UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_DEVICE);
    if (!device)
        return 0;
    config = (UsbConfigDescriptor *)UsbGetDeviceStaticDescriptor(devId, device, USB_DT_CONFIG);
    if (!config || config->bLength < 9 ||
        config->wTotalLength < config->bLength ||
        config->wTotalLength > 4096)
        return 0;

    cursor = (const u8 *)config + config->bLength;
    end = (const u8 *)config + config->wTotalLength;
    while (cursor + 2 <= end) {
        unsigned int length = cursor[0];
        if (length < 2 || length > (unsigned int)(end - cursor))
            return 0;
        if (cursor[1] == USB_DT_INTERFACE) {
            UsbInterfaceDescriptor *iface;
            candidate = NULL;
            if (length >= sizeof(UsbInterfaceDescriptor)) {
                iface = (UsbInterfaceDescriptor *)cursor;
                /* Never claim boot-keyboard or boot-mouse interfaces. */
                if (iface->bInterfaceClass == 0x03 &&
                    !(iface->bInterfaceSubClass == 1 &&
                      (iface->bInterfaceProtocol == 1 || iface->bInterfaceProtocol == 2)))
                    candidate = iface;
            }
        } else if (cursor[1] == USB_DT_ENDPOINT && candidate &&
                   length >= sizeof(UsbEndpointDescriptor)) {
            UsbEndpointDescriptor *ep = (UsbEndpointDescriptor *)cursor;
            if ((ep->bmAttributes & 3) == USB_ENDPOINT_XFER_INT &&
                (ep->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN) {
                if (found_interface)
                    *found_interface = candidate;
                if (found_input)
                    *found_input = ep;
                return 1;
            }
        }
        cursor += length;
    }
    return 0;
}

/* Ask the gamepad for its advertised report descriptor length (Microntek
 * 0079:0006 is commonly 107 bytes); low-speed firmware can reject 512.
 * This parser never reads past the enumerated config descriptor. */
static unsigned int maizohid_report_length(int devId, unsigned int iface_number)
{
    UsbDeviceDescriptor *device;
    UsbConfigDescriptor *config;
    const u8 *cursor, *end;
    int selected = 0;

    device = (UsbDeviceDescriptor *)UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_DEVICE);
    if (!device)
        return MAIZOHID_MAX_DESCRIPTOR;
    config = (UsbConfigDescriptor *)UsbGetDeviceStaticDescriptor(devId, device, USB_DT_CONFIG);
    if (!config || config->bLength < 9 ||
        config->wTotalLength < config->bLength ||
        config->wTotalLength > 4096)
        return MAIZOHID_MAX_DESCRIPTOR;
    cursor = (const u8 *)config + config->bLength;
    end = (const u8 *)config + config->wTotalLength;
    while (cursor + 2 <= end) {
        unsigned int length = cursor[0];
        if (length < 2 || length > (unsigned int)(end - cursor))
            break;
        if (cursor[1] == USB_DT_INTERFACE) {
            selected = length >= sizeof(UsbInterfaceDescriptor) &&
                       cursor[2] == iface_number;
        } else if (cursor[1] == 0x21 && selected && length >= 9 &&
                   cursor[6] == 0x22) {
            unsigned int report_len = cursor[7] | ((unsigned int)cursor[8] << 8);
            if (report_len && report_len <= MAIZOHID_MAX_DESCRIPTOR)
                return report_len;
            break;
        }
        cursor += length;
    }
    return MAIZOHID_MAX_DESCRIPTOR;
}

#endif
