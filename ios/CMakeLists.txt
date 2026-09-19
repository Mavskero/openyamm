enable_language(OBJCXX)
set(OPENYAMM_DEVELOPMENT_TEAM "" CACHE STRING "Apple development team used for device signing.")
set(OPENYAMM_BUNDLE_IDENTIFIER "org.openyamm.ipados" CACHE STRING "iPad application bundle identifier.")

add_executable(openyamm_ipados MACOSX_BUNDLE
    IOSMain.mm Launcher.mm ResourceImport.mm ../tools/AppleMetalSmoke.cpp ../tools/AppleMenuPreview.cpp
    ../tools/AppleOutdoorPreview.cpp ../tools/AppleStoryCheck.cpp)
file(GLOB OPENYAMM_GAMEPLAY_LAYOUT_FILES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/assets_dev/engine/ui/gameplay/*.yml")
target_sources(openyamm_ipados PRIVATE ${OPENYAMM_GAMEPLAY_LAYOUT_FILES})
set_source_files_properties(${OPENYAMM_GAMEPLAY_LAYOUT_FILES} PROPERTIES
    MACOSX_PACKAGE_LOCATION "runtime/gameplay_layouts")
target_link_libraries(openyamm_ipados PRIVATE openyamm_game openyamm_engine openyamm_menu_import
    openyamm_outdoor_import openyamm_story_import openyamm_ui_import openyamm_decoration_import
    openyamm_runtime_import
    "-framework Foundation" "-framework UIKit" "-framework UniformTypeIdentifiers")
target_compile_definitions(openyamm_ipados PRIVATE OPENYAMM_SMOKE_SHADER_DIR="runtime/shaders/metal")
target_compile_options(openyamm_ipados PRIVATE -fobjc-arc)
set_target_properties(openyamm_ipados PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
    MACOSX_BUNDLE_GUI_IDENTIFIER "${OPENYAMM_BUNDLE_IDENTIFIER}"
    MACOSX_BUNDLE_BUNDLE_NAME "OpenYAMM"
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "2")
if (CMAKE_GENERATOR STREQUAL "Xcode")
    set_target_properties(openyamm_ipados PROPERTIES
        XCODE_GENERATE_SCHEME YES
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${OPENYAMM_BUNDLE_IDENTIFIER}"
        XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED YES
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE Automatic
        XCODE_ATTRIBUTE_SUPPORTED_PLATFORMS iphoneos)
    if (NOT OPENYAMM_DEVELOPMENT_TEAM STREQUAL "")
        set_target_properties(openyamm_ipados PROPERTIES
            XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${OPENYAMM_DEVELOPMENT_TEAM}")
    endif()
endif()
add_custom_command(TARGET openyamm_ipados POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${OPENYAMM_RUNTIME_SHADER_DIR}" "$<TARGET_BUNDLE_DIR:openyamm_ipados>/runtime/shaders")
