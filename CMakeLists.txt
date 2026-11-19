if(CONFIG_KEYFX)
  zephyr_library()
  zephyr_library_sources(src/keyfx_engine.c src/keyfx_behavior.c src/keyfx_wrap.c)
  zephyr_include_directories(${APPLICATION_SOURCE_DIR}/include)
  # config/keyfx_config.h (the user-editable rules) lives in the ZMK config dir
  zephyr_library_include_directories(${ZMK_CONFIG})

  if(CONFIG_KEYFX_ACTION_HOOK)
    # Route ZMK's behavior invocations through src/keyfx_hook.c first (see that file)
    zephyr_library_sources(src/keyfx_hook.c)
    zephyr_ld_options(-Wl,--wrap=zmk_behavior_invoke_binding)
  endif()
endif()

# ---- dongle lab: test hardware (see lab/) ----
if(CONFIG_KEYFX_LAB_JOYSTICK OR CONFIG_KEYFX_LAB_IMU OR CONFIG_KEYFX_LAB_IMU_MOUSE)
  zephyr_library_named(keyfx_lab)
  zephyr_library_sources_ifdef(CONFIG_KEYFX_LAB_JOYSTICK lab/joystick.c)
  zephyr_library_sources_ifdef(CONFIG_KEYFX_LAB_IMU lab/imu.c)
  zephyr_library_sources_ifdef(CONFIG_KEYFX_LAB_IMU_MOUSE lab/imu_mouse.c)
endif()
