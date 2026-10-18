if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/generated/glyphs.bin")
    message(FATAL_ERROR "Generate runtime assets: python tools/generate_runtime_assets.py")
endif()
idf_component_register(SRCS "app_main.cc" "runtime_ui.cc" "../../core/learning.cc" "../../core/offline.cc" "../../core/snapshot_store.cc"
    INCLUDE_DIRS "." "../../core"
    REQUIRES zectrix_board zectrix_epd nvs_flash esp_timer
    EMBED_FILES "generated/glyphs.bin" "generated/dashboard_fonts.bin")
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
