# MAIZO SUPREME — Hardware input safety / 2026-11-14

## Symptom

After a previous build, START could open settings while the highlighted
category moved DOWN continuously, and neither physical controllers nor USB
HID appeared to work. The regression was introduced when the menu blindly
merged untrusted bytes from a new, unversioned 16-byte USB RPC response
into libpad's button state. An empty response (all zero) becomes every
active-low DS2 button held, including DOWN and START. The previous code
also assumed four USB slots even though the menu driver implements only
two (MAX_PADS=2). Non-HID Sony controllers could return stale or
uninitialised transfer data when USB polling failed.

## Fix

- IOP and EE use matching MZ + version 1 snapshot protocol, two slots,
  an explicitly valid connection bitmask and neutral defaults.
- Send a slot only after a real HID report was successfully decoded.
- Broken, missing, all-zero or version-mismatched replies are ignored.
- Each connected remote must emit one neutral sample before input is
  accepted; unplug/replug resets that state.
- GUI RPC receives cached snapshots instead of waiting on a live USB
  interrupt transfer for each frame.
- Bluetooth controller data is likewise guarded.
- Physical PS2 libpad input remains independent of USB and Bluetooth.
- L1+R1+START on a physically attached PS2 pad enables temporary
  physical-only mode until reboot; Diagnostics reports the state.
- This cannot guarantee compatibility without tests on actual hardware.

## Manual tests

1. Start unplugged USB/BT: settings must remain stationary.
2. Navigate with a PS2 controller on P1 and P2.
3. Attach HID, release buttons, then test D-pad, START, X/O and L1/R1.
4. Enter/exit a real game, test quick taps and PadEmu persistence.
5. Press L1+R1+START using physical PS2 pad: USB/BT input is bypassed.
6. Test controller hotplug, 3D cover proportions, all Supreme submenus.
7. Verify multitap with physical hardware separately from P1/P2 UI.
