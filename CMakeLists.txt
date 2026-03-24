execute_process(
  COMMAND git -C ${ZEPHYR_BASE} rev-parse --short HEAD
  OUTPUT_VARIABLE ZEPHYR_GIT_SHA
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
  COMMAND git -C ${CMAKE_SOURCE_DIR} rev-parse --short HEAD
  OUTPUT_VARIABLE ZMK_GIT_SHA
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
  COMMAND git -C ${CMAKE_CURRENT_SOURCE_DIR} rev-parse --short HEAD
  OUTPUT_VARIABLE MNT_GIT_SHA
  OUTPUT_STRIP_TRAILING_WHITESPACE
  RESULT_VARIABLE MNT_GIT_RESULT
)

if(DEFINED ENV{GITHUB_SHA} AND MNT_GIT_RESULT)
  string(SUBSTRING $ENV{GITHUB_SHA} 0 7 MNT_GIT_SHA)
endif()

foreach(var ZEPHYR_GIT_SHA ZMK_GIT_SHA MNT_GIT_SHA)
  if(NOT ${var})
    set(${var} "unknown")
  endif()
endforeach()

# Export as compile definitions
zephyr_compile_definitions(
  ZEPHYR_GIT_SHA="${ZEPHYR_GIT_SHA}"
  ZMK_GIT_SHA="${ZMK_GIT_SHA}"
  MNT_GIT_SHA="${MNT_GIT_SHA}"
)

target_include_directories(app PRIVATE include)

if ((NOT CONFIG_ZMK_SPLIT) OR CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    target_sources(app PRIVATE src/behaviors/behavior_menu.c)
    target_sources(app PRIVATE src/behaviors/behavior_reform_sysctrl.c)
    target_sources(app PRIVATE src/reform/font.c)
    target_sources(app PRIVATE src/reform/matrix.c)
    target_sources(app PRIVATE src/reform/display.c)
    target_sources(app PRIVATE src/reform/sysctrl.c)
    target_sources_ifdef(CONFIG_ZMK_STUDIO app PRIVATE src/reform/studio_unlock.c)
endif() # ((NOT CONFIG_ZMK_SPLIT) OR CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
