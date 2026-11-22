# Input recovery and flat library

- Non-multitap selector directly represents the persisted P1/P2 mask. Disabled or absent virtual devices use native SIO2 responses. Mixed transfers preserve native command bytes.
- USB dashboard consumes validated cached input without requiring a neutral button report; dashboard worker uses its previous 2 ms interval and higher priority.
- Dashboard renders flat covers; distinct items with the same disc ID are retained. Game settings and covers still use disc ID.
- Physical testing is required for USB hotplug, native P1 plus virtual P2, both slots, multitap, settings reload, and same-ID titles.

- Controller settings include global USB HID logical-button mapping and keyboard mapping, persisted in CONFIG_OPL and passed to the IOP in menu and game. Standard HID boot keyboard only; no guarantee for NKRO or proprietary keyboards. Xbox decoding unchanged. In-game USB or Bluetooth remains the transport selection. Keyboard default arrows navigate, Z = Cross, X = Circle, Enter = Start, Backspace = Select.
