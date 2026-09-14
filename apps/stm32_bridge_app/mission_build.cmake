###########################################################
#
# STM32_BRIDGE_APP mission build setup
#
# This file is evaluated as part of the "prepare" stage
# and can be used to set up prerequisites for the build,
# such as generating header files
#
###########################################################

# The list of header files that control the STM32_BRIDGE_APP configuration
set(STM32_BRIDGE_APP_MISSION_CONFIG_FILE_LIST
  stm32_bridge_app_fcncode_values.h
  stm32_bridge_app_interface_cfg_values.h
  stm32_bridge_app_mission_cfg.h
  stm32_bridge_app_perfids.h
  stm32_bridge_app_msg.h
  stm32_bridge_app_msgdefs.h
  stm32_bridge_app_msgstruct.h
  stm32_bridge_app_tbl.h
  stm32_bridge_app_tbldefs.h
  stm32_bridge_app_tblstruct.h
  stm32_bridge_app_topicid_values.h
)

generate_configfile_set(${STM32_BRIDGE_APP_MISSION_CONFIG_FILE_LIST})

