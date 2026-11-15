# MAIZO Supreme — File manager + OPL case + HID menu (Nov 2026)

## Native MAIZO file browser (wLaunchELF-inspired)

Integrated in **START > Armazenamento > Gerenciador de arquivos**,
and available from the Armazenamento category. Two panes browse memory
cards, mass0:/, mass1:/ and the active SMB share. Supports folders,
listing (128 items/page session, scrolling), multi-device copy/paste,
copying directories with a finite recursion limit, same-device moving,
renaming, mkdir, guarded deletion, file properties and refreshing.
All destructive operations require manual initiation; overwrite is
refused rather than silently destroying data.

* The SMB root is taken from the existing OPL ETH module when
  `gNetworkStartup == 0`. No second network credentials or SMB profile is
  created. Connection parameters in START > Armazenamento > SMB status
  display the already configured server, share, port and username
  without exposing the saved password.
* If the existing SMB share is read-only, copy **from** SMB to a writable
  device works in principle; operations that write **to** the share
  correctly fail with an error. This is a feature of the server, not a
  reason to change the user's credentials.
* Advanced file operations can hand off to a **separately installed**
  official wLaunchELF ELF from a known path (e.g. mass0:/APPS/wLaunchELF/
  BOOT.ELF). This project does **not** embed or copy wLaunchELF's code
  or claim identical features (e.g. partition management, FTP server,
  text editor, PSU save backups).
* Moving across devices is conservatively a copy with the original
  preserved. Directory recursion is capped at 9 and directory listings
  are capped at 128 to keep PS2 memory use bounded.
* STOP: binary file manager code has not been validated on real storage
  hardware; GitHub cross-compile success only verifies the build.

Reference: ps2homebrew/wLaunchELF (Academic Free License 2.0, no source
copied). Its original file browser inspired the dual-pane workflow.
OPL ART case geometry reference: misc/conf_theme_OPL.cfg.

## Front cover appearance

The carousel retains the user-approved scroll animation and centering,
but now calls the exact original OPL `rmDrawOverlayPixmap` compositor.
The original plastic-case texture is drawn over the **warped** cover
at the four corner coordinates from `misc/conf_theme_OPL.cfg`.
Unlike the prior fake rectangle, the front art must follow the case
perspective and show the original case spine.

## HID controls

Some 0079:0006 USB controllers have raw idle stick axes at a value other
than 127; requiring `105 <= axis <= 150` meant the menu never armed.
The fix accepts **released digital buttons**, captures per-device stick
centres from that report and then decodes motion relative to each centre.
The existing fail-closed invalid-RPC and unplug safety remain.
USB diagnostics now show report ready masks, valid menu slots and
real active-low button bits to distinguish recognition from mapping.

## PS2 hardware checks still required

1. Boot USB HID without a physical pad, release buttons, then navigate.
2. In diagnostics verify READY / SLOT bits and changes in BTN hex.
3. Check true P1/P2 DS2 and BT navigation and PadEmu gameplay.
4. Check official 168x254 case overlay alignment on CRT with ART covers.
5. Try mass0:/, mc0:/, SMB browsing, copy, paste, rename, mkdir and delete.
6. Try a guest read-only SMB server and validate graceful write failures.
7. Test optional wLaunchELF external handoff only when installed locally.
