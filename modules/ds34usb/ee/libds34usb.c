#include <kernel.h>
#include <sifrpc.h>
#include <string.h>

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
#define DS34USB_PROFILES    12
#define DS34USB_CAPTURE     13
#define DS34USB_MENU_PORT   14
#define DS34USB_BIND_RPC_ID 0x18E3878E

static SifRpcClientData_t ds34usb __attribute__((aligned(64)));
/* Match the original OPL DS34 transport: one client and one aligned,
 * in-place DMA staging buffer, serialized across menu and diagnostics. */
static u8 rpcbuf[64] __attribute__((aligned(64)));
static int rpc_lock = -1;
static u8 ds34usb_inited;

static int bind_client(SifRpcClientData_t *client)
{
    int tries;
    client->server = NULL;
    for (tries = 0; tries < 16; tries++) {
        if (SifBindRpc(client, DS34USB_BIND_RPC_ID, 0) < 0) {
            nopdelay();
            continue;
        }
        if (client->server)
            return 1;
        nopdelay();
    }
    return 0;
}

int ds34usb_init()
{
    if (rpc_lock < 0) {
        ee_sema_t sema;
        memset(&sema, 0, sizeof(sema));
        sema.init_count = 1;
        sema.max_count = 1;
        rpc_lock = CreateSema(&sema);
    }
    if (rpc_lock < 0)
        return 0;
    if (PollSema(rpc_lock) < 0)
        return 0;
    if (!ds34usb_inited)
        ds34usb_inited = bind_client(&ds34usb);
    SignalSema(rpc_lock);
    return ds34usb_inited;
}

int ds34usb_deinit()
{
    ds34usb_inited = 0;
    return 1;
}

/* Use the known OPL in-place RPC contract. The uncached alias observes
 * the actual completed IOP DMA, not a cached copy of the zeroed buffer. */
static int rpc_transaction(int cmd, const void *input, int input_size,
                           void *output, int output_size, int menu)
{
    int ok;
    if (rpc_lock < 0 || input_size < 0 || input_size > (int)sizeof(rpcbuf) ||
        output_size < 0 || output_size > (int)sizeof(rpcbuf))
        return 0;
    if ((menu ? PollSema(rpc_lock) : WaitSema(rpc_lock)) < 0)
        return 0;
    if (!ds34usb_inited) {
        SignalSema(rpc_lock);
        return 0;
    }
    memset(rpcbuf, 0, sizeof(rpcbuf));
    if (input_size)
        memcpy(rpcbuf, input, input_size);
    ok = SifCallRpc(&ds34usb, cmd, 0, input_size ? rpcbuf : NULL, input_size,
                    output_size ? rpcbuf : NULL, output_size, NULL, NULL) == 0;
    if (ok && output_size)
        memcpy(output, UNCACHED_SEG(rpcbuf), output_size);
    if (!ok)
        ds34usb_inited = 0;
    SignalSema(rpc_lock);
    return ok;
}

int ds34usb_reinit_ports(u8 ports)
{
    return rpc_transaction(DS34USB_INIT, &ports, 1, NULL, 0, 1);
}

int ds34usb_get_usb_diag(u8 info[16])
{
    return rpc_transaction(DS34USB_USB_DIAG, NULL, 0, info, 16, 0);
}

int ds34usb_get_twin_diag(u8 info[8])
{
    return rpc_transaction(DS34USB_TWIN_DIAG, NULL, 0, info, 8, 0);
}

int ds34usb_set_profiles(const u8 profiles[32])
{
    return profiles && rpc_transaction(DS34USB_PROFILES, profiles, 32, NULL, 0, 0);
}

int ds34usb_get_capture(u8 out[40])
{
    return out && rpc_transaction(DS34USB_CAPTURE, NULL, 0, out, 40, 1);
}

int ds34usb_get_menu_state(u8 out[16])
{
    const u8 request[16] = {0x4d, 0x5a};
    return out && rpc_transaction(DS34USB_MENU_STATE, request, 16, out, 16, 1);
}

int ds34usb_get_menu_port(int port, u8 out[32])
{
    const u8 request[4] = {0x4d, 0x50, 1, port};
    return out && port >= 0 && port < 2 &&
           rpc_transaction(DS34USB_MENU_PORT, request, sizeof(request), out, 32, 1);
}

int ds34usb_get_status_checked(int port, u8 *status)
{
    u8 input = port;
    return status && port >= 0 && port < 2 &&
           rpc_transaction(DS34USB_GET_STATUS, &input, 1, status, 1, 1);
}

int ds34usb_get_status(int port)
{
    u8 status = 0;
    ds34usb_get_status_checked(port, &status);
    return status;
}

int ds34usb_get_bdaddr(int port, u8 *bdaddr)
{
    u8 input = port, reply[7];
    if (!rpc_transaction(DS34USB_GET_BDADDR, &input, 1, reply, 7, 0) || !reply[0])
        return 0;
    memcpy(bdaddr, reply + 1, 6);
    return 1;
}

int ds34usb_set_bdaddr(int port, u8 *bdaddr)
{
    u8 request[7];
    request[0] = port;
    memcpy(request + 1, bdaddr, 6);
    return rpc_transaction(DS34USB_SET_BDADDR, request, 7, NULL, 0, 0);
}

int ds34usb_set_rumble(int port, u8 lrum, u8 rrum)
{
    u8 request[3] = {port, lrum, rrum};
    return rpc_transaction(DS34USB_SET_RUMBLE, request, 3, NULL, 0, 0);
}

int ds34usb_set_led(int port, u8 led)
{
    u8 request[2] = {port, led};
    return rpc_transaction(DS34USB_SET_LED, request, 2, NULL, 0, 0);
}

int ds34usb_get_data(int port, u8 *data)
{
    u8 input = port;
    return data && port >= 0 && port < 2 &&
           rpc_transaction(DS34USB_GET_DATA, &input, 1, data, 18, 1);
}

int ds34usb_reset()
{
    return rpc_transaction(DS34USB_RESET, NULL, 0, NULL, 0, 0);
}
