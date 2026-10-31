include_guard(GLOBAL)

function(openyamm_patch_sdl_uikit sourceDirectory)
    if (NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
        return()
    endif()
    find_package(Git REQUIRED)
    foreach(patchName IN ITEMS sdl-uikit-scene-bounds.patch sdl-uikit-marked-text.patch)
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
            message(FATAL_ERROR "SDL UIKit patch ${patchName} no longer matches; review it: ${patchError}")
        endif()
        execute_process(COMMAND "${GIT_EXECUTABLE}" apply "${patchPath}"
            WORKING_DIRECTORY "${sourceDirectory}" COMMAND_ERROR_IS_FATAL ANY)
    endforeach()
endfunction()
