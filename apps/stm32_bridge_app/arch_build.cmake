###########################################################
#
# STM32_BRIDGE_APP platform build setup
#
# This file is evaluated as part of the "prepare" stage
# and can be used to set up prerequisites for the build,
# such as generating header files
#
###########################################################

# The list of header files that control the STM32_BRIDGE_APP configuration
set(STM32_BRIDGE_APP_PLATFORM_CONFIG_FILE_LIST
  stm32_bridge_app_internal_cfg_values.h
  stm32_bridge_app_platform_cfg.h
  stm32_bridge_app_perfids.h
  stm32_bridge_app_msgids.h
  stm32_bridge_app_msgid_values.h
)

generate_configfile_set(${STM32_BRIDGE_APP_PLATFORM_CONFIG_FILE_LIST})

