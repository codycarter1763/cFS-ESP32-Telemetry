###########################################################
#
# DISPLAY_APP mission build setup
#
# This file is evaluated as part of the "prepare" stage
# and can be used to set up prerequisites for the build,
# such as generating header files
#
###########################################################

# The list of header files that control the DISPLAY_APP configuration
set(DISPLAY_APP_MISSION_CONFIG_FILE_LIST
  display_app_fcncode_values.h
  display_app_interface_cfg_values.h
  display_app_mission_cfg.h
  display_app_perfids.h
  display_app_msg.h
  display_app_msgdefs.h
  display_app_msgstruct.h
  display_app_tbl.h
  display_app_tbldefs.h
  display_app_tblstruct.h
  display_app_topicid_values.h
)

generate_configfile_set(${DISPLAY_APP_MISSION_CONFIG_FILE_LIST})

