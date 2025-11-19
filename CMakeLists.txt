# @copyright Ruuvi Innovations Ltd.
# SPDX-License-Identifier: BSD-3-Clause

# Find the directory containing <module_name>.cmake by walking up:
#   <this>/cmake/modules → parent/cmake/modules → grandparent/cmake/modules
#
# Usage:
#   find_module_dir_upwards(OUT_VAR git_describe_tag [REQUIRED] [SUBDIR "cmake/modules"])
#
# Notes:
# - module_name may be given with or without the .cmake suffix
# - If REQUIRED is passed and not found → FATAL_ERROR
# - On success, OUT_VAR is set to the *directory* path (normalized)
# - On failure (without REQUIRED), OUT_VAR is set to an empty string
function(find_module_dir_upwards OUT_VAR MODULE_NAME)
  set(options REQUIRED)
  set(oneValueArgs SUBDIR)
  cmake_parse_arguments(FMDU "${options}" "${oneValueArgs}" "" ${ARGN})

  if(MODULE_NAME MATCHES "\\.cmake$")
    set(_module_file "${MODULE_NAME}")
  else()
    set(_module_file "${MODULE_NAME}.cmake")
  endif()

  if(NOT FMDU_SUBDIR)
    set(FMDU_SUBDIR "cmake/modules")
  endif()

  # Build candidate search dirs: current → parent → grandparent
  set(_dirs)
  list(APPEND _dirs "${CMAKE_CURRENT_LIST_DIR}/${FMDU_SUBDIR}")

  get_filename_component(_p1 "${CMAKE_CURRENT_LIST_DIR}" DIRECTORY)
  list(APPEND _dirs "${_p1}/${FMDU_SUBDIR}")

  get_filename_component(_p2 "${_p1}" DIRECTORY)
  list(APPEND _dirs "${_p2}/${FMDU_SUBDIR}")

  # Find the directory containing the module file
  find_path(_hit_dir
    NAMES "${_module_file}"
    HINTS ${_dirs}
    NO_DEFAULT_PATH
  )

  if(NOT _hit_dir)
    if(FMDU_REQUIRED)
      message(FATAL_ERROR
        "Could not find '${_module_file}'. Searched:\n  ${_dirs}")
    endif()
    set(${OUT_VAR} "" PARENT_SCOPE)
    return()
  endif()

  # Normalize and return
  get_filename_component(_norm "${_hit_dir}" REALPATH)
  set(${OUT_VAR} "${_norm}" PARENT_SCOPE)
endfunction()

find_module_dir_upwards(MOD_DIR git_describe_tag.cmake REQUIRED SUBDIR "cmake/modules")
include("${MOD_DIR}/git_describe_tag.cmake")

SET(TAG_PREFIX "b0_v")
SET(TAG_WORK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")

set(B0_VERSION_CUSTOMIZATION
  "//#include;\\\"b0_version_extra.h\\\";"
)

set(GENERATED_VERSION "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")

set(VH ${PROJECT_BINARY_DIR}/include/generated/app_version.h)
set(MYVH ${PROJECT_BINARY_DIR}/include/generated/app_version.generated.h)

git_describe_tag(${TAG_WORK_DIR} ${TAG_PREFIX} B0_BUILD_VERSION)

add_custom_command(
  OUTPUT ${MYVH}
  COMMAND ${CMAKE_COMMAND} -DZEPHYR_BASE=${ZEPHYR_BASE}
    -DOUT_FILE=${MYVH}
    -DVERSION_TYPE=APP
    -DVERSION_FILE=${GENERATED_VERSION}
    -DAPP_VERSION_CUSTOMIZATION="${B0_VERSION_CUSTOMIZATION}"
    -DAPP_BUILD_VERSION=${B0_BUILD_VERSION}
    -DCMAKE_MODULE_PATH=${MOD_DIR}
    -P ${ZEPHYR_BASE}/cmake/gen_version_h.cmake
  DEPENDS ${GENERATED_VERSION} ${git_dependency}
  COMMAND_EXPAND_LISTS
)

# stamp so we don't collide with Zephyr's rule that already outputs ${VH}
add_custom_command(
  OUTPUT ${MYVH}.stamp
  COMMAND ${CMAKE_COMMAND} -E copy ${MYVH} ${VH}
  COMMAND ${CMAKE_COMMAND} -E touch ${MYVH}.stamp
  DEPENDS ${MYVH}             # ensure the default header exists first
)

add_custom_target(override_version_h ALL
  DEPENDS ${MYVH}.stamp)

# Make the Zephyr 'app' target depend on the VERSION file and generted header
add_dependencies(app override_version_h)


target_sources(app PRIVATE
    src/b0_button.c
    src/b0_button.h
    src/b0_early_init.c
    src/b0_err_handler.c
    src/b0_ext_flash_power.c
    src/b0_ext_flash_power.h
    src/b0_gpio_input.c
    src/b0_gpio_input.h
    src/b0_hook.c
    src/b0_led.c
    src/b0_led.h
    src/b0_led_err.c
    src/b0_led_err.h
    src/b0_supercap.c
    src/b0_supercap.h
    src/b0_sleep.c
    src/b0_sleep.h
    src/b0_segger_rtt.c
    src/b0_segger_rtt.h
    src/b0_wrap_printk.c
    src/btldr_img_op.c
    src/btldr_img_op.h
)

target_include_directories(app PRIVATE
    ${ZEPHYR_BASE}/kernel/include
    ${ARCH_DIR}/${ARCH}/include
    ../include
)

target_link_options(app PUBLIC
    -Wl,--wrap=vprintk
)
