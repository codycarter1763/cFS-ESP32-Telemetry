###########################################################
#
# DISPLAY_APP platform build setup
#
# This file is evaluated as part of the "prepare" stage
# and can be used to set up prerequisites for the build,
# such as generating header files
#
###########################################################

# The list of header files that control the DISPLAY_APP configuration
set(DISPLAY_APP_PLATFORM_CONFIG_FILE_LIST
  display_app_internal_cfg_values.h
  display_app_platform_cfg.h
  display_app_perfids.h
  display_app_msgids.h
  display_app_msgid_values.h
)

generate_configfile_set(${DISPLAY_APP_PLATFORM_CONFIG_FILE_LIST})

