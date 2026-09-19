add_library(openyamm_engine
    BgfxContext.cpp
    AudioSystem.cpp
    ApplicationConfig.cpp
    AssetFileSystem.cpp
    AssetScaleTier.cpp
    AssetPaths.cpp
    ImageAssetLoader.cpp
    EngineApplication.cpp
    scripting/LuaStateOwner.cpp
    TextTable.cpp
)

if (APPLE)
    target_sources(openyamm_engine PRIVATE AppleText.mm ApplePointer.mm)
    set_source_files_properties(AppleText.mm ApplePointer.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    target_link_libraries(openyamm_engine PUBLIC "-framework Foundation" "-framework CoreText"
        "-framework CoreGraphics")
    if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
        target_link_libraries(openyamm_engine PUBLIC "-framework UIKit" "-framework QuartzCore")
    endif()
endif()

target_include_directories(openyamm_engine
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

target_compile_features(openyamm_engine PUBLIC cxx_std_20)

target_compile_definitions(openyamm_engine
    PUBLIC
        OPENYAMM_DEV_ASSETS_DIR="${OPENYAMM_DEV_ASSETS_DIR}"
        OPENYAMM_WINDOW_WIDTH=${OPENYAMM_WINDOW_WIDTH}
        OPENYAMM_WINDOW_HEIGHT=${OPENYAMM_WINDOW_HEIGHT}
)

target_link_libraries(openyamm_engine
    PUBLIC
        openyamm_bgfx
        SDL3::SDL3
        physfs-static
        ${OPENYAMM_LUA_TARGET}
)
