# Copyright (c) 2026 WiggelMc
# SPDX-License-Identifier: MIT

if ((NOT CONFIG_ZMK_SPLIT) OR CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
  target_sources_ifdef(CONFIG_ZMK_BEHAVIOR_TAPITHIUM_MODS app PRIVATE src/behaviors/behavior_tapithium_mods.c)

  zephyr_include_directories(include)
endif()
