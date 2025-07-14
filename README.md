# ZMK Module for MNT Reform Keyboard

**NOTE: This firmware is a work in progress and is not stable. Use at your own risk.**

This is an unofficial port of the ZMK firmware to the MNT Reform Keyboard V4.0 based on the RP2040 MCU.

Initially, I aim to get all of the hardware working under ZMK and support the standalone keyboard usecase.

In the future, I hope to implement full support for the laptop usecase as well.

# Status

**The USB HID is currently unstable and will crash often making the keyboard unresponsive to the host**

* If this happens, the keyboard can be reset by pressing `CIRCLE + HYPER + R`
* **Broken after 4.1 migration** - The keyboard can be putting flashing mode by pressing `CIRCLE + HYPER + X`

## Functional
* Reform device tree and pinctrl
* Keyboard matrix and function layers
* RGB - WS2812 driver on PIO0

## Borked
* Enabling the I2C SSD1306 OLED display causes a hard fault

## Planned
* Physical keyboard layout definition
* ZMK Studio Support

## No plan yet
* Trackball, Trackpad support
* MNT Reform system controller support
* MNT Reform Pocket support

# Testing

## Using a prebuilt uf2 firmware

* Download the most recent build from the [GitHub Actions tab](https://github.com/joguSD/zmk-mntre-module/actions)

## Building from source

* Follow the standard ZMK documentation to setup a [native toolchain](https://zmk.dev/docs/development/local-toolchain/setup/native)
* Use the work in progress branch of ZMK that ports it to Zephyr 4.1 [core/move-to-zephyr-4-1](https://github.com/petejohanson/zmk/tree/core%2Fmove-to-zephyr-4-1)
* Build from within the ZMK repository using a command like the following, pointing to this repository as an extra module:

```
west build -p -b reform -- -DZMK_EXTRA_MODULES=/some/path/zmk-mntre-module
```

## Debugging

* Currently, the UART port normally used for the system controller communication is being used as a UART console.

```
sudo minicom -D /dev/ttyACM1 -b 11520
```

## References

* [MNT Reform Shop link for this keyboard](https://shop.mntre.com/products/mnt-reform-keyboard-30)
* [Official Firmware Source](https://source.mnt.re/reform/reform/-/tree/master/reform2-keyboard4-fw?ref_type=heads)
* [Keyboard PCB Source](https://source.mnt.re/reform/reform/-/tree/master/reform2-keyboard4-pcb?ref_type=heads)
* [Keyboard Schematic](https://mntre.com/documentation/reform-handbook/schematics.html#keyboard-schematics)

## License

SPDX-License-Identifier: GPL-3.0-or-later
