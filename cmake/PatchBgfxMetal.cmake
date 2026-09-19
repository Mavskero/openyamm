include_guard(GLOBAL)

function(openyamm_patch_bgfx_metal)
    # The pinned bgfx revision queries a macOS-only selector on iOS devices.
    # Preserve its existing iOS Depth32Float_Stencil8 choice instead.
    find_package(Git REQUIRED)
    set(patchPath "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/patches/bgfx-metal-ios-depth-format.patch")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${patchPath}")
    execute_process(COMMAND "${GIT_EXECUTABLE}" apply --reverse --check "${patchPath}"
        WORKING_DIRECTORY "${OPENYAMM_BGFX_SOURCE_DIR}"
        RESULT_VARIABLE alreadyApplied OUTPUT_QUIET ERROR_QUIET)
    if (alreadyApplied EQUAL 0)
        return()
    endif()
    execute_process(COMMAND "${GIT_EXECUTABLE}" apply --check "${patchPath}"
        WORKING_DIRECTORY "${OPENYAMM_BGFX_SOURCE_DIR}"
        RESULT_VARIABLE checkResult OUTPUT_VARIABLE patchOutput ERROR_VARIABLE patchError)
    if (NOT checkResult EQUAL 0)
        message(FATAL_ERROR "bgfx Metal patch no longer matches the dependency; review it: ${patchError}")
    endif()
    execute_process(COMMAND "${GIT_EXECUTABLE}" apply "${patchPath}"
        WORKING_DIRECTORY "${OPENYAMM_BGFX_SOURCE_DIR}"
        COMMAND_ERROR_IS_FATAL ANY)
endfunction()
