/*
  Copyright 2009, Ifcaro, Volca
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#include "include/opl.h"
#include "include/pad.h"
#include "include/ioman.h"
#include "include/gui.h"
#include <libpad.h>
#include "include/maizo_menu_input.h"
#include <timer.h>
#include <time.h>

#ifdef PADEMU
#include <libds34bt.h>
#include <libds34usb.h>
#endif

#define MAX_PADS 4

// Cpu ticks per one milisecond
#define CLOCKS_PER_MILISEC 147456

// 200 ms per repeat
#define DEFAULT_PAD_DELAY 200

struct pad_data_t
{
    int port, slot;
    int state;
    int notified;
    u32 paddata;
    u32 oldpaddata;
    struct padButtonStatus buttons;

    // pad_dma_buf is provided by the user, one buf for each pad
    // contains the pad's current state
    char padBuf[256] __attribute__((aligned(64)));

    char actAlign[6];
    int actuators;
};

/// current time in miliseconds (last update time)
static u32 curtime = 0;
static u32 time_since_last = 0;

static unsigned short pad_count;
#ifdef PADEMU
/* USB input is accepted only from a validated driver snapshot.
 * Bluetooth retains its neutral-report safety gate. */
static unsigned int maizoUsbArmed, maizoUsbCentered;
static unsigned int maizoUsbNotified, maizoBtNotified;
static unsigned char maizoUsbType[2], maizoBtType[2];
static unsigned char maizoUsbMissing[2], maizoBtMissing[2];
static u8 maizoUsbCenters[4] = {127, 127, 127, 127};
static unsigned int maizoBtArmed;
static int maizoPhysicalRescueMode;
static int maizoRescueHeld;
static u32 maizoRescueStart;
#endif
static struct pad_data_t pad_data[MAX_PADS];

// gathered pad data
static u32 paddata;
static u32 oldpaddata;

static int delaycnt[16];
static int paddelay[16];

// KEY_ to PAD_ conversion table
static const int keyToPad[17] = {
    -1,
    PAD_LEFT,
    PAD_DOWN,
    PAD_RIGHT,
    PAD_UP,
    PAD_START,
    PAD_R3,
    PAD_L3,
    PAD_SELECT,
    PAD_SQUARE,
    PAD_CROSS,
    PAD_CIRCLE,
    PAD_TRIANGLE,
    PAD_R1,
    PAD_L1,
    PAD_R2,
    PAD_L2};

/*
 * waitPadReady()
 */
static int waitPadReady(struct pad_data_t *pad)
{
    int state;

    // busy wait for the pad to get ready
    do {
        state = padGetState(pad->port, pad->slot);
    } while ((state != PAD_STATE_STABLE) && (state != PAD_STATE_FINDCTP1) && (state != PAD_STATE_DISCONN));

    return state;
};

static int initializePad(struct pad_data_t *pad)
{
    int tmp;
    int modes;
    int i;

    LOG("PAD initializing pad %d,%d\n", pad->port, pad->slot);

    // is there any device connected to that port?
    if (waitPadReady(pad) == PAD_STATE_DISCONN) {
        LOG("PAD pad %d,%d not connected.\n", pad->port, pad->slot);
        return 1; // nope, don't waste your time here!
    }

    // How many different modes can this device operate in?
    // i.e. get # entrys in the modetable
    modes = padInfoMode(pad->port, pad->slot, PAD_MODETABLE, -1);
    LOG("PAD The device has %d modes: ", modes);

    if (modes > 0) {
        LOG("( ");

        for (i = 0; i < modes; i++) {
            tmp = padInfoMode(pad->port, pad->slot, PAD_MODETABLE, i);
            LOG("%d ", tmp);
        }

        LOG(")\n");
    }

    tmp = padInfoMode(pad->port, pad->slot, PAD_MODECURID, 0);
    LOG("PAD It is currently using mode %d\n", tmp);

    // If modes == 0, this is not a Dual shock controller
    // (it has no actuator engines)
    if (modes == 0) {
        LOG("PAD This is a digital controller?\n");
        return 1;
    }

    // Verify that the controller has a DUAL SHOCK mode
    i = 0;
    do {
        if (padInfoMode(pad->port, pad->slot, PAD_MODETABLE, i) == PAD_TYPE_DUALSHOCK)
            break;
        i++;
    } while (i < modes);

    if (i >= modes) {
        LOG("PAD This is no Dual Shock controller\n");
        return 1;
    }

    // If ExId != 0x0 => This controller has actuator engines
    // This check should always pass if the Dual Shock test above passed
    tmp = padInfoMode(pad->port, pad->slot, PAD_MODECUREXID, 0);
    if (tmp == 0) {
        LOG("PAD This is no Dual Shock controller??\n");
        return 1;
    }

    LOG("PAD Enabling dual shock functions\n");

    // When using MMODE_LOCK, user cant change mode with Select button
    padSetMainMode(pad->port, pad->slot, PAD_MMODE_DUALSHOCK, PAD_MMODE_LOCK);

    waitPadReady(pad);
    tmp = padInfoPressMode(pad->port, pad->slot);
    LOG("PAD infoPressMode: %d\n", tmp);

    waitPadReady(pad);
    tmp = padEnterPressMode(pad->port, pad->slot);
    LOG("PAD enterPressMode: %d\n", tmp);

    waitPadReady(pad);
    pad->actuators = padInfoAct(pad->port, pad->slot, -1, 0);
    LOG("PAD # of actuators: %d\n", pad->actuators);

    if (pad->actuators != 0) {
        pad->actAlign[0] = 0; // Enable small engine
        pad->actAlign[1] = 1; // Enable big engine
        pad->actAlign[2] = 0xff;
        pad->actAlign[3] = 0xff;
        pad->actAlign[4] = 0xff;
        pad->actAlign[5] = 0xff;

        waitPadReady(pad);
        tmp = padSetActAlign(pad->port, pad->slot, pad->actAlign);
        LOG("PAD padSetActAlign: %d\n", tmp);
    } else {
        LOG("PAD Did not find any actuators.\n");
    }

    waitPadReady(pad);

    return 1;
}

static void updatePadState(struct pad_data_t *pad, int state)
{ // To simplify processing, monitor only Disconnected, FindCTP1 & Stable states.
    if ((state == PAD_STATE_DISCONN) || (state == PAD_STATE_STABLE) || (state == PAD_STATE_FINDCTP1))
        pad->state = state;
}

static u32 readLeftJoy(struct pad_data_t *pad, u32 pdata)
{
    u32 padData = pdata;
    int xDeadzone, yDeadzone;

    if ((pad->buttons.mode >> 4) == 0x07) {
        switch (gXSensitivity) {
            case 0:
                xDeadzone = 100;
                break;
            case 1:
                xDeadzone = 80;
                break;
            case 2:
                xDeadzone = 60;
                break;
            default:
                xDeadzone = 80;
        }

        switch (gYSensitivity) {
            case 0:
                yDeadzone = 100;
                break;
            case 1:
                yDeadzone = 80;
                break;
            case 2:
                yDeadzone = 60;
                break;
            default:
                yDeadzone = 80;
        }

        if (pad->buttons.ljoy_h < 127 - xDeadzone)
            padData |= PAD_LEFT;
        else if (pad->buttons.ljoy_h > 127 + xDeadzone)
            padData |= PAD_RIGHT;

        if (pad->buttons.ljoy_v < 127 - yDeadzone)
            padData |= PAD_UP;
        else if (pad->buttons.ljoy_v > 127 + yDeadzone)
            padData |= PAD_DOWN;
    }

    return padData;
}

static int readPad(struct pad_data_t *pad)
{
    int rcode = 0, oldState, newState, ret, padsRead;
    u32 newpdata = 0;

    padsRead = 0;
    oldState = pad->state;
    newState = padGetState(pad->port, pad->slot);
    updatePadState(pad, newState);
    if ((oldState == PAD_STATE_DISCONN) && ((pad->state == PAD_STATE_STABLE) || (pad->state == PAD_STATE_FINDCTP1))) {
        // Pad just connected.
        LOG("PAD pad %d,%d connected\n", pad->port, pad->slot);
        initializePad(pad);
    }
    // The pad may transit from any state to disconnected. So check only for the disconnected state.
    else if ((oldState != PAD_STATE_DISCONN) && (pad->state == PAD_STATE_DISCONN)) {
        LOG("PAD pad %d,%d disconnected\n", pad->port, pad->slot);
        pad->notified = 0;
    }

    if ((pad->state == PAD_STATE_STABLE) || (pad->state == PAD_STATE_FINDCTP1)) {
        // pad is connected. Read pad button information.
        ret = padRead(pad->port, pad->slot, &pad->buttons); // port, slot, buttons

        if (ret != 0 && pad->buttons.btns != 0) {
            if (!pad->notified) {
                guiNotifyController("Controle PS2", pad->port + 1);
                pad->notified = 1;
            }
            newpdata = 0xffff ^ pad->buttons.btns;
            padsRead++;
        }
    }

    /* USB and Bluetooth controllers are polled once per GUI frame after
     * all physical pads. Their lifetime must NOT depend on padPortOpen()
     * nor on whether a physical DS2 happens to occupy slot P1/P2. */
    if (padsRead > 0) {
        if (newpdata != 0x0) // something
            rcode = 1;
        else
            rcode = 0;

        newpdata |= readLeftJoy(pad, newpdata);

        pad->oldpaddata = pad->paddata;
        pad->paddata = newpdata;

        // merge into the global vars
        paddata |= pad->paddata;
    }

    return rcode;
}

/** Returns delay (in miliseconds) specified for the given key.
 * @param id The button id
 * @param repeat Boolean value specifying if we want initial key delay (0) or the repeat key delay (1)
 * @return the delay to the next key event
 */
static int getKeyDelay(int id, int repeat)
{
    int delay = paddelay[id - 1];

    // if not in repeat, the delay is enlarged
    if (!repeat)
        delay *= 3;

    return delay;
}

#ifdef PADEMU
/* The compact snapshot is preferred, but the original per-port RPC is a
 * separate recovery route when the snapshot transport is unavailable. */
static int maizoReadUsbSnapshot(u8 snapshot[MAIZO_MENU_SNAPSHOT_SIZE])
{
    if (ds34usb_get_menu_state(snapshot) &&
        snapshot[8] == MAIZO_MENU_MAGIC0 && snapshot[9] == MAIZO_MENU_MAGIC1 &&
        snapshot[10] == MAIZO_MENU_VERSION)
        return 1;

    for (int i = 0; i < MAIZO_MENU_SNAPSHOT_SIZE; i++)
        snapshot[i] = 0;
    snapshot[8] = MAIZO_MENU_MAGIC0;
    snapshot[9] = MAIZO_MENU_MAGIC1;
    snapshot[10] = MAIZO_MENU_VERSION;
    for (int port = 0; port < MAIZO_MENU_SLOTS; port++) {
        u8 report[18] = {0};
        int pos = port * 4;
        if ((ds34usb_get_status(port) & DS34USB_STATE_RUNNING) &&
            ds34usb_get_data(port, report) && (report[0] || report[1])) {
            snapshot[pos] = report[0];
            snapshot[pos + 1] = report[1];
            snapshot[pos + 2] = report[4];
            snapshot[pos + 3] = report[5];
            snapshot[11] |= 1u << port;
            snapshot[12] |= 1u << port;
        }
    }
    return snapshot[11] != 0;
}

/* The untrusted USB/BT path never writes over physical libpad state. */
static const char *maizoPadTypeName(unsigned int type, int bt)
{
    static const char *const names[] = {
        "USB", "DualShock 3 USB", "DualShock 4 USB", "Guitarra USB",
        "Guitarra USB", "USB HID", "Twin USB", "Xbox 360 USB", "Teclado USB"};
    if (bt)
        return type == 1 ? "DualShock 3 Bluetooth" : type == 2 ? "DualShock 4 Bluetooth" :
                                                                 "Bluetooth";
    return names[type < sizeof(names) / sizeof(names[0]) ? type : 0];
}

static void maizoNoticeSlot(int slot, unsigned int type, int connected, int bt)
{
    unsigned int bit = 1u << slot;
    unsigned int *known = bt ? &maizoBtNotified : &maizoUsbNotified;
    unsigned char *types = bt ? maizoBtType : maizoUsbType;
    unsigned char *missing = bt ? maizoBtMissing : maizoUsbMissing;
    if (connected) {
        missing[slot] = 0;
        if (!(*known & bit) || (type && types[slot] && type != types[slot])) {
            guiNotifyController(maizoPadTypeName(type, bt), slot + 1);
            *known |= bit;
        }
        if (type || !types[slot])
            types[slot] = type;
    } else if (*known & bit) {
        if (++missing[slot] >= 15) {
            guiNotifyControllerDisconnected(maizoPadTypeName(types[slot], bt), slot + 1);
            *known &= ~bit;
            types[slot] = missing[slot] = 0;
        }
    }
}

static int maizoReadRemotePads(void)
{
    int rslt = 0;
    int i;

    /* Do not let an uninitialised, disconnected or version-mismatched
     * USB RPC snapshot synthesize 0xffff pressed buttons. It previously
     * locked all physical controllers out while scrolling DOWN forever. */
    {
        u8 snapshot[MAIZO_MENU_SNAPSHOT_SIZE];
        if (maizoReadUsbSnapshot(snapshot)) {
            unsigned int valid = maizoMenuPadValidMask(snapshot);
            u32 remote;
            unsigned int neutral = maizoMenuPadNeutralMask(snapshot);
            unsigned int fresh;
            for (unsigned int slot = 0; slot < MAIZO_MENU_SLOTS; slot++) {
                unsigned int type = maizoMenuPadType(snapshot, slot);
                maizoNoticeSlot(slot, type, type || (valid & (1u << slot)), 0);
            }
            for (int slot = 0; slot < MAIZO_MENU_SLOTS; ++slot) {
                if (!(valid & (1u << slot))) {
                    maizoUsbCenters[slot * 2] = 127;
                    maizoUsbCenters[slot * 2 + 1] = 127;
                }
            }
            maizoUsbArmed &= valid;
            maizoUsbCentered &= valid;
            fresh = neutral & snapshot[12] & ~maizoUsbCentered;
            for (int slot = 0; slot < MAIZO_MENU_SLOTS; ++slot) {
                if (fresh & (1u << slot)) {
                    maizoUsbCenters[slot * 2] = snapshot[slot * 4 + 2];
                    maizoUsbCenters[slot * 2 + 1] = snapshot[slot * 4 + 3];
                }
            }
            maizoUsbCentered |= fresh;
            /* A validated decoded report is usable immediately. */
            maizoUsbArmed |= valid;
            remote = maizoMenuPadMask(snapshot, gXSensitivity,
                                      gYSensitivity, maizoUsbArmed,
                                      maizoUsbCenters);
            paddata |= remote;
            if (remote)
                rslt = 1;
        } else {
            maizoUsbArmed = 0;
            maizoUsbCentered = 0;
            for (int slot = 0; slot < MAIZO_MENU_SLOTS; slot++)
                maizoNoticeSlot(slot, 0, 0, 0);
            for (int slot = 0; slot < MAIZO_MENU_SLOTS * 2; slot++)
                maizoUsbCenters[slot] = 127;
        }
    }

    /* Bluetooth has exactly two real IOP slots. A stale/empty RPC
     * response must not generate every PS2 button simultaneously. */
    for (i = 0; i < 2; ++i) {
        unsigned int bit = 1u << i;
        if (ds34bt_get_status(i) & DS34BT_STATE_RUNNING) {
            u8 report[18];
            maizoNoticeSlot(i, (maizoBtNotified & bit) ? maizoBtType[i] : ds34bt_get_pad_type(i), 1, 1);
            if (ds34bt_get_data(i, report)) {
                u16 buttons = report[0] | ((u16)report[1] << 8);
                if (buttons == 0)
                    continue;
                if (buttons == 0xffff &&
                    report[4] >= 105 && report[4] <= 150 &&
                    report[5] >= 105 && report[5] <= 150)
                    maizoBtArmed |= bit;
                if (maizoBtArmed & bit) {
                    u32 remote = 0xffffu ^ buttons;
                    paddata |= remote;
                    if (remote)
                        rslt = 1;
                }
            }
        } else {
            maizoBtArmed &= ~bit;
            maizoNoticeSlot(i, 0, 0, 1);
        }
    }
    return rslt;
}
#endif

/** polling method. Call every frame. */
unsigned int maizoMenuConnectedPads(void)
{
#ifdef PADEMU
    return maizoUsbNotified | (maizoBtNotified << 2);
#else
    return 0;
#endif
}

int readPads()
{
    int i;
    oldpaddata = paddata;
    paddata = 0;

    // in ms.
    u32 newtime = cpu_ticks() / CLOCKS_PER_MILISEC;
    time_since_last = newtime - curtime;
    curtime = newtime;

    int rslt = 0;

    for (i = 0; i < pad_count; ++i) {
        rslt |= readPad(&pad_data[i]);
    }

#ifdef PADEMU
    /* Native-only rescue: hold the chord for 1.5 seconds to toggle.
     * An ordinary shortcut or transient report cannot permanently disable USB. */
    if ((paddata & (PAD_L1 | PAD_R1 | PAD_START)) ==
        (PAD_L1 | PAD_R1 | PAD_START)) {
        if (!maizoRescueHeld) {
            maizoRescueStart = curtime;
            maizoRescueHeld = 1;
        } else if (maizoRescueHeld == 1 && curtime - maizoRescueStart >= 1500) {
            maizoPhysicalRescueMode = !maizoPhysicalRescueMode;
            maizoRescueHeld = 2;
        }
    } else {
        maizoRescueHeld = 0;
    }
    /* Always poll remote ports, even in native rescue. Suppress only their
     * input in rescue mode, never their connection notifications. */
    if (!maizoPhysicalRescueMode)
        rslt |= maizoReadRemotePads();
    else {
        u32 native = paddata;
        maizoReadRemotePads();
        paddata = native;
    }
#endif

    for (i = 0; i < 16; ++i) {
        if (getKeyPressed(i + 1))
            delaycnt[i] -= time_since_last;
        else
            delaycnt[i] = getKeyDelay(i + 1, 0);
    }

    return rslt;
}

/** Key getter with key repeats.
 * @param id The button ID
 * @return nonzero if button is being pressed just now
 */
int getKey(int id)
{
    if ((id <= 0) || (id >= 17))
        return 0;

    int kid = id - 1;

    // either the button was not pressed this frame, then reset counter and return
    // or it was, then handle the repetition
    if (getKeyOn(id)) {
        delaycnt[kid] = getKeyDelay(id, 0);
        KeyPressedOnce = 1;
        DisableCron = 1;
        return 1;
    }

    if (!getKeyPressed(id))
        return 0;

    if (delaycnt[kid] <= 0) {
        delaycnt[kid] = getKeyDelay(id, 1);
        KeyPressedOnce = 1;
        DisableCron = 1;
        return 1;
    }

    return 0;
}

/** Detects key-on event. Returns true if the button was not pressed the last frame but is pressed this frame.
 * @param id The button ID
 * @return nonzero if button is being pressed just now
 */
int getKeyOn(int id)
{
    if ((id <= 0) || (id >= 17))
        return 0;

    // old v.s. new pad data
    int keyid = keyToPad[id];

    return (paddata & keyid) && (!(oldpaddata & keyid));
}

/** Detects key-off event. Returns true if the button was pressed the last frame but is not pressed this frame.
 * @param id The button ID
 * @return nonzero if button is being released
 */
int getKeyOff(int id)
{
    if ((id <= 0) || (id >= 17))
        return 0;

    // old v.s. new pad data
    int keyid = keyToPad[id];

    return (!(paddata & keyid)) && (oldpaddata & keyid);
}

/** Returns true (nonzero) if the button is currently pressed
 * @param id The button ID
 * @return nonzero if button is being held
 */
int getKeyPressed(int id)
{
    // old v.s. new pad data
    int keyid = keyToPad[id];

    return (paddata & keyid);
}

/** Sets the delay to wait for button repetition event to occur.
 * @param button The button ID
 * @param btndelay The button delay (in query count)
 */
void setButtonDelay(int button, int btndelay)
{
    if ((button <= 0) || (button >= 17))
        return;

    paddelay[button - 1] = btndelay;
}

int getButtonDelay(int button)
{
    if ((button <= 0) || (button >= 17))
        return 0;

    return paddelay[button - 1];
}

/** Unloads a single pad.
 * @see unloadPads */
static void unloadPad(struct pad_data_t *pad)
{
    padPortClose(pad->port, pad->slot);
}

/** Unloads all pads. Use to terminate the usage of the pads. */
void unloadPads()
{
    int i;

    for (i = 0; i < pad_count; ++i)
        unloadPad(&pad_data[i]);

    padEnd();
}

/** Tries to start a single pad.
 * @param pad The pad data holding structure
 * @return 0 Error, != 0 Ok */
static int startPad(struct pad_data_t *pad)
{
    int newState;

    if (padPortOpen(pad->port, pad->slot, pad->padBuf) == 0) {
        return 0;
    }

    if (!initializePad(pad)) {
        return 0;
    }

    newState = waitPadReady(pad);
    updatePadState(pad, newState);
    return 1;
}

/** Starts all pads.
 * @return Count of dual shock compatible pads. 0 if none present. */
int startPads()
{
    // scan for pads that exist... at least one has to be present
    pad_count = 0;
#ifdef PADEMU
    maizoUsbArmed = 0;
    maizoUsbCentered = 0;
    maizoBtArmed = 0;
    maizoUsbNotified = 0;
    maizoBtNotified = 0;
#endif

    int maxports = padGetPortMax();

    int port; // 0 -> Connector 1, 1 -> Connector 2
    int slot; // Always zero if not using multitap

    for (port = 0; port < maxports; ++port) {
        int maxslots = padGetSlotMax(port);

        for (slot = 0; slot < maxslots && pad_count < MAX_PADS; ++slot) {

            struct pad_data_t *cpad = &pad_data[pad_count];

            cpad->port = port;
            cpad->slot = slot;
            cpad->state = PAD_STATE_DISCONN;
            cpad->notified = 0;

            if (startPad(cpad))
                ++pad_count;
        }

        if (pad_count >= MAX_PADS)
            break; // enough already!
    }

    int n;
    for (n = 0; n < 16; ++n) {
        delaycnt[n] = DEFAULT_PAD_DELAY;
        paddelay[n] = DEFAULT_PAD_DELAY;
    }

    return pad_count;
}

void padStoreSettings(int *buffer)
{
    int i;

    for (i = 0; i < 16; i++)
        buffer[i] = paddelay[i];
}


void padRestoreSettings(int *buffer)
{
    int i;

    for (i = 0; i < 16; i++)
        paddelay[i] = buffer[i];
}

int maizoIsPhysicalRescueMode(void)
{
#ifdef PADEMU
    return maizoPhysicalRescueMode;
#else
    return 0;
#endif
}
