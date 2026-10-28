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

set(OPENYAMM_OWNED_UI_IMAGE_MANIFEST
    "${PROJECT_SOURCE_DIR}/assets_dev/engine/ui/gameplay/owned_images.txt")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${OPENYAMM_OWNED_UI_IMAGE_MANIFEST}")
file(STRINGS "${OPENYAMM_OWNED_UI_IMAGE_MANIFEST}" OPENYAMM_OWNED_UI_IMAGE_NAMES)
if (NOT OPENYAMM_OWNED_UI_IMAGE_NAMES)
    message(FATAL_ERROR "Owned UI image manifest must not be empty.")
endif()
set(OPENYAMM_OWNED_UI_IMAGE_FILES)
foreach (OPENYAMM_OWNED_UI_IMAGE_NAME IN LISTS OPENYAMM_OWNED_UI_IMAGE_NAMES)
    if (NOT OPENYAMM_OWNED_UI_IMAGE_NAME MATCHES "^[a-z0-9_]+[.]png$")
        message(FATAL_ERROR "Invalid owned UI image name: ${OPENYAMM_OWNED_UI_IMAGE_NAME}")
    endif()
    set(OPENYAMM_OWNED_UI_IMAGE_FILE
        "${PROJECT_SOURCE_DIR}/assets_dev/engine/icons/${OPENYAMM_OWNED_UI_IMAGE_NAME}")
    if (NOT EXISTS "${OPENYAMM_OWNED_UI_IMAGE_FILE}")
        message(FATAL_ERROR "Missing owned UI image: ${OPENYAMM_OWNED_UI_IMAGE_FILE}")
    endif()
    list(APPEND OPENYAMM_OWNED_UI_IMAGE_FILES "${OPENYAMM_OWNED_UI_IMAGE_FILE}")
endforeach()
target_sources(openyamm_ipados PRIVATE
    "${OPENYAMM_OWNED_UI_IMAGE_MANIFEST}" ${OPENYAMM_OWNED_UI_IMAGE_FILES})
set_source_files_properties("${OPENYAMM_OWNED_UI_IMAGE_MANIFEST}" PROPERTIES
    MACOSX_PACKAGE_LOCATION "runtime/gameplay_layouts")
set_source_files_properties(${OPENYAMM_OWNED_UI_IMAGE_FILES} PROPERTIES
    MACOSX_PACKAGE_LOCATION "icons")
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
