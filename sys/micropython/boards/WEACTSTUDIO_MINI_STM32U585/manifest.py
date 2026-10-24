# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Koen De Vleeschauwer

include("$(PORT_DIR)/boards/manifest.py")
module("display.py")
module("events.py", base_path="$(BOARD_DIR)/../../drivers")
module("keys.py", base_path="$(BOARD_DIR)/../../drivers")
module("demo_clock.py", base_path="$(BOARD_DIR)/../../examples")
module("demo_graphics.py", base_path="$(BOARD_DIR)/../../examples")
module("demo_mui.py", base_path="$(BOARD_DIR)/../../examples")
module("demo_settings.py", base_path="$(BOARD_DIR)/../../examples")
