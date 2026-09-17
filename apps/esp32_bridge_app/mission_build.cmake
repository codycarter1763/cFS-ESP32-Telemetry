###########################################################
#
# ESP32_BRIDGE_APP mission build setup
#
# This file is evaluated as part of the "prepare" stage
# and can be used to set up prerequisites for the build,
# such as generating header files
#
###########################################################

# The list of header files that control the ESP32_BRIDGE_APP configuration
set(ESP32_BRIDGE_APP_MISSION_CONFIG_FILE_LIST
  esp32_bridge_app_fcncode_values.h
  esp32_bridge_app_interface_cfg_values.h
  esp32_bridge_app_mission_cfg.h
  esp32_bridge_app_perfids.h
  esp32_bridge_app_msg.h
  esp32_bridge_app_msgdefs.h
  esp32_bridge_app_msgstruct.h
  esp32_bridge_app_tbl.h
  esp32_bridge_app_tbldefs.h
  esp32_bridge_app_tblstruct.h
  esp32_bridge_app_topicid_values.h
)

generate_configfile_set(${ESP32_BRIDGE_APP_MISSION_CONFIG_FILE_LIST})

