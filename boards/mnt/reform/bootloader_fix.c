/*
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright 2025 Pete Johanson
  Copyright 2025 joguSD
*/
#include <cmsis_core.h>
#include <pico/bootrom.h>

/*
  This fix is originally from:
  https://github.com/petejohanson/zephyr
  branch: v3.5.0+zmk-fixes
  commit: dc8bff77097c5a1d9a19e2858d83ff83ee1620b3

  And was applied to: soc/arm/rpi_pico/rp2/soc.c

  TODO: Remove once this is supported upstream.

  Overrides the weak ARM implementation:
  Set general purpose retention register and reboot
*/
void sys_arch_reboot(int type) {
    if (type != 0) {
        reset_usb_boot(0,0);
    } else {
        NVIC_SystemReset();
    }
}
