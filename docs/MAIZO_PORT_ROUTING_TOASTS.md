# PadEmu port mapping and controller notifications

- SIO2 commands are routed using the real physical connector and the enabled
  port mask. Disabled ports retain the original native SIO2 response.
  A mixed packet changes only the emulated command's own byte range.
- Multitap uses the selected connector and enabled slot; do not redirect
  the other connector to the multitap slot.
- PadEmu configuration always reloads the correct port's checkbox when
  the selector or settings source changes or the dialog reopens.
- Generic USB HID, Twin USB and Xbox 360 use cached asynchronous input
  at a requested worker delay of 1 ms (previously 2 ms), bounded by USB
  endpoint polling. Unsupported output/rumble avoids a semaphore wait.
- Sony USB and Bluetooth game drivers are retained; the existing in-game
  choice between USB and Bluetooth is not replaced by simultaneous mixed
  transport emulation. Menu input remains independent of that choice.
- Menu toasts display actual controller protocol and logical menu slot for
  USB, paired DS3/DS4 Bluetooth, and physical PS2. A FIFO prevents connection
  notifications from overwriting one another. The notification is a GUI
  feature, not an overlay on top of a running PS2 game.
- Existing Xbox 360 input decoder is unchanged. No unverified output
  protocol or rumble feature is introduced.

Hardware validation: boot P2-only USB, keep a real PS2 pad on P1, swap
P1/P2 settings, boot P1+P2 USB with different devices, test short inputs,
hotplug, paired Bluetooth, and multitap. Verify rapid presses in the game
and ensure no ghost DOWN on disconnect. CI does not measure actual input
latency, game compatibility, or hardware enumeration.
