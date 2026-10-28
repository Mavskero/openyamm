include_guard(GLOBAL)

function(openyamm_patch_sdl_cocoa sourceDirectory)
    if (NOT APPLE OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
        return()
    endif()
    find_package(Git REQUIRED)
    foreach(patchName IN ITEMS sdl-cocoa-button-position.patch sdl-cocoa-button-release.patch)
        set(patchPath "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/patches/${patchName}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${patchPath}")
        execute_process(COMMAND "${GIT_EXECUTABLE}" apply --reverse --check "${patchPath}"
            WORKING_DIRECTORY "${sourceDirectory}"
            RESULT_VARIABLE alreadyApplied OUTPUT_QUIET ERROR_QUIET)
        if (alreadyApplied EQUAL 0)
            continue()
        endif()
        execute_process(COMMAND "${GIT_EXECUTABLE}" apply --check "${patchPath}"
            WORKING_DIRECTORY "${sourceDirectory}"
            RESULT_VARIABLE checkResult OUTPUT_QUIET ERROR_VARIABLE patchError)
        if (NOT checkResult EQUAL 0)
            message(FATAL_ERROR "SDL Cocoa patch ${patchName} no longer matches; review it: ${patchError}")
        endif()
        execute_process(COMMAND "${GIT_EXECUTABLE}" apply "${patchPath}"
            WORKING_DIRECTORY "${sourceDirectory}" COMMAND_ERROR_IS_FATAL ANY)
    endforeach()
endfunction()
