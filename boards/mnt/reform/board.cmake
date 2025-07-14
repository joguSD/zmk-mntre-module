# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright 2025 joguSD

board_runner_args(uf2 "--board-id=RPI-RP2")

include(${ZEPHYR_BASE}/boards/common/uf2.board.cmake)
