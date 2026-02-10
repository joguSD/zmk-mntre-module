target_include_directories(app PRIVATE include)

if ((NOT CONFIG_ZMK_SPLIT) OR CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    target_sources(app PRIVATE src/behaviors/behavior_menu.c)
    target_sources(app PRIVATE src/reform/matrix.c)
    target_sources(app PRIVATE src/reform/display.c)
endif() # ((NOT CONFIG_ZMK_SPLIT) OR CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
