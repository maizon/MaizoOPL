#include "types.h"
#include "loadcore.h"
#include "stdio.h"
#include "sifrpc.h"
#include "sysclib.h"
#include "usbd.h"
#include "usbd_macro.h"
#include "thbase.h"
#include "intrman.h"
#include "thsemap.h"
#include "ds34usb.h"
#include "../../pademu/maizotwin.h"
#include "../../pademu/maizohid.h"
#include "../../pademu/maizohid_usb.h"
#include "../../pademu/maizohid_latch.h"
#include "../../../include/maizo_menu_input.h"

IRX_ID("ds34usb", 1, 1);

//#define DPRINTF(x...) printf(x)
#define DPRINTF(x...)

#define REQ_USB_OUT (USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE)
#define REQ_USB_IN  (USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE)

#define MAX_PADS 2

static u8 output_01_report[] =
    {
        0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x02,
        0xff, 0x27, 0x10, 0x00, 0x32,
        0xff, 0x27, 0x10, 0x00, 0x32,
        0xff, 0x27, 0x10, 0x00, 0x32,
        0xff, 0x27, 0x10, 0x00, 0x32,
        0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00};

static u8 led_patterns[][2] =
    {
        {0x1C, 0x02},
        {0x1A, 0x04},
        {0x16, 0x08},
        {0x0E, 0x10},
};

static u8 power_level[] =
    {
        0x00, 0x00, 0x02, 0x06, 0x0E, 0x1E};

static u8 rgbled_patterns[][2][3] =
    {
        {{0x00, 0x00, 0x10}, {0x00, 0x00, 0x7F}}, // light blue/blue
        {{0x00, 0x10, 0x00}, {0x00, 0x7F, 0x00}}, // light green/green/
        {{0x10, 0x10, 0x00}, {0x7F, 0x7F, 0x00}}, // light yellow/yellow
        {{0x00, 0x10, 0x10}, {0x00, 0x7F, 0x7F}}, // light cyan/cyan
};

static u8 link_key[] = // for ds4 authorisation
    {
        0x56, 0xE8, 0x81, 0x38, 0x08, 0x06, 0x51, 0x41,
        0xC0, 0x7F, 0x12, 0xAA, 0xD9, 0x66, 0x3C, 0xCE};

static u8 usb_buf[MAX_BUFFER_SIZE + 32] __attribute((aligned(4))) = {0};

int usb_probe(int devId);
int usb_connect(int devId);
int usb_disconnect(int devId);

static void usb_release(int pad);
static void usb_config_set(int result, int count, void *arg);

UsbDriver usb_driver = {NULL, NULL, "ds34usb", usb_probe, usb_connect, usb_disconnect};

static void DS3USB_init(int pad);
static void readReport(u8 *data, int bytes, int pad);
static void maizo_descriptor_cb(int resultCode, int bytes, void *arg);
static u8 hid_report_descriptor[MAX_PADS][MAIZOHID_MAX_DESCRIPTOR] __attribute__((aligned(4)));
static int LEDRumble(u8 *led, u8 lrum, u8 rrum, int pad);
static int Rumble(u8 lrum, u8 rrum, int pad);

ds34usb_device ds34pad[MAX_PADS];

/* One 0810:0001 USB receiver exposes two pads by report ID 0x01/0x02.
 * Only the root owns the USB endpoints. Both logical slots share its lock. */
static int twin_root = -1;
static int twin_slots[2] = {-1, -1};
/* Diagnosticos do USB no PS2: nenhuma etapa e fabricada a partir do VID/PID. */
enum {
    TWIN_STAGE_NOT_SEEN = 0,
    TWIN_STAGE_PROBED = 1,
    TWIN_STAGE_CONNECT = 2,
    TWIN_STAGE_REJECT_CLASS = 3,
    TWIN_STAGE_ENDPOINT_FAIL = 4,
    TWIN_STAGE_CONFIG_FAIL = 5,
    TWIN_STAGE_READY = 6,
    TWIN_STAGE_DISCONNECTED = 7
};
static u8 twin_stage, twin_class, twin_endpoints, twin_report_counts[2];

/* USB HID diagnostic state belongs to the IOP driver; VID/PID alone never
 * implies that a report descriptor parsed or that PadEmu is operational. */
enum {
    MAIZO_USB_NOT_SEEN = 0,
    MAIZO_USB_PROBED,
    MAIZO_USB_CONNECT,
    MAIZO_USB_REJECT_INTERFACE,
    MAIZO_USB_ENDPOINT_FAIL,
    MAIZO_USB_CONFIG_FAIL,
    MAIZO_USB_DESCRIPTOR_REQUEST_FAIL,
    MAIZO_USB_DESCRIPTOR_PARSE_FAIL,
    MAIZO_USB_READY,
    MAIZO_USB_DISCONNECTED,
    MAIZO_USB_NO_SLOT
};
static u8 maizo_usb_stage, maizo_usb_class, maizo_usb_endpoints;

/* Menu USB HID is sampled asynchronously too. The previous RPC handler
 * waited for every interrupt-IN completion and could stall the whole
 * dashboard by hundreds of milliseconds under fast navigation. */
static volatile u8 hid_stream_state[MAX_PADS];
static volatile int hid_stream_result[MAX_PADS], hid_stream_bytes[MAX_PADS];
static volatile unsigned int hid_stream_generation[MAX_PADS];
static volatile u8 hid_report_ready[MAX_PADS];
static u16 hid_pending[MAX_PADS];
static u8 hid_stream_buffers[MAX_PADS][MAX_BUFFER_SIZE] __attribute__((aligned(4)));
static int hid_stream_thread_id = -1;
static void hid_stream_thread(void *arg);
static void hid_stream_callback(int result, int bytes, void *arg);
static u8 maizo_usb_reports, maizo_usb_error;
static u16 maizo_usb_vid, maizo_usb_pid, maizo_usb_packet, maizo_usb_report_bits;



int usb_probe(int devId)
{
    UsbDeviceDescriptor *device = NULL;

    DPRINTF("DS34USB: probe: devId=%i\n", devId);

    device = (UsbDeviceDescriptor *)UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_DEVICE);
    if (device == NULL) {
        DPRINTF("DS34USB: Error - Couldn't get device descriptor\n");
        return 0;
    }

    if (device->idVendor == MAIZOHID_VID && device->idProduct == MAIZOHID_PID) {
        maizo_usb_stage = MAIZO_USB_PROBED;
        maizo_usb_vid = device->idVendor;
        maizo_usb_pid = device->idProduct;
        return 1;
    }
    if (device->idVendor == MAIZOTWIN_VID && device->idProduct == MAIZOTWIN_PID) {
        twin_stage = TWIN_STAGE_PROBED;
        return 1;
    }

    if (device->idVendor == DS34_VID && (device->idProduct == DS3_PID || device->idProduct == DS4_PID || device->idProduct == DS4_PID_SLIM))
        return 1;

    /* A valid HID gamepad interface is not restricted to one VID/PID. */
    if (maizohid_find_interface(devId, NULL, NULL)) {
        maizo_usb_stage = MAIZO_USB_PROBED;
        maizo_usb_vid = device->idVendor;
        maizo_usb_pid = device->idProduct;
        return 1;
    }
    return 0;
}

int usb_connect(int devId)
{
    int pad, epCount, second;
    UsbDeviceDescriptor *device;
    UsbConfigDescriptor *config;
    UsbInterfaceDescriptor *interface;
    UsbEndpointDescriptor *endpoint;
    UsbInterfaceDescriptor *hid_interface = NULL;
    UsbEndpointDescriptor *hid_endpoint = NULL;
    int generic_hid;

    DPRINTF("DS34USB: connect: devId=%i\n", devId);

    for (pad = 0; pad < MAX_PADS; pad++) {
        if (ds34pad[pad].devId == -1 && ds34pad[pad].enabled)
            break;
    }

    if (pad >= MAX_PADS) {
        if (maizo_usb_stage == MAIZO_USB_PROBED)
            maizo_usb_stage = MAIZO_USB_NO_SLOT;
        DPRINTF("DS34USB: Error - only %d device allowed !\n", MAX_PADS);
        return 1;
    }

    PollSema(ds34pad[pad].sema);

    ds34pad[pad].devId = devId;

    ds34pad[pad].status = DS34USB_STATE_AUTHORIZED;

    ds34pad[pad].controlEndp = UsbOpenEndpoint(devId, NULL);

    device = (UsbDeviceDescriptor *)UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_DEVICE);
    if (!device) {
        usb_release(pad);
        return 1;
    }
    config = (UsbConfigDescriptor *)UsbGetDeviceStaticDescriptor(devId, device, USB_DT_CONFIG);
    if (!config || config->bLength < sizeof(UsbConfigDescriptor) ||
        config->wTotalLength < sizeof(UsbConfigDescriptor) + sizeof(UsbInterfaceDescriptor)) {
        if (device->idVendor == MAIZOTWIN_VID && device->idProduct == MAIZOTWIN_PID)
            twin_stage = TWIN_STAGE_REJECT_CLASS;
        usb_release(pad);
        return 1;
    }
    interface = (UsbInterfaceDescriptor *)((char *)config + config->bLength);
    generic_hid = maizohid_find_interface(devId, &hid_interface, &hid_endpoint);
    if (maizo_usb_stage == MAIZO_USB_PROBED &&
        maizo_usb_vid == device->idVendor && maizo_usb_pid == device->idProduct) {
        maizo_usb_stage = MAIZO_USB_CONNECT;
        maizo_usb_reports = maizo_usb_error = 0;
        maizo_usb_report_bits = maizo_usb_packet = 0;
        maizo_usb_class = hid_interface ? hid_interface->bInterfaceClass : 0;
        maizo_usb_endpoints = hid_interface ? hid_interface->bNumEndpoints : 0;
    }

    if (device->idVendor == MAIZOTWIN_VID && device->idProduct == MAIZOTWIN_PID) {
        twin_stage = TWIN_STAGE_CONNECT;
        twin_class = interface->bInterfaceClass;
        twin_endpoints = interface->bNumEndpoints;
        twin_report_counts[0] = twin_report_counts[1] = 0;
        if (twin_root >= 0 || interface->bInterfaceClass != 0x03 ||
            interface->bNumEndpoints == 0) {
            twin_stage = TWIN_STAGE_REJECT_CLASS;
            usb_release(pad);
            return 1;
        }
        ds34pad[pad].type = TWINHID;
        twin_root = pad;
        twin_slots[0] = pad;
        twin_slots[1] = -1;

        /* Reserve one extra logical player slot for the shared endpoint.
         * There is exactly one USB interface, not a second USB device. */
        for (second = 0; second < MAX_PADS; second++) {
            if (second != pad && ds34pad[second].enabled &&
                ds34pad[second].devId == -1) {
                twin_slots[1] = second;
                ds34pad[second].devId = devId;
                ds34pad[second].type = TWINHID;
                ds34pad[second].status = DS34USB_STATE_AUTHORIZED | DS34USB_STATE_CONNECTED;
                break;
            }
        }
        epCount = interface->bNumEndpoints - 1;
    } else if (generic_hid &&
               !(device->idVendor == DS34_VID &&
                 (device->idProduct == DS3_PID || device->idProduct == DS4_PID ||
                  device->idProduct == DS4_PID_SLIM))) {
        ds34pad[pad].type = MAIZOHID;
        ds34pad[pad].hid_interface = hid_interface->bInterfaceNumber;
        epCount = 0; /* Only the matched interrupt-IN descriptor is needed. */
    } else if (device->idVendor == MAIZOHID_VID &&
               device->idProduct == MAIZOHID_PID && !generic_hid) {
        maizo_usb_stage = MAIZO_USB_REJECT_INTERFACE;
        usb_release(pad);
        return 1;
    } else if (device->idProduct == DS3_PID) {
        ds34pad[pad].type = DS3;
        epCount = interface->bNumEndpoints - 1;
    } else {
        ds34pad[pad].type = DS4;
        epCount = 20; // ds4 v2 returns interface->bNumEndpoints as 0
    }

    endpoint = ds34pad[pad].type == MAIZOHID ?
                   hid_endpoint :
                   (UsbEndpointDescriptor *)UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_ENDPOINT);
    if (!endpoint) {
        if (ds34pad[pad].type == MAIZOHID)
            maizo_usb_stage = MAIZO_USB_ENDPOINT_FAIL;
        if (ds34pad[pad].type == TWINHID)
            twin_stage = TWIN_STAGE_ENDPOINT_FAIL;
        usb_release(pad);
        return 1;
    }

    do {
        if (endpoint->bmAttributes == USB_ENDPOINT_XFER_INT) {
            if ((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN && ds34pad[pad].interruptEndp < 0) {
                ds34pad[pad].interruptEndp = UsbOpenEndpointAligned(devId, endpoint);
                if (ds34pad[pad].type == TWINHID) {
                    ds34pad[pad].hid_packet_size = MAIZOTWIN_PACKET;
                } else if (ds34pad[pad].type == MAIZOHID) {
                    unsigned int packet = ((unsigned int)endpoint->wMaxPacketSizeHB << 8) | endpoint->wMaxPacketSizeLB;
                    /* Low-speed HID often uses 8-byte reports: requesting 64
                     * bytes can require eight polls or never complete. */
                    ds34pad[pad].hid_packet_size = (packet > 0 && packet <= MAX_BUFFER_SIZE) ? packet : MAX_BUFFER_SIZE;
                }
                DPRINTF("DS34USB: register Event endpoint id =%i addr=%02X packetSize=%i\n", ds34pad[pad].interruptEndp, endpoint->bEndpointAddress, (unsigned short int)endpoint->wMaxPacketSizeHB << 8 | endpoint->wMaxPacketSizeLB);
            }
            if ((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_OUT && ds34pad[pad].outEndp < 0) {
                ds34pad[pad].outEndp = UsbOpenEndpointAligned(devId, endpoint);
                DPRINTF("DS34USB: register Output endpoint id =%i addr=%02X packetSize=%i\n", ds34pad[pad].outEndp, endpoint->bEndpointAddress, (unsigned short int)endpoint->wMaxPacketSizeHB << 8 | endpoint->wMaxPacketSizeLB);
            }
        }

        endpoint = (UsbEndpointDescriptor *)((char *)endpoint + endpoint->bLength);

    } while (epCount--);

    if (ds34pad[pad].interruptEndp < 0 ||
        ((ds34pad[pad].type != MAIZOHID && ds34pad[pad].type != TWINHID) &&
         ds34pad[pad].outEndp < 0)) {
        if (ds34pad[pad].type == MAIZOHID)
            maizo_usb_stage = MAIZO_USB_ENDPOINT_FAIL;
        if (ds34pad[pad].type == TWINHID)
            twin_stage = TWIN_STAGE_ENDPOINT_FAIL;
        usb_release(pad);
        return 1;
    }

    ds34pad[pad].status |= DS34USB_STATE_CONNECTED;

    {
        int configured = UsbSetDeviceConfiguration(ds34pad[pad].controlEndp,
                                                   config->bConfigurationValue, usb_config_set,
                                                   (void *)pad);
        if (configured != USB_RC_OK) {
            if (ds34pad[pad].type == TWINHID)
                twin_stage = TWIN_STAGE_CONFIG_FAIL;
            if (ds34pad[pad].type == MAIZOHID) {
                maizo_usb_stage = MAIZO_USB_CONFIG_FAIL;
                maizo_usb_error = (u8)configured;
            }
        }
    }
    SignalSema(ds34pad[pad].sema);

    return 0;
}

int usb_disconnect(int devId)
{
    u8 pad;

    DPRINTF("DS34USB: disconnect: devId=%i\n", devId);

    for (pad = 0; pad < MAX_PADS; pad++) {
        if (ds34pad[pad].devId == devId)
            break;
    }

    if (pad < MAX_PADS) {
        if (ds34pad[pad].type == MAIZOHID)
            maizo_usb_stage = MAIZO_USB_DISCONNECTED;
        if (ds34pad[pad].type == TWINHID)
            twin_stage = TWIN_STAGE_DISCONNECTED;
        usb_release(pad);
    }

    return 0;
}

static void usb_release(int pad)
{
    int state;
    CpuSuspendIntr(&state);
    hid_stream_generation[pad]++;
    hid_stream_state[pad] = 0;
    hid_pending[pad] = 0;
    hid_report_ready[pad] = 0;
    CpuResumeIntr(state);
    if (pad == twin_root) {
        int secondary = twin_slots[1];
        twin_root = -1;
        twin_slots[0] = twin_slots[1] = -1;
        if (secondary >= 0 && secondary != pad)
            usb_release(secondary);
    }
    PollSema(ds34pad[pad].sema);

    if (ds34pad[pad].interruptEndp >= 0)
        UsbCloseEndpoint(ds34pad[pad].interruptEndp);

    if (ds34pad[pad].outEndp >= 0)
        UsbCloseEndpoint(ds34pad[pad].outEndp);

    ds34pad[pad].controlEndp = -1;
    ds34pad[pad].interruptEndp = -1;
    ds34pad[pad].outEndp = -1;
    ds34pad[pad].devId = -1;
    ds34pad[pad].hid_packet_size = 0;
    mips_memset(&ds34pad[pad].hid_layout, 0, sizeof(ds34pad[pad].hid_layout));
    ds34pad[pad].status = DS34USB_STATE_DISCONNECTED;

    SignalSema(ds34pad[pad].sema);
}

static int usb_resulCode;
static int usb_rx_bytes;

static void usb_data_cb(int resultCode, int bytes, void *arg)
{
    int pad = (int)arg;

    // DPRINTF("DS34USB: usb_data_cb: res %d, bytes %d, arg %p \n", resultCode, bytes, arg);

    usb_resulCode = resultCode;
    usb_rx_bytes = bytes;

    SignalSema(ds34pad[pad].sema);
}

static void usb_cmd_cb(int resultCode, int bytes, void *arg)
{
    int pad = (int)arg;

    // DPRINTF("DS34USB: usb_cmd_cb: res %d, bytes %d, arg %p \n", resultCode, bytes, arg);

    SignalSema(ds34pad[pad].cmd_sema);
}

static void usb_config_set(int result, int count, void *arg)
{
    int pad = (int)arg;
    u8 led[4] = {0, 0, 0, 0};
    int ret;

    PollSema(ds34pad[pad].sema);
    if (result != USB_RC_OK) {
        if (ds34pad[pad].type == MAIZOHID) {
            maizo_usb_stage = MAIZO_USB_CONFIG_FAIL;
            maizo_usb_error = (u8)result;
        }
        if (ds34pad[pad].type == TWINHID)
            twin_stage = TWIN_STAGE_CONFIG_FAIL;
        SignalSema(ds34pad[pad].sema);
        return;
    }
    ds34pad[pad].status |= DS34USB_STATE_CONFIGURED;

    if (ds34pad[pad].type == TWINHID) {
        twin_stage = TWIN_STAGE_READY;
        ds34pad[pad].status |= DS34USB_STATE_RUNNING;
        if (twin_slots[1] >= 0) {
            ds34pad[twin_slots[1]].status |= DS34USB_STATE_CONFIGURED | DS34USB_STATE_RUNNING;
        }
        SignalSema(ds34pad[pad].sema);
        return;
    }

    if (ds34pad[pad].type == MAIZOHID) {
        unsigned int descriptor_len =
            maizohid_report_length(ds34pad[pad].devId, ds34pad[pad].hid_interface);
        ret = UsbControlTransfer(ds34pad[pad].controlEndp, USB_DIR_IN | USB_RECIP_INTERFACE,
                                 0x06 /* GET_DESCRIPTOR */, 0x2200,
                                 ds34pad[pad].hid_interface, descriptor_len,
                                 hid_report_descriptor[pad], maizo_descriptor_cb, (void *)pad);
        if (ret != USB_RC_OK) {
            maizo_usb_stage = MAIZO_USB_DESCRIPTOR_REQUEST_FAIL;
            maizo_usb_error = (u8)ret;
            DPRINTF("MAIZOHID: descriptor request failed %d\n", ret);
        }
        SignalSema(ds34pad[pad].sema);
        return;
    }

    if (ds34pad[pad].type == DS3) {
        DS3USB_init(pad);
        DelayThread(10000);
        led[0] = led_patterns[pad][1];
        led[3] = 0;
    } else if (ds34pad[pad].type == DS4) {
        led[0] = rgbled_patterns[pad][1][0];
        led[1] = rgbled_patterns[pad][1][1];
        led[2] = rgbled_patterns[pad][1][2];
        led[3] = 0;
    }

    LEDRumble(led, 0, 0, pad);
    DelayThread(20000);

    ds34pad[pad].status |= DS34USB_STATE_RUNNING;

    SignalSema(ds34pad[pad].sema);
}

static void maizo_descriptor_cb(int resultCode, int bytes, void *arg)
{
    int pad = (int)arg;
    if (pad < 0 || pad >= MAX_PADS || ds34pad[pad].type != MAIZOHID || ds34pad[pad].devId < 0)
        return;
    if (resultCode == USB_RC_OK && bytes > 0 &&
        maizohid_parse(&ds34pad[pad].hid_layout, hid_report_descriptor[pad], bytes) &&
        ds34pad[pad].hid_layout.report_bits <= ds34pad[pad].hid_packet_size * 8) {
        ds34pad[pad].status |= DS34USB_STATE_RUNNING;
        maizo_usb_stage = MAIZO_USB_READY;
        maizo_usb_report_bits = ds34pad[pad].hid_layout.report_bits;
        maizo_usb_packet = ds34pad[pad].hid_packet_size;
        DPRINTF("MAIZOHID menu: detected port %d\n", pad);
    } else {
        ds34pad[pad].status &= ~DS34USB_STATE_RUNNING;
        maizo_usb_stage = MAIZO_USB_DESCRIPTOR_PARSE_FAIL;
        maizo_usb_error = (u8)resultCode;
        DPRINTF("MAIZOHID menu: unsupported report descriptor\n");
    }
}

static void DS3USB_init(int pad)
{
    usb_buf[0] = 0x42;
    usb_buf[1] = 0x0c;
    usb_buf[2] = 0x00;
    usb_buf[3] = 0x00;

    UsbControlTransfer(ds34pad[pad].controlEndp, REQ_USB_OUT, USB_REQ_SET_REPORT, (HID_USB_GET_REPORT_FEATURE << 8) | 0xF4, 0, 4, usb_buf, NULL, NULL);
}

static void readReport(u8 *data, int bytes, int pad)
{
    if (ds34pad[pad].type == TWINHID) {
        int player = (bytes >= MAIZOTWIN_PACKET && data[0] >= 1 && data[0] <= 2) ?
                         twin_slots[data[0] - 1] :
                         -1;
        if (player >= 0) {
            u16 oldButtons = ds34pad[player].ds2.nButtonState;
            if (maizotwin_decode(data, bytes, &ds34pad[player].ds2)) {
                hid_report_ready[player] = 1;
                hid_pending[player] |=
                    maizohid_press_edges(oldButtons, ds34pad[player].ds2.nButtonState);
                twin_report_counts[data[0] - 1]++;
            }
        }
        return;
    }
    if (ds34pad[pad].type == MAIZOHID) {
        u16 oldButtons = ds34pad[pad].ds2.nButtonState;
        if (maizohid_decode(&ds34pad[pad].hid_layout, data, bytes, &ds34pad[pad].ds2)) {
            hid_report_ready[pad] = 1;
            hid_pending[pad] |=
                maizohid_press_edges(oldButtons, ds34pad[pad].ds2.nButtonState);
            if (maizo_usb_reports < 255)
                maizo_usb_reports = maizo_usb_reports == 254 ? 1 : maizo_usb_reports + 1;
        }
        return;
    }
    /* Reject truncated Sony reports before interpreting their controls.
     * A newly connected pad must publish a complete report first. */
    if (data[0] && bytes >= 48) {
        hid_report_ready[pad] = 1;

        if (ds34pad[pad].type == DS3) {
            struct ds3report *report;

            report = (struct ds3report *)&data[2];

            ds34pad[pad].data[0] = ~report->ButtonStateL;
            ds34pad[pad].data[1] = ~report->ButtonStateH;

            ds34pad[pad].data[2] = report->RightStickX; // rx
            ds34pad[pad].data[3] = report->RightStickY; // ry
            ds34pad[pad].data[4] = report->LeftStickX;  // lx
            ds34pad[pad].data[5] = report->LeftStickY;  // ly

            ds34pad[pad].data[6] = report->PressureRight; // right
            ds34pad[pad].data[7] = report->PressureLeft;  // left
            ds34pad[pad].data[8] = report->PressureUp;    // up
            ds34pad[pad].data[9] = report->PressureDown;  // down

            ds34pad[pad].data[10] = report->PressureTriangle; // triangle
            ds34pad[pad].data[11] = report->PressureCircle;   // circle
            ds34pad[pad].data[12] = report->PressureCross;    // cross
            ds34pad[pad].data[13] = report->PressureSquare;   // square

            ds34pad[pad].data[14] = report->PressureL1; // L1
            ds34pad[pad].data[15] = report->PressureR1; // R1
            ds34pad[pad].data[16] = report->PressureL2; // L2
            ds34pad[pad].data[17] = report->PressureR2; // R2

            if (report->PSButton && report->Power != 0xEE) // display battery level
                ds34pad[pad].oldled[0] = power_level[report->Power];
            else
                ds34pad[pad].oldled[0] = led_patterns[pad][1];

            if (report->Power == 0xEE) // charging
                ds34pad[pad].oldled[3] = 1;
            else
                ds34pad[pad].oldled[3] = 0;

        } else if (ds34pad[pad].type == DS4) {
            struct ds4report *report;
            u8 up = 0, down = 0, left = 0, right = 0;

            report = (struct ds4report *)data;

            switch (report->Dpad) {
                case 0:
                    up = 1;
                    break;
                case 1:
                    up = 1;
                    right = 1;
                    break;
                case 2:
                    right = 1;
                    break;
                case 3:
                    down = 1;
                    right = 1;
                    break;
                case 4:
                    down = 1;
                    break;
                case 5:
                    down = 1;
                    left = 1;
                    break;
                case 6:
                    left = 1;
                    break;
                case 7:
                    up = 1;
                    left = 1;
                    break;
                case 8:
                    up = 0;
                    down = 0;
                    left = 0;
                    right = 0;
                    break;
            }

            if (report->TPad) {
                if (!report->nFinger1Active) {
                    if (report->Finger1X < 960)
                        report->Share = 1;
                    else
                        report->Option = 1;
                }

                if (!report->nFinger2Active) {
                    if (report->Finger2X < 960)
                        report->Share = 1;
                    else
                        report->Option = 1;
                }
            }

            ds34pad[pad].data[0] = ~(report->Share | report->L3 << 1 | report->R3 << 2 | report->Option << 3 | up << 4 | right << 5 | down << 6 | left << 7);
            ds34pad[pad].data[1] = ~(report->L2 | report->R2 << 1 | report->L1 << 2 | report->R1 << 3 | report->Triangle << 4 | report->Circle << 5 | report->Cross << 6 | report->Square << 7);

            ds34pad[pad].data[2] = report->RightStickX; // rx
            ds34pad[pad].data[3] = report->RightStickY; // ry
            ds34pad[pad].data[4] = report->LeftStickX;  // lx
            ds34pad[pad].data[5] = report->LeftStickY;  // ly

            ds34pad[pad].data[6] = right * 255; // right
            ds34pad[pad].data[7] = left * 255;  // left
            ds34pad[pad].data[8] = up * 255;    // up
            ds34pad[pad].data[9] = down * 255;  // down

            ds34pad[pad].data[10] = report->Triangle * 255; // triangle
            ds34pad[pad].data[11] = report->Circle * 255;   // circle
            ds34pad[pad].data[12] = report->Cross * 255;    // cross
            ds34pad[pad].data[13] = report->Square * 255;   // square

            ds34pad[pad].data[14] = report->L1 * 255;   // L1
            ds34pad[pad].data[15] = report->R1 * 255;   // R1
            ds34pad[pad].data[16] = report->PressureL2; // L2
            ds34pad[pad].data[17] = report->PressureR2; // R2

            if (report->PSButton) { // display battery level
                ds34pad[pad].oldled[0] = report->Battery;
                ds34pad[pad].oldled[1] = 0;
                ds34pad[pad].oldled[2] = 0;
            } else {
                ds34pad[pad].oldled[0] = rgbled_patterns[pad][1][0];
                ds34pad[pad].oldled[1] = rgbled_patterns[pad][1][1];
                ds34pad[pad].oldled[2] = rgbled_patterns[pad][1][2];
            }

            if (report->Power != 0xB && report->Usb_plugged) // charging
                ds34pad[pad].oldled[3] = 1;
            else
                ds34pad[pad].oldled[3] = 0;
        }
    }
}

static int LEDRumble(u8 *led, u8 lrum, u8 rrum, int pad)
{
    int ret = 0;

    if (ds34pad[pad].type == MAIZOHID)
        return USB_RC_OK;

    PollSema(ds34pad[pad].cmd_sema);

    mips_memset(usb_buf, 0, sizeof(usb_buf));

    if (ds34pad[pad].type == DS3) {
        mips_memcpy(usb_buf, output_01_report, sizeof(output_01_report));

        usb_buf[1] = 0xFE; // rt
        usb_buf[2] = rrum; // rp
        usb_buf[3] = 0xFE; // lt
        usb_buf[4] = lrum; // lp

        usb_buf[9] = led[0] & 0x7F; // LED Conf

        if (led[3]) { // means charging, so blink
            usb_buf[13] = 0x32;
            usb_buf[18] = 0x32;
            usb_buf[23] = 0x32;
            usb_buf[28] = 0x32;
        }

        ret = UsbControlTransfer(ds34pad[pad].controlEndp, REQ_USB_OUT, USB_REQ_SET_REPORT, (HID_USB_SET_REPORT_OUTPUT << 8) | 0x01, 0, sizeof(output_01_report), usb_buf, usb_cmd_cb, (void *)pad);
    } else if (ds34pad[pad].type == DS4) {
        usb_buf[0] = 0x05;
        usb_buf[1] = 0xFF;

        usb_buf[4] = rrum; // ds4 has full control
        usb_buf[5] = lrum;

        usb_buf[6] = led[0]; // r
        usb_buf[7] = led[1]; // g
        usb_buf[8] = led[2]; // b

        if (led[3]) {           // means charging, so blink
            usb_buf[9] = 0x80;  // Time to flash bright (255 = 2.5 seconds)
            usb_buf[10] = 0x80; // Time to flash dark (255 = 2.5 seconds)
        }

        ret = UsbInterruptTransfer(ds34pad[pad].outEndp, usb_buf, 32, usb_cmd_cb, (void *)pad);
    }

    ds34pad[pad].oldled[0] = led[0];
    ds34pad[pad].oldled[1] = led[1];
    ds34pad[pad].oldled[2] = led[2];
    ds34pad[pad].oldled[3] = led[3];

    ds34pad[pad].lrum = lrum;
    ds34pad[pad].rrum = rrum;

    return ret;
}

/* USB callbacks publish completion metadata, never start nested
 * transfers. Polling and parsing are outside RPC and the EE frame loop. */
static void hid_stream_callback(int result, int bytes, void *arg)
{
    unsigned int token = (unsigned int)arg;
    int port = token & (MAX_PADS - 1);
    if ((token >> 2) != hid_stream_generation[port])
        return;
    hid_stream_result[port] = result;
    hid_stream_bytes[port] = bytes;
    hid_stream_state[port] = 2;
}

static void hid_stream_thread(void *arg)
{
    int pad;
    (void)arg;
    for (;;) {
        for (pad = 0; pad < MAX_PADS; pad++) {
            ds34usb_device *device = &ds34pad[pad];
            unsigned int token;
            int ret, oldstate, bytes;
            if (!device->enabled || device->devId < 0 ||
                device->interruptEndp < 0 ||
                !(device->status & DS34USB_STATE_RUNNING) ||
                (device->type == TWINHID && pad != twin_root))
                continue;

            if (hid_stream_state[pad] == 2) {
                bytes = hid_stream_bytes[pad];
                if (hid_stream_result[pad] == USB_RC_OK &&
                    bytes > 0 && bytes <= MAX_BUFFER_SIZE) {
                    CpuSuspendIntr(&oldstate);
                    readReport(hid_stream_buffers[pad], bytes, pad);
                    CpuResumeIntr(oldstate);
                }
                hid_stream_state[pad] = 0;
            }
            if (hid_stream_state[pad] != 0)
                continue;
            token = (hid_stream_generation[pad] << 2) | (unsigned int)pad;
            hid_stream_state[pad] = 1;
            ret = UsbInterruptTransfer(device->interruptEndp,
                                       hid_stream_buffers[pad],
                                       (device->type == MAIZOHID ||
                                        device->type == TWINHID) ?
                                           device->hid_packet_size :
                                           MAX_BUFFER_SIZE,
                                       hid_stream_callback, (void *)token);
            if (ret != USB_RC_OK)
                hid_stream_state[pad] = 0;
        }
        DelayThread(2000);
    }
}

static unsigned int timeout(void *arg)
{
    int sema = (int)arg;
    iSignalSema(sema);
    return 0;
}

static void TransferWait(int sema)
{
    iop_sys_clock_t cmd_timeout;

    cmd_timeout.lo = 200000;
    cmd_timeout.hi = 0;

    if (SetAlarm(&cmd_timeout, timeout, (void *)sema) == 0) {
        WaitSema(sema);
        CancelAlarm(timeout, NULL);
    }
}

static int LEDRUM(u8 *led, u8 lrum, u8 rrum, int pad)
{
    int ret = 0;

    if (ds34pad[pad].type == MAIZOHID || ds34pad[pad].type == TWINHID)
        return USB_RC_OK; /* Do not send Sony rumble to generic HID pads. */

    WaitSema(ds34pad[pad].sema);

    if (ds34pad[pad].update_rum) {
        ret = LEDRumble(led, lrum, rrum, pad);

        if (ret == USB_RC_OK)
            TransferWait(ds34pad[pad].cmd_sema);
        else
            DPRINTF("DS34USB: LEDRUM usb transfer error 0x%02X\n", ret);
    }

    SignalSema(ds34pad[pad].sema);

    return ret;
}

static int LED(u8 *led, int pad)
{
    return LEDRUM(led, ds34pad[pad].lrum, ds34pad[pad].rrum, pad);
}

static int Rumble(u8 lrum, u8 rrum, int pad)
{
    return LEDRUM(ds34pad[pad].oldled, lrum, rrum, pad);
}

void ds34usb_set_rumble(u8 lrum, u8 rrum, int port)
{
    if (port >= MAX_PADS)
        return;

    Rumble(lrum, rrum, port);
}

void ds34usb_set_led(u8 *led, int port)
{
    if (port >= MAX_PADS)
        return;

    LED(led, port);
}

/* Atomic one-shot edges: retain a press observed between two calls
 * to PAD_GET_DATA, then release normally on the following game poll. */
static void hid_copy_snapshot(void *dst, int size, int port)
{
    struct ds2report report;
    mips_memcpy(&report, ds34pad[port].data, sizeof(report));
    maizohid_apply_press(&report, hid_pending[port]);
    hid_pending[port] = 0;
    mips_memcpy(dst, &report, size);
}

void ds34usb_get_data(char *dst, int size, int port)
{
    int ret;

    if (port >= MAX_PADS)
        return;

    int reader = (ds34pad[port].type == TWINHID && twin_root >= 0) ?
                     twin_root :
                     port;

    /* A menu frame must never wait for a USB low-speed report. */
    if (hid_stream_thread_id > 0) {
        int oldstate;
        if (!(ds34pad[port].status & DS34USB_STATE_RUNNING))
            return;
        CpuSuspendIntr(&oldstate);
        hid_copy_snapshot(dst, size, port);
        CpuResumeIntr(oldstate);
        return;
    }

    if (ds34pad[reader].interruptEndp < 0)
        return;

    WaitSema(ds34pad[reader].sema);

    PollSema(ds34pad[reader].sema);

    usb_resulCode = 1;
    usb_rx_bytes = 0;
    ret = UsbInterruptTransfer(ds34pad[reader].interruptEndp, usb_buf,
                               (ds34pad[reader].type == MAIZOHID ||
                                ds34pad[reader].type == TWINHID) ?
                                   ds34pad[reader].hid_packet_size :
                                   MAX_BUFFER_SIZE,
                               usb_data_cb, (void *)reader);

    if (ret == USB_RC_OK) {
        TransferWait(ds34pad[reader].sema);
        if (!usb_resulCode && usb_rx_bytes > 0)
            readReport(usb_buf, usb_rx_bytes, reader);

        usb_resulCode = 1;
    } else {
        DPRINTF("DS34USB: ds34usb_get_data usb transfer error %d\n", ret);
    }

    mips_memcpy(dst, ds34pad[port].data, size);

    SignalSema(ds34pad[reader].sema);
}

int ds34usb_get_bdaddr(u8 *data, int port)
{
    int i, ret;

    if (port < 0 || port >= MAX_PADS || (ds34pad[port].type == MAIZOHID || ds34pad[port].type == TWINHID))
        return 0;

    if (port >= MAX_PADS)
        return 0;

    if (ds34pad[port].update_rum) {
        ds34pad[port].update_rum = 0;
        return 0;
    }

    WaitSema(ds34pad[port].sema);

    PollSema(ds34pad[port].cmd_sema);

    if (ds34pad[port].type == DS3) {
        ret = UsbControlTransfer(ds34pad[port].controlEndp, REQ_USB_IN, USB_REQ_GET_REPORT, (HID_USB_GET_REPORT_FEATURE << 8) | 0xF5, 0, 8, usb_buf, usb_cmd_cb, (void *)port);

        if (ret == USB_RC_OK) {
            TransferWait(ds34pad[port].cmd_sema);

            for (i = 0; i < 6; i++)
                data[5 - i] = usb_buf[2 + i];

            ret = 1;
        } else {
            DPRINTF("DS34USB: ds3usb_get_bdaddr usb transfer error %d\n", ret);
            ret = 0;
        }
    } else {
        ret = UsbControlTransfer(ds34pad[port].controlEndp, REQ_USB_IN, USB_REQ_GET_REPORT, (HID_USB_GET_REPORT_FEATURE << 8) | 0x12, 0, 16, usb_buf, usb_cmd_cb, (void *)port);

        if (ret == USB_RC_OK) {
            TransferWait(ds34pad[port].cmd_sema);

            for (i = 0; i < 6; i++)
                data[5 - i] = usb_buf[15 - i];

            ret = 1;
        } else {
            DPRINTF("DS34USB: ds3usb_get_bdaddr usb transfer error %d\n", ret);
            ret = 0;
        }
    }

    ds34pad[port].update_rum = 1;
    SignalSema(ds34pad[port].sema);

    return ret;
}

void ds34usb_set_bdaddr(u8 *data, int port)
{
    int i, ret;

    if (port < 0 || port >= MAX_PADS ||
        ds34pad[port].type == MAIZOHID || ds34pad[port].type == TWINHID)
        return;

    if (port >= MAX_PADS)
        return;

    WaitSema(ds34pad[port].sema);

    PollSema(ds34pad[port].cmd_sema);

    if (ds34pad[port].type == DS3) {
        usb_buf[0] = 0x01;
        usb_buf[1] = 0x00;

        for (i = 0; i < 6; i++)
            usb_buf[i + 2] = data[5 - i];

        ret = UsbControlTransfer(ds34pad[port].controlEndp, REQ_USB_OUT, USB_REQ_SET_REPORT, (HID_USB_GET_REPORT_FEATURE << 8) | 0xF5, 0, 8, usb_buf, usb_cmd_cb, (void *)port);
    } else {
        usb_buf[0] = 0x13;

        for (i = 0; i < 6; i++)
            usb_buf[i + 1] = data[i];

        for (i = 0; i < sizeof(link_key); i++)
            usb_buf[i + 7] = link_key[i];

        ret = UsbControlTransfer(ds34pad[port].controlEndp, REQ_USB_OUT, USB_REQ_SET_REPORT, (HID_USB_GET_REPORT_FEATURE << 8) | 0x13, 0, 24, usb_buf, usb_cmd_cb, (void *)port);
    }

    if (ret == USB_RC_OK)
        TransferWait(ds34pad[port].cmd_sema);
    else
        DPRINTF("DS34USB: ds3usb_set_bdaddr usb transfer error %d\n", ret);

    SignalSema(ds34pad[port].sema);
}

void ds34usb_reset()
{
    int pad;

    for (pad = 0; pad < MAX_PADS; pad++)
        usb_release(pad);
}

int ds34usb_get_status(int port)
{
    int ret;

    if (port >= MAX_PADS)
        return 0;

    WaitSema(ds34pad[port].sema);
    ret = ds34pad[port].status;
    SignalSema(ds34pad[port].sema);

    return ret;
}

void ds34usb_init(u8 pads)
{
    u8 pad;

    for (pad = 0; pad < MAX_PADS; pad++) {
        WaitSema(ds34pad[pad].sema);
        ds34pad[pad].enabled = (pads >> pad) & 1;
        SignalSema(ds34pad[pad].sema);
    }
}

static void rpc_thread(void *data);
static void *rpc_sf(int cmd, void *data, int size);

static SifRpcDataQueue_t rpc_que __attribute__((aligned(16)));
static SifRpcServerData_t rpc_svr __attribute__((aligned(16)));

static int rpc_buf[64] __attribute((aligned(16)));

#define DS34USB_INIT       1
#define DS34USB_GET_STATUS 2
#define DS34USB_GET_BDADDR 3
#define DS34USB_SET_BDADDR 4
#define DS34USB_SET_RUMBLE 5
#define DS34USB_SET_LED    6
#define DS34USB_GET_DATA   7
#define DS34USB_RESET      8

#define DS34USB_TWIN_DIAG   9
#define DS34USB_USB_DIAG    10
#define DS34USB_MENU_STATE  11
#define DS34USB_BIND_RPC_ID 0x18E3878E

void rpc_thread(void *data)
{
    SifInitRpc(0);
    SifSetRpcQueue(&rpc_que, GetThreadId());
    SifRegisterRpc(&rpc_svr, DS34USB_BIND_RPC_ID, rpc_sf, rpc_buf, NULL, NULL, &rpc_que);
    SifRpcLoop(&rpc_que);
}

void *rpc_sf(int cmd, void *data, int size)
{
    switch (cmd) {
        case DS34USB_INIT:
            ds34usb_init(*(u8 *)data);
            break;
        case DS34USB_GET_STATUS:
            *(u8 *)data = ds34usb_get_status(*(u8 *)data);
            break;
        case DS34USB_GET_BDADDR:
            *(u8 *)data = ds34usb_get_bdaddr(((u8 *)data + 1), *(u8 *)data);
            break;
        case DS34USB_SET_BDADDR:
            ds34usb_set_bdaddr(((u8 *)data + 1), *(u8 *)data);
            break;
        case DS34USB_SET_RUMBLE:
            ds34usb_set_rumble(*((u8 *)data + 1), *((u8 *)data + 2), *(u8 *)data);
            break;
        case DS34USB_SET_LED:
            ds34usb_set_led(((u8 *)data + 1), *(u8 *)data);
            break;
        case DS34USB_GET_DATA:
            ds34usb_get_data((char *)data, 18, *(u8 *)data);
            break;
        case DS34USB_MENU_STATE: {
            u8 *snapshot = (u8 *)data;
            int port;
            /* One stateless 16-byte reply. No UsbInterruptTransfer waits in
             * the GUI RPC path, no stale stack report and no phantom P3/P4.
             * Only mark a slot valid after a real running device has
             * published a complete DS2 state. */
            mips_memset(snapshot, 0, MAIZO_MENU_SNAPSHOT_SIZE);
            snapshot[8] = MAIZO_MENU_MAGIC0;
            snapshot[9] = MAIZO_MENU_MAGIC1;
            snapshot[10] = MAIZO_MENU_VERSION;
            for (port = 0; port < MAX_PADS; port++) {
                unsigned int pos = port * 4;
                int oldstate;
                snapshot[pos] = snapshot[pos + 1] = 0xff;
                snapshot[pos + 2] = snapshot[pos + 3] = 0x7f;
                if (!ds34pad[port].enabled ||
                    ds34pad[port].devId < 0 || !hid_report_ready[port] ||
                    !(ds34pad[port].status & DS34USB_STATE_RUNNING))
                    continue;
                CpuSuspendIntr(&oldstate);
                /* Deliver one observed short USB press once per EE poll. */
                {
                    u8 report[18];
                    hid_copy_snapshot(report, sizeof(report), port);
                    snapshot[pos] = report[0];
                    snapshot[pos + 1] = report[1];
                    snapshot[pos + 2] = report[4];
                    snapshot[pos + 3] = report[5];
                }
                CpuResumeIntr(oldstate);
                snapshot[11] |= 1u << port;
            }
            break;
        }
        case DS34USB_RESET:
            ds34usb_reset();
            break;
        case DS34USB_USB_DIAG:
            mips_memset(data, 0, 16);
            ((u8 *)data)[0] = maizo_usb_stage;
            ((u8 *)data)[1] = maizo_usb_vid & 0xFF;
            ((u8 *)data)[2] = maizo_usb_vid >> 8;
            ((u8 *)data)[3] = maizo_usb_pid & 0xFF;
            ((u8 *)data)[4] = maizo_usb_pid >> 8;
            ((u8 *)data)[5] = maizo_usb_class;
            ((u8 *)data)[6] = maizo_usb_endpoints;
            ((u8 *)data)[7] = maizo_usb_packet & 0xFF;
            ((u8 *)data)[8] = maizo_usb_packet >> 8;
            ((u8 *)data)[9] = maizo_usb_reports;
            ((u8 *)data)[10] = maizo_usb_report_bits & 0xFF;
            ((u8 *)data)[11] = maizo_usb_report_bits >> 8;
            ((u8 *)data)[12] = maizo_usb_error;
            break;
        case DS34USB_TWIN_DIAG:
            mips_memset(data, 0, 8);
            ((u8 *)data)[0] = twin_stage;
            ((u8 *)data)[1] = twin_class;
            ((u8 *)data)[2] = twin_endpoints;
            ((u8 *)data)[3] = twin_report_counts[0];
            ((u8 *)data)[4] = twin_report_counts[1];
            ((u8 *)data)[5] = twin_root >= 0 ? (u8)ds34pad[twin_root].status : 0;
            break;
        default:
            break;
    }

    return data;
}

int _start(int argc, char *argv[])
{
    int pad;
    u8 enable = 0xFF;

    if (argc > 1) {
        enable = argv[1][0];
    }

    for (pad = 0; pad < MAX_PADS; pad++) {
        ds34pad[pad].status = 0;
        ds34pad[pad].devId = -1;
        ds34pad[pad].oldled[0] = 0;
        ds34pad[pad].oldled[1] = 0;
        ds34pad[pad].oldled[2] = 0;
        ds34pad[pad].oldled[3] = 0;
        ds34pad[pad].lrum = 0;
        ds34pad[pad].rrum = 0;
        ds34pad[pad].update_rum = 1;
        ds34pad[pad].sema = -1;
        ds34pad[pad].cmd_sema = -1;
        ds34pad[pad].controlEndp = -1;
        ds34pad[pad].interruptEndp = -1;
        ds34pad[pad].outEndp = -1;
        ds34pad[pad].enabled = (enable >> pad) & 1;
        ds34pad[pad].type = 0;
        hid_report_ready[pad] = 0;

        ds34pad[pad].data[0] = 0xFF;
        ds34pad[pad].data[1] = 0xFF;

        mips_memset(&ds34pad[pad].data[2], 0x7F, 4);
        mips_memset(&ds34pad[pad].data[6], 0x00, 12);

        ds34pad[pad].sema = CreateMutex(IOP_MUTEX_UNLOCKED);
        ds34pad[pad].cmd_sema = CreateMutex(IOP_MUTEX_UNLOCKED);

        if (ds34pad[pad].sema < 0 || ds34pad[pad].cmd_sema < 0) {
            DPRINTF("DS34USB: Failed to allocate I/O semaphore.\n");
            return MODULE_NO_RESIDENT_END;
        }
    }

    if (UsbRegisterDriver(&usb_driver) != USB_RC_OK) {
        DPRINTF("DS34USB: Error registering USB devices\n");
        return MODULE_NO_RESIDENT_END;
    }

    {
        iop_thread_t stream_th;
        stream_th.attr = TH_C;
        stream_th.thread = hid_stream_thread;
        stream_th.priority = 42;
        stream_th.stacksize = 0x1000;
        stream_th.option = 0;
        hid_stream_thread_id = CreateThread(&stream_th);
        if (hid_stream_thread_id > 0 &&
            StartThread(hid_stream_thread_id, NULL) < 0) {
            DeleteThread(hid_stream_thread_id);
            hid_stream_thread_id = -1;
        }
    }

    iop_thread_t rpc_th;

    rpc_th.attr = TH_C;
    rpc_th.thread = rpc_thread;
    rpc_th.priority = 40;
    rpc_th.stacksize = 0x800;
    rpc_th.option = 0;

    int thid = CreateThread(&rpc_th);

    if (thid > 0) {
        StartThread(thid, NULL);
        return MODULE_RESIDENT_END;
    }

    return MODULE_NO_RESIDENT_END;
}
