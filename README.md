# ZMK Module for MNT Reform Keyboard

**NOTE: This firmware is a work in progress and is not stable. Use at your own risk.**

This is an unofficial port of the ZMK firmware to the MNT Reform Keyboard V4.0 based on the RP2040 MCU.

Using this firmware as a standalone keyboard is currently fully supported and functional.

Using this firmware within a Reform laptop (Classic & Next) is largely
implemented but untested as I do not have any laptop hardware at the moment.

# Status

**The USB HID is currently unstable and will crash often making the keyboard unresponsive to the host**

* If this happens, the keyboard can be reset by pressing `CIRCLE -> R`
* The keyboard can be put into flashing mode by pressing `CIRCLE -> X`
* The keyboard can unlocked for modification in ZMK Studio by pressing:
    * `CIRCLE -> U`
    * `HYPER + U`

## Functional
* Reform device tree and pinctrl
* Keyboard matrix and function layers
* RGB - WS2812 driver on PIO0
* Display - Custom Display Code
    * does not use ZMK's display widgets
* Menu - Custom Menu ZMK Behavior
    * Matches the circle menu from original firmware
* MNT Reform system controller support
* Settings saving & loading
* Physical keyboard layout definition
* ZMK Studio Support

![ZMK Studio Screenshot](https://github.com/user-attachments/assets/8b30a9c6-927b-4d7e-b47d-196373f5c54a)

## Planned
* MNT Reform Next Trackpad support

## No plan yet
* MNT Reform Pocket support

# Testing

## Using a prebuilt uf2 firmware

* Download the most recent build from the [GitHub Actions tab](https://github.com/joguSD/zmk-mntre-module/actions)

## Building from source

* Follow the standard ZMK documentation to setup a [native toolchain](https://zmk.dev/docs/development/local-toolchain/setup/native)
* Use the current main branch of ZMK that is on top of Zephyr 4.1
* Build from within the ZMK repository using a command like the following, pointing to this repository as an extra module:

```
west build -p -b reform -S rp2-boot-mode-retention -- -DZMK_EXTRA_MODULES="/some/path/zmk-mntre-module"
```

With ZMK Studio support enabled:

```
west build -p -b reform -S studio-rpc-usb-uart -S rp2-boot-mode-retention -- -DZMK_EXTRA_MODULES="/some/path/zmk-mntre-module" -DCONFIG_ZMK_STUDIO=yes
```

Additional snippets can be added to build for either the MNT Reform Classic or MNT Reform Next mode:
* `-S reform-classic`
* `-S reform-next`

## References

* [MNT Reform Shop link for this keyboard](https://shop.mntre.com/products/mnt-reform-keyboard-30)
* [Official Firmware Source](https://source.mnt.re/reform/reform/-/tree/master/reform2-keyboard4-fw?ref_type=heads)
* [Keyboard PCB Source](https://source.mnt.re/reform/reform/-/tree/master/reform2-keyboard4-pcb?ref_type=heads)
* [Keyboard Schematic](https://mntre.com/documentation/reform-handbook/schematics.html#keyboard-schematics)

## License

SPDX-License-Identifier: GPL-3.0-or-later
