project(CFE_DISPLAY_APP C)

set(APP_SRC_FILES
  fsw/src/display_app.c
  fsw/src/display_app_cmds.c
  fsw/src/display_app_utils.c
)

if (CFE_EDS_ENABLED)
  list(APPEND APP_SRC_FILES
    fsw/src/display_app_eds_dispatch.c
  )
else()
  list(APPEND APP_SRC_FILES
    fsw/src/display_app_dispatch.c
  )
endif()

# GTK
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

add_cfe_app(display_app ${APP_SRC_FILES})

target_include_directories(display_app PUBLIC
    fsw/inc
    ${CMAKE_SOURCE_DIR}/apps/stm32_bridge_app/fsw/inc
    ${GTK3_INCLUDE_DIRS}
)

target_link_libraries(display_app
    ${GTK3_LIBRARIES}
)

target_compile_options(display_app PRIVATE
    ${GTK3_CFLAGS_OTHER}
)

add_cfe_app_dependency(display_app sample_lib)

add_cfe_tables(display_app fsw/tables/display_app_tbl.c)

if (ENABLE_UNIT_TESTS)
  add_subdirectory(unit-test)
endif()