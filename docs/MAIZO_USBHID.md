# Maizo OPL USB HID (experimental hardware support)

Target: **Microntek USB Joystick VID 0079 / PID 0006, bcdDevice 0107**.

## What is implemented

- USB PadEmu can probe the Microntek/DragonRise VID/PID family.
- The IOP reads the HID **report descriptor at runtime** and maps X/Y, Z/Rx (or
  alternative axes), 12 buttons, a hat switch (or four D-pad usages), and
  digital pressure values into the existing DualShock 2 PadEmu.
- Report IDs, bit-packed fields, nonzero logical minima and 8/15 neutral
  hats are handled. Unknown layouts are rejected rather than causing
  phantom button presses.
- Microntek input-only USB interfaces are supported; no proprietary output
  reports are sent (vibration is **not implemented** for Microntek).
- The original DS3/DS4 USB path is preserved.
- The separate OPL-menu ds34usb IOP driver also uses the descriptor-based decoder, allowing USB pad input in the frontend when USB PadEmu is enabled.
- The Bluetooth path receives defensive fixes for unknown connection handles,
  truncated DS4 reports, and additional **DS4-like names**. Bluetooth discovery,
  SSP pairing and DS4 clone compatibility are **not** implemented.

## How to try on PS2

1. Build OPL with `PADEMU=1` (or download the successful CI artifact).
2. Keep a known-working OPL ELF available as fallback.
3. Start a PS2 game with **Pad Emulator** enabled and **DualShock3/4 USB**
   selected. Enable emulation for the desired pad port.
4. Plug the Microntek `0079:0006` into a PS2 USB port before launching the
   game. The USB PadEmu driver detects its HID descriptor and translates the
   input to the game's DS2 port.
5. Test digital input, diagonals, both joysticks, Start/Select, L3/R3, L/R
   buttons and disconnection. Some titles do not work with PadEmu's SIO2 hook.

## Hardware validation still needed

**This is a driver implementation, not a claim of full hardware compatibility.**
VID/PID `0079:0006` is shared by multiple controllers with different HID
layouts. The actual report descriptor and reports of the user's unit are not
available in this repository. The driver requires a descriptor fitting its
bounded decoder; motor output requires separate protocol confirmation.

If Android with root exposes a HID descriptor:

```sh
su -c 'for f in /sys/bus/hid/devices/*/report_descriptor; do
    case "$f" in *0079:0006*) echo "$f"; od -An -tx1 -v "$f";; esac
done'
```

On Android some kernels do not expose `hidraw` or report descriptors.
Without raw HID data, do not assume a clone DS4 has Sony-compatible Bluetooth
reports merely because it is called `Wireless Controller`.

## Build/host tests

```sh
cc -std=c99 -Wall -Wextra -Werror -Iinclude -Imodules/pademu \
   modules/pademu/maizohid.c tests/maizohid_test.c -o /tmp/maizohid_test
/tmp/maizohid_test
make PADEMU=1
```

The CI workflow compiles OPL using the PS2 toolchain. A successful compile
does not prove that the user's controller pairs or works inside every game.


## Maizo Bluetooth Classic Manager (experimental)
The PadEmu Bluetooth mode provides **Buscar / conectar controle Bluetooth**.
Start HCI Inquiry, refresh the list of discovered addresses and names, select
a device and initiate HCI Create Connection. The frontend supports legacy
PIN 0000 and SSP Just Works on compatible dongles. The 8-device discovery list
and learned keys are session-scoped. **No BLE or universal HID parser.**
Menu keys are not yet passed to the separate in-game PadEmu IOP module; clone
compatibility must be verified on real PS2 hardware. Microntek and original
DS3/DS4 support are retained.

Link keys acquired through SSP / legacy pairing are forwarded from the OPL menu
through EECore into game-time PadEmu when launching a Bluetooth game, but are
currently not persisted on disk. Re-pair after closing/restarting OPL.

Some DS4-compatible Classic HID clones using USB-style input report 0x01
are decoded using the DS4 button layout. Arbitrary generic HID report layouts
remain unsupported.

## Corrigido: lista azul / selecao e pareamento

O campo com o MAC entra em modo de edição quando se pressiona o botão de
confirmação (normalmente X). Pressione o mesmo botão novamente para sair desse
modo preservando a escolha; Círculo cancela e restaura o índice anterior.
A lista agora fica apta a conectar assim que o HCI Inquiry termina, sem ficar
presa na resposta opcional de nome remoto de controles paralelos. A atualização
da lista conserva o dispositivo escolhido e as mensagens de erro diferenciam
busca pendente, lista vazia e porta ocupada. Esta correção não garante suporte
aos relatórios HID específicos de todos os clones.


## Twin USB Joystick 0810:0001 — two players, one USB interface

The user's actual wireless twin receiver is VID:PID **0810:0001**, with **one
USB HID interface** and alternating eight-byte reports marked by **0x01 (P1)**
and **0x02 (P2)** in the first byte. The new USB menu driver and the game-time
PadEmu driver both reserve two logical player slots while sharing a single USB
interrupt endpoint; an independent report cache prevents P2 events overwriting
P1 inputs. Known axes/button/hat assignments follow the captured receiver's
`0810_0001.ini` mapping. P1/P2 must be enabled in PadEmu game settings.
Vibration and untested alternate device firmwares are not guaranteed.


## DS4 clone status and game launch handoff

The controller's **blue LED means only that Bluetooth ACL has connected**;
PadEmu must finish HID control/interrupt L2CAP setup before its input is usable.
The menu now displays this intermediate state separately from HID-ready.
Selected addresses are passed to the game even without a learned 16-byte
link key. As the IOP is rebooted at game launch (which necessarily disconnects
the existing Bluetooth link), the in-game module issues an outbound HCI
Create Connection request to the selected DS4 address and opens HID L2CAP
without waiting for optional Remote Name responses. This is a reconnection
attempt, not a promise that all clone hardware accepts the connection. Using
PS again after game launch may still be necessary.

## Receptor USB 0810:0001 sem reconhecimento no PS2

A partir desta versao, dentro de PadEmu USB, abra **Diagnosticar Twin USB**.
A interface exibe em qual etapa o driver para: sem enumeracao (USB host PS2
nao chamou o driver), VID/PID reconhecido, interface rejeitada, endpoint IN
sem acesso, falha na configuracao USB ou receptor pronto. Em "USB pronto"
mostra contadores reais de pacotes P1/P2 que chegaram ao PS2. Isso e apenas
diagnostico, nao uma afirmacao de que o receptor e fisicamente compativel.
O VID/PID 0810:0001 e reutilizado por diferentes firmwares e descritores.
Experimente o receptor conectado diretamente antes de ligar o PS2 e com
ambos os pads ligados. Nao altera a NAND nem o firmware do receptor.

## Universal USB device detector (menu)

The Twin-only button is now a passive USB-device observer. While the OPL menu
DS34USB IOP driver is loaded, its probe records up to eight devices presented
by PS2 USBD, whether or not PadEmu supports them. The user can cycle through
observed entries; the dialog shows VID/PID, device/interface class, subclass,
protocol, endpoint count and PadEmu driver state. This can include HID gamepads,
mouse/keyboard devices, USB drives, hubs and vendor-specific peripherals when
they reach the driver's USB probe.

**Detection does not imply input-driver support.** Unsupported devices are
not claimed, to avoid breaking the USB drivers responsible for them. For
devices not claimed by DS34USB, the last-seen record may remain after removal:
there is no disconnect callback to this driver in that case. This is an
enumeration diagnostic, not a physical-port analyzer or universal USB HID
controller emulator.

The older Twin-only RPC remains available for compatibility. Twin's
existing player-1/player-2 packet decoder remains unchanged.

## Bluetooth in-game re-pairing improvements

Selected-DS4 incoming reconnect requests now reclaim the existing slot even
if a dropped ACL connection did not deliver its disconnect-complete event.
The game driver no longer sends HCI Disconnect on the old handle right before
accepting the fresh one; that asynchronous completion could have erased the
new slot's state. A connected but not-yet-ready controller retries a stalled
L2CAP control-channel request after 30 seconds without explicitly tearing
down its Bluetooth link.

These are defensive compatibility fixes. The gamepad and Bluetooth dongle
must still be tested on physical PlayStation 2 hardware; no source-level
patch can guarantee all DS4 clones and adapters work.
