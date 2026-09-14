project(CFE_STM32_BRIDGE_APP C)

set(APP_SRC_FILES
  fsw/src/stm32_bridge_app.c
  fsw/src/stm32_bridge_app_cmds.c
  fsw/src/stm32_bridge_app_utils.c
)

if (CFE_EDS_ENABLED)
  list(APPEND APP_SRC_FILES
    fsw/src/stm32_bridge_app_eds_dispatch.c
  )
else()
  list(APPEND APP_SRC_FILES
    fsw/src/stm32_bridge_app_dispatch.c
  )
endif()

# Create the app module
add_cfe_app(stm32_bridge_app ${APP_SRC_FILES})

target_include_directories(stm32_bridge_app PUBLIC fsw/inc)
# Include the public API from sample_lib to demonstrate how
# to call library-provided functions
add_cfe_app_dependency(stm32_bridge_app sample_lib)

# Add table
add_cfe_tables(stm32_bridge_app fsw/tables/stm32_bridge_app_tbl.c)

# If UT is enabled, then add the tests from the subdirectory
# Note that this is an app, and therefore does not provide
# stub functions, as other entities would not typically make
# direct function calls into this application.
if (ENABLE_UNIT_TESTS)
  add_subdirectory(unit-test)
endif (ENABLE_UNIT_TESTS)
