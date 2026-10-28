cmake_minimum_required(VERSION 3.22)

project(OpenYAMM VERSION 0.1.0 LANGUAGES C CXX)

if (APPLE)
    enable_language(OBJC OBJCXX)
    set(CMAKE_OBJCXX_STANDARD 20)
    set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
endif()

if (WIN32)
    enable_language(RC)
endif()

if (CMAKE_VERSION VERSION_GREATER_EQUAL 4.0 AND NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

if (POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

include(FetchContent)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_BUILD_RPATH_USE_ORIGIN ON)

option(OPENYAMM_BUILD_EDITOR "Build the editor target." OFF)
option(OPENYAMM_BUILD_TOOLS "Build the tools targets." OFF)
option(OPENYAMM_BUILD_TESTS "Build the unit test target." ON)
option(OPENYAMM_BUILD_DESKTOP_EXECUTABLE "Build the desktop OpenYAMM executable." ON)
option(OPENYAMM_USE_SYSTEM_SDL3 "Use an installed SDL3 package instead of fetching it." OFF)
option(OPENYAMM_FETCH_DEPENDENCIES_ONLY "Fetch dependencies before building native shader tools." OFF)
option(
    OPENYAMM_ENABLE_GPROF_SAMPLING
    "Link the desktop executable for low-overhead gprof histogram sampling without call-arc instrumentation."
    OFF
)
set(OPENYAMM_RUNTIME_SHADER_PATH
    ""
    CACHE STRING
    "Runtime shader path embedded in the desktop executable. Empty selects the platform default."
)

set(OPENYAMM_DEV_ASSETS_DIR
    "${CMAKE_SOURCE_DIR}/assets_dev"
    CACHE PATH
    "Path to the development assets directory."
)

set(OPENYAMM_WINDOW_WIDTH
    1600
    CACHE STRING
    "Default application window width."
)

set(OPENYAMM_WINDOW_HEIGHT
    900
    CACHE STRING
    "Default application window height."
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include(Dependencies)
if (OPENYAMM_FETCH_DEPENDENCIES_ONLY)
    return()
endif()
include(BgfxRuntime)

openyamm_configure_lua()

set(OPENYAMM_RUNTIME_SHADER_DIR "${CMAKE_BINARY_DIR}/runtime/shaders")
FetchContent_GetProperties(bgfx SOURCE_DIR OPENYAMM_BGFX_SOURCE_DIR)
FetchContent_GetProperties(bx SOURCE_DIR OPENYAMM_BX_SOURCE_DIR)
FetchContent_GetProperties(bimg SOURCE_DIR OPENYAMM_BIMG_SOURCE_DIR)
FetchContent_GetProperties(ffmpeg SOURCE_DIR OPENYAMM_FFMPEG_SOURCE_DIR)
openyamm_configure_bgfx_runtime()
openyamm_configure_runtime_shaders()

add_subdirectory(engine)
add_subdirectory(game)

if (APPLE)
    add_library(openyamm_menu_import STATIC tools/mmerge_import/MenuResources.cpp tools/mmerge_import/LodResources.cpp)
    target_include_directories(openyamm_menu_import PUBLIC "${CMAKE_SOURCE_DIR}")
    target_link_libraries(openyamm_menu_import PRIVATE z)
    add_library(openyamm_outdoor_import STATIC tools/mmerge_import/OutdoorResources.cpp
        tools/mmerge_import/ActorResources.cpp tools/mmerge_import/SpriteFrameResources.cpp
        tools/mmerge_import/SurfaceResources.cpp)
    target_link_libraries(openyamm_outdoor_import PUBLIC openyamm_menu_import openyamm_game)
    add_library(openyamm_text_import STATIC tools/mmerge_import/TextResources.cpp)
    target_include_directories(openyamm_text_import PUBLIC "${CMAKE_SOURCE_DIR}")
    target_link_libraries(openyamm_text_import PRIVATE iconv)
    add_library(openyamm_story_import STATIC tools/mmerge_import/StoryResources.cpp)
    target_link_libraries(openyamm_story_import PUBLIC openyamm_menu_import openyamm_game openyamm_text_import)
    add_library(openyamm_core_import STATIC tools/mmerge_import/CoreResources.cpp)
    target_link_libraries(openyamm_core_import PUBLIC openyamm_menu_import openyamm_game openyamm_text_import)
    add_library(openyamm_combat_import STATIC tools/mmerge_import/CombatResources.cpp
        tools/mmerge_import/EnchantmentResources.cpp)
    target_link_libraries(openyamm_combat_import PUBLIC openyamm_menu_import openyamm_game openyamm_text_import)
    add_library(openyamm_service_import STATIC tools/mmerge_import/ServiceResources.cpp)
    target_link_libraries(openyamm_service_import PUBLIC openyamm_menu_import openyamm_game openyamm_text_import)
    target_link_libraries(openyamm_service_import PRIVATE openyamm_engine)
    add_library(openyamm_media_import STATIC tools/mmerge_import/MediaResources.cpp)
    target_link_libraries(openyamm_media_import PUBLIC openyamm_menu_import)
    add_library(openyamm_journal_import STATIC tools/mmerge_import/JournalResources.cpp)
    target_link_libraries(openyamm_journal_import PUBLIC openyamm_menu_import openyamm_game openyamm_text_import)
    target_link_libraries(openyamm_journal_import PRIVATE openyamm_engine)
    add_library(openyamm_class_import STATIC tools/mmerge_import/ClassResources.cpp)
    target_link_libraries(openyamm_class_import PUBLIC openyamm_menu_import openyamm_game openyamm_text_import)
    target_link_libraries(openyamm_class_import PRIVATE openyamm_engine)
    add_library(openyamm_inspection_import STATIC tools/mmerge_import/InspectionResources.cpp)
    target_link_libraries(openyamm_inspection_import PUBLIC openyamm_menu_import openyamm_game openyamm_text_import)
    add_library(openyamm_race_import STATIC tools/mmerge_import/RaceResources.cpp)
    target_link_libraries(openyamm_race_import PUBLIC openyamm_menu_import openyamm_game openyamm_text_import)
    target_link_libraries(openyamm_race_import PRIVATE yaml-cpp::yaml-cpp)
    add_library(openyamm_inventory_import STATIC tools/mmerge_import/InventoryResources.cpp
        tools/mmerge_import/ItemIconResources.cpp)
    target_link_libraries(openyamm_inventory_import PUBLIC openyamm_menu_import openyamm_game openyamm_text_import)
    target_link_libraries(openyamm_inventory_import PRIVATE openyamm_engine)
    add_library(openyamm_frame_import STATIC tools/mmerge_import/FrameResources.cpp)
    target_link_libraries(openyamm_frame_import PUBLIC openyamm_menu_import openyamm_game openyamm_text_import)
    add_library(openyamm_fx_import STATIC tools/mmerge_import/FxResources.cpp)
    target_link_libraries(openyamm_fx_import PUBLIC openyamm_menu_import openyamm_game openyamm_text_import)
    target_link_libraries(openyamm_fx_import PRIVATE openyamm_engine)
    add_library(openyamm_transition_import STATIC tools/mmerge_import/TransitionResources.cpp)
    target_link_libraries(openyamm_transition_import PUBLIC openyamm_menu_import openyamm_game openyamm_text_import)
    target_link_libraries(openyamm_transition_import PRIVATE openyamm_engine)
    add_library(openyamm_roster_import STATIC tools/mmerge_import/RosterResources.cpp)
    target_link_libraries(openyamm_roster_import PUBLIC openyamm_menu_import openyamm_game openyamm_text_import)
    target_link_libraries(openyamm_roster_import PRIVATE openyamm_engine)
    add_library(openyamm_doll_import STATIC tools/mmerge_import/DollResources.cpp)
    target_link_libraries(openyamm_doll_import PUBLIC openyamm_menu_import openyamm_game openyamm_text_import)
    target_link_libraries(openyamm_doll_import PRIVATE openyamm_engine)
    add_library(openyamm_teacher_import STATIC tools/mmerge_import/TeacherResources.cpp)
    target_link_libraries(openyamm_teacher_import PUBLIC openyamm_menu_import openyamm_game openyamm_text_import)
    target_link_libraries(openyamm_teacher_import PRIVATE openyamm_engine)
    add_library(openyamm_npc_import STATIC tools/mmerge_import/NpcResources.cpp)
    target_link_libraries(openyamm_npc_import PUBLIC openyamm_menu_import openyamm_game openyamm_text_import)
    target_link_libraries(openyamm_npc_import PRIVATE openyamm_engine)
    add_library(openyamm_support_import STATIC tools/mmerge_import/SupportResources.cpp)
    target_link_libraries(openyamm_support_import PUBLIC openyamm_menu_import openyamm_game openyamm_text_import)
    target_link_libraries(openyamm_support_import PRIVATE openyamm_engine)
    add_library(openyamm_bolster_import STATIC tools/mmerge_import/BolsterResources.cpp)
    target_link_libraries(openyamm_bolster_import PUBLIC openyamm_menu_import openyamm_game openyamm_text_import)
    target_link_libraries(openyamm_bolster_import PRIVATE openyamm_engine)
    add_library(openyamm_voice_import STATIC tools/mmerge_import/VoiceResources.cpp)
    target_link_libraries(openyamm_voice_import PUBLIC openyamm_menu_import openyamm_game openyamm_text_import)
    target_link_libraries(openyamm_voice_import PRIVATE openyamm_engine)
    add_library(openyamm_equipment_picture_import STATIC tools/mmerge_import/EquipmentPictureResources.cpp)
    target_link_libraries(openyamm_equipment_picture_import PUBLIC
        openyamm_menu_import openyamm_game openyamm_text_import)
    target_link_libraries(openyamm_equipment_picture_import PRIVATE openyamm_engine)
    add_library(openyamm_world_rule_import STATIC tools/mmerge_import/WorldRuleResources.cpp)
    target_link_libraries(openyamm_world_rule_import PUBLIC openyamm_menu_import openyamm_game openyamm_text_import)
    target_link_libraries(openyamm_world_rule_import PRIVATE openyamm_engine)
    add_library(openyamm_arcomage_import STATIC tools/mmerge_import/ArcomageResources.cpp)
    target_link_libraries(openyamm_arcomage_import PUBLIC openyamm_menu_import openyamm_game openyamm_text_import)
    target_link_libraries(openyamm_arcomage_import PRIVATE openyamm_engine)
    add_library(openyamm_sound_import STATIC tools/mmerge_import/SoundResources.cpp
        tools/mmerge_import/SoundPayloadResources.cpp)
    target_link_libraries(openyamm_sound_import PUBLIC openyamm_menu_import openyamm_game openyamm_text_import)
    target_link_libraries(openyamm_sound_import PRIVATE openyamm_engine)
    add_library(openyamm_speech_import STATIC tools/mmerge_import/SpeechResources.cpp)
    target_link_libraries(openyamm_speech_import PUBLIC openyamm_menu_import openyamm_game openyamm_text_import)
    target_link_libraries(openyamm_speech_import PRIVATE openyamm_engine)
    add_library(openyamm_ui_import STATIC tools/mmerge_import/UiResources.cpp)
    target_link_libraries(openyamm_ui_import PUBLIC openyamm_menu_import openyamm_game)
    target_link_libraries(openyamm_ui_import PRIVATE openyamm_engine)
    add_library(openyamm_decoration_import STATIC tools/mmerge_import/DecorationResources.cpp)
    target_link_libraries(openyamm_decoration_import PUBLIC openyamm_outdoor_import openyamm_text_import)
    add_library(openyamm_event_import STATIC tools/mmerge_import/EventResources.cpp
        tools/mmerge_import/CrossContinentsResources.cpp
        tools/mmerge_import/DimensionDoorResources.cpp
        tools/mmerge_import/HarmondaleCastleResources.cpp
        tools/mmerge_import/LambentCastleResources.cpp
        tools/mmerge_import/StoneCityResources.cpp
        tools/mmerge_import/EmeraldIslandTravelResources.cpp
        tools/mmerge_import/FreeHavenTempleResources.cpp
        tools/mmerge_import/TownPortalVisitResources.cpp
        tools/mmerge_import/SubmarineTravelResources.cpp
        tools/mmerge_import/StoneStatueResources.cpp
        tools/mmerge_import/Mm8ReentryResources.cpp
        tools/mmerge_import/TempleBottleResources.cpp
        tools/mmerge_import/ControlCubeRefillResources.cpp
        tools/mmerge_import/GloamingCastleResources.cpp
        tools/mmerge_import/SkeletonTransformerResources.cpp
        tools/mmerge_import/SciFiResources.cpp
        tools/mmerge_import/BlasterLearningResources.cpp
        tools/mmerge_import/LichPromotionResources.cpp
        tools/mmerge_import/Mm7EndingResources.cpp
        tools/mmerge_import/ArchibaldResources.cpp
        tools/mmerge_import/SmugglersCoveResources.cpp
        tools/mmerge_import/Mm6HiveResources.cpp
        tools/mmerge_import/Mm7ShoalsResources.cpp
        tools/mmerge_import/Mm8EndgameResources.cpp
        tools/mmerge_import/QuestItemCapacityResources.cpp
        tools/mmerge_import/RunawayChaosResources.cpp
        tools/mmerge_import/BreachMazeTemplateResources.cpp
        tools/mmerge_import/BreachMazePopulationResources.cpp
        tools/mmerge_import/BreachMazeRuntimeResources.cpp
        tools/mmerge_import/BreachAlvarCompanionResources.cpp
        tools/mmerge_import/BreachMercenaryResources.cpp
        tools/mmerge_import/MercenaryReleaseResources.cpp
        tools/LegacyLuaExport.cpp tools/legacy_events/EvtProgram.cpp tools/legacy_events/StrTable.cpp)
    file(READ "${CMAKE_SOURCE_DIR}/assets_dev/engine/scripts/common/event_support.lua" OPENYAMM_EVENT_SUPPORT_SOURCE)
    configure_file(tools/mmerge_import/EventSupportSource.h.in generated/EventSupportSource.h @ONLY)
    file(READ "${CMAKE_SOURCE_DIR}/tools/mmerge_import/overlays/Global_mmmerge_cross_continents_intro.lua"
        OPENYAMM_CC_INTRO_SOURCE)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/assets_dev/engine/scripts/common/event_support.lua"
        "${CMAKE_SOURCE_DIR}/tools/mmerge_import/overlays/Global_mmmerge_cross_continents_intro.lua")
    configure_file(tools/mmerge_import/CrossContinentsIntroSource.h.in generated/CrossContinentsIntroSource.h @ONLY)
    file(READ "${CMAKE_SOURCE_DIR}/tools/mmerge_import/overlays/Map_mm6_hive.lua" OPENYAMM_MM6_HIVE_SOURCE)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/tools/mmerge_import/overlays/Map_mm6_hive.lua")
    configure_file(tools/mmerge_import/Mm6HiveSource.h.in generated/Mm6HiveSource.h @ONLY)
    file(READ "${CMAKE_SOURCE_DIR}/tools/mmerge_import/overlays/Map_mm7_out14.lua" OPENYAMM_MM7_AVLEE_SOURCE)
    file(READ "${CMAKE_SOURCE_DIR}/tools/mmerge_import/overlays/Map_mm7_7out15.lua" OPENYAMM_MM7_SHOALS_SOURCE)
    file(READ "${CMAKE_SOURCE_DIR}/tools/mmerge_import/overlays/Map_mm7_7d23.lua" OPENYAMM_MM7_LINCOLN_SOURCE)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/tools/mmerge_import/overlays/Map_mm7_out14.lua"
        "${CMAKE_SOURCE_DIR}/tools/mmerge_import/overlays/Map_mm7_7out15.lua"
        "${CMAKE_SOURCE_DIR}/tools/mmerge_import/overlays/Map_mm7_7d23.lua")
    configure_file(tools/mmerge_import/Mm7ShoalsSource.h.in generated/Mm7ShoalsSource.h @ONLY)
    file(READ "${CMAKE_SOURCE_DIR}/tools/mmerge_import/overlays/Global_mmmerge_runaway_chaos.lua"
        OPENYAMM_RUNAWAY_CHAOS_SOURCE)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/tools/mmerge_import/overlays/Global_mmmerge_runaway_chaos.lua")
    configure_file(tools/mmerge_import/RunawayChaosSource.h.in generated/RunawayChaosSource.h @ONLY)
    file(READ "${CMAKE_SOURCE_DIR}/tools/mmerge_import/overlays/Global_mmmerge_quest_item_capacity.lua"
        OPENYAMM_QUEST_ITEM_CAPACITY_SOURCE)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/tools/mmerge_import/overlays/Global_mmmerge_quest_item_capacity.lua")
    configure_file(tools/mmerge_import/QuestItemCapacitySource.h.in generated/QuestItemCapacitySource.h @ONLY)
    file(READ "${CMAKE_SOURCE_DIR}/tools/mmerge_import/overlays/Map_mmmerge_br_alvar_companions.lua"
        OPENYAMM_BREACH_ALVAR_COMPANION_MAP_SOURCE)
    file(READ "${CMAKE_SOURCE_DIR}/tools/mmerge_import/overlays/Global_mmmerge_br_alvar_companions.lua"
        OPENYAMM_BREACH_ALVAR_COMPANION_GLOBAL_SOURCE)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/tools/mmerge_import/overlays/Map_mmmerge_br_alvar_companions.lua"
        "${CMAKE_SOURCE_DIR}/tools/mmerge_import/overlays/Global_mmmerge_br_alvar_companions.lua")
    configure_file(tools/mmerge_import/BreachAlvarCompanionSource.h.in
        generated/BreachAlvarCompanionSource.h @ONLY)
    file(READ "${CMAKE_SOURCE_DIR}/tools/mmerge_import/overlays/Map_mmmerge_br_alvar_maze.lua"
        OPENYAMM_BREACH_MAZE_BR_ALVAR_MAP_SOURCE)
    file(READ "${CMAKE_SOURCE_DIR}/tools/mmerge_import/overlays/Map_mmmerge_breach_maze.lua"
        OPENYAMM_BREACH_MAZE_BREACH_MAP_SOURCE)
    file(READ "${CMAKE_SOURCE_DIR}/tools/mmerge_import/overlays/Map_mmmerge_br_base_maze.lua"
        OPENYAMM_BREACH_MAZE_BR_BASE_MAP_SOURCE)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/tools/mmerge_import/overlays/Map_mmmerge_br_alvar_maze.lua"
        "${CMAKE_SOURCE_DIR}/tools/mmerge_import/overlays/Map_mmmerge_breach_maze.lua"
        "${CMAKE_SOURCE_DIR}/tools/mmerge_import/overlays/Map_mmmerge_br_base_maze.lua")
    configure_file(tools/mmerge_import/BreachMazeEventSource.h.in generated/BreachMazeEventSource.h @ONLY)
    target_include_directories(openyamm_event_import PRIVATE "${CMAKE_BINARY_DIR}/generated")
    target_link_libraries(openyamm_event_import PUBLIC openyamm_menu_import openyamm_game openyamm_text_import)
    target_link_libraries(openyamm_event_import PRIVATE openyamm_engine)
    target_link_libraries(openyamm_story_import PRIVATE openyamm_event_import)
    add_library(openyamm_runtime_import STATIC tools/mmerge_import/RuntimeResources.cpp
        tools/mmerge_import/RuntimeZip.cpp tools/mmerge_import/SceneResources.cpp
        tools/mmerge_import/ObjectPresentationResources.cpp)
    target_include_directories(openyamm_runtime_import PRIVATE "${CMAKE_BINARY_DIR}/generated")
    target_link_libraries(openyamm_runtime_import PUBLIC openyamm_menu_import openyamm_core_import
        openyamm_story_import openyamm_combat_import openyamm_service_import openyamm_journal_import
        openyamm_class_import openyamm_inspection_import openyamm_race_import openyamm_inventory_import
        openyamm_frame_import openyamm_fx_import openyamm_transition_import openyamm_roster_import
        openyamm_doll_import openyamm_teacher_import openyamm_npc_import openyamm_support_import
        openyamm_bolster_import openyamm_voice_import openyamm_equipment_picture_import openyamm_world_rule_import
        openyamm_arcomage_import openyamm_sound_import openyamm_speech_import openyamm_ui_import
        openyamm_outdoor_import openyamm_decoration_import openyamm_media_import openyamm_event_import)
    target_link_libraries(openyamm_runtime_import PRIVATE openyamm_engine z)
endif()

if (APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    add_executable(runtime_import tools/runtime_import_main.cpp)
    target_link_libraries(runtime_import PRIVATE openyamm_runtime_import)
    add_executable(scene_import tools/indoor_import_main.cpp)
    target_link_libraries(scene_import PRIVATE openyamm_runtime_import)
    add_executable(object_presentation_import tools/object_presentation_import_main.cpp)
    target_link_libraries(object_presentation_import PRIVATE openyamm_runtime_import)
    add_executable(indoor_import tools/indoor_import_main.cpp)
    target_link_libraries(indoor_import PRIVATE openyamm_runtime_import)
    add_executable(scene_import_runtime_check tests/SceneImportRuntimeCheck.cpp)
    target_link_libraries(scene_import_runtime_check PRIVATE openyamm_runtime_import openyamm_game openyamm_engine)
    add_executable(merge_scene_resource_check tests/MergeSceneResourceCheck.cpp)
    target_link_libraries(merge_scene_resource_check PRIVATE openyamm_game openyamm_engine)
    add_executable(minimap_arrow_check tests/MinimapArrowCheck.cpp)
    target_link_libraries(minimap_arrow_check PRIVATE openyamm_game openyamm_engine)
    add_executable(indoor_session_check tools/IndoorSessionCheck.cpp)
    target_link_libraries(indoor_session_check PRIVATE openyamm_game openyamm_engine)
    add_executable(event_import tools/event_import_main.cpp)
    target_link_libraries(event_import PRIVATE openyamm_event_import)
    add_executable(event_import_runtime_check tests/EventImportRuntimeCheck.cpp)
    target_include_directories(event_import_runtime_check PRIVATE "${CMAKE_BINARY_DIR}/generated")
    target_link_libraries(event_import_runtime_check PRIVATE openyamm_event_import openyamm_engine)
    add_executable(cross_continents_intro_check tests/UnitTestMain.cpp tests/CrossContinentsIntroTests.cpp)
    target_include_directories(cross_continents_intro_check PRIVATE
        "${CMAKE_BINARY_DIR}/generated" "${CMAKE_SOURCE_DIR}/third_party/doctest")
    target_link_libraries(cross_continents_intro_check PRIVATE openyamm_event_import openyamm_engine)
    add_executable(merge_invitation_check tests/UnitTestMain.cpp tests/MergeInvitationTests.cpp
        tests/ChestRegressionTests.cpp tests/RegressionGameData.cpp)
    target_include_directories(merge_invitation_check PRIVATE
        "${CMAKE_BINARY_DIR}/generated" "${CMAKE_SOURCE_DIR}/third_party/doctest")
    target_compile_definitions(merge_invitation_check PRIVATE OPENYAMM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(merge_invitation_check PRIVATE openyamm_event_import openyamm_game openyamm_engine)
    add_executable(town_portal_contact_check tests/UnitTestMain.cpp tests/TownPortalContactTests.cpp)
    add_executable(merge_quest_lifecycle_check tests/UnitTestMain.cpp tests/MergeQuestLifecycleTests.cpp)
    target_include_directories(merge_quest_lifecycle_check PRIVATE
        "${CMAKE_BINARY_DIR}/generated" "${CMAKE_SOURCE_DIR}/third_party/doctest")
    target_compile_definitions(merge_quest_lifecycle_check PRIVATE OPENYAMM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(merge_quest_lifecycle_check PRIVATE openyamm_game openyamm_engine)
    add_executable(verdant_encounter_check tests/UnitTestMain.cpp tests/VerdantEncounterTests.cpp
        tests/RegressionGameData.cpp)
    target_include_directories(verdant_encounter_check PRIVATE
        "${CMAKE_BINARY_DIR}/generated" "${CMAKE_SOURCE_DIR}/third_party/doctest")
    target_compile_definitions(verdant_encounter_check PRIVATE OPENYAMM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(verdant_encounter_check PRIVATE openyamm_game openyamm_engine)
    add_executable(verdant_dialogue_check tests/UnitTestMain.cpp tests/VerdantDialogueTests.cpp
        tests/RegressionGameData.cpp)
    target_include_directories(verdant_dialogue_check PRIVATE
        "${CMAKE_BINARY_DIR}/generated" "${CMAKE_SOURCE_DIR}/third_party/doctest")
    target_compile_definitions(verdant_dialogue_check PRIVATE OPENYAMM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(verdant_dialogue_check PRIVATE openyamm_game openyamm_engine)
    add_executable(connector_stone_check tests/UnitTestMain.cpp tests/ConnectorStoneTests.cpp
        tests/RegressionGameData.cpp)
    target_include_directories(connector_stone_check PRIVATE
        "${CMAKE_BINARY_DIR}/generated" "${CMAKE_SOURCE_DIR}/third_party/doctest")
    target_compile_definitions(connector_stone_check PRIVATE OPENYAMM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(connector_stone_check PRIVATE openyamm_game openyamm_engine)
    add_executable(continent_rewards_check tests/UnitTestMain.cpp tests/ContinentRewardTests.cpp
        tests/RegressionGameData.cpp)
    target_include_directories(continent_rewards_check PRIVATE
        "${CMAKE_BINARY_DIR}/generated" "${CMAKE_SOURCE_DIR}/third_party/doctest")
    target_compile_definitions(continent_rewards_check PRIVATE OPENYAMM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(continent_rewards_check PRIVATE openyamm_game openyamm_engine)
    add_executable(pending_item_delivery_check tests/UnitTestMain.cpp tests/PendingItemDeliveryTests.cpp)
    target_include_directories(pending_item_delivery_check PRIVATE "${CMAKE_SOURCE_DIR}/third_party/doctest")
    target_link_libraries(pending_item_delivery_check PRIVATE openyamm_game openyamm_engine)
    add_executable(event_item_ownership_check tests/UnitTestMain.cpp tests/EventItemOwnershipTests.cpp
        tests/RegressionGameData.cpp)
    target_include_directories(event_item_ownership_check PRIVATE
        "${CMAKE_BINARY_DIR}/generated" "${CMAKE_SOURCE_DIR}/third_party/doctest")
    target_compile_definitions(event_item_ownership_check PRIVATE OPENYAMM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(event_item_ownership_check PRIVATE openyamm_game openyamm_engine)
    add_executable(quest_item_capacity_check tests/UnitTestMain.cpp tests/QuestItemCapacityTests.cpp
        tests/RegressionGameData.cpp)
    target_include_directories(quest_item_capacity_check PRIVATE
        "${CMAKE_BINARY_DIR}/generated" "${CMAKE_SOURCE_DIR}/third_party/doctest")
    target_compile_definitions(quest_item_capacity_check PRIVATE OPENYAMM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(quest_item_capacity_check PRIVATE openyamm_event_import openyamm_game openyamm_engine)
    add_executable(npc_follower_persistence_check tests/UnitTestMain.cpp tests/NpcFollowerPersistenceTests.cpp)
    target_include_directories(npc_follower_persistence_check PRIVATE "${CMAKE_SOURCE_DIR}/third_party/doctest")
    target_link_libraries(npc_follower_persistence_check PRIVATE openyamm_game openyamm_engine)
    add_executable(mm6_hive_endgame_check tests/UnitTestMain.cpp tests/Mm6HiveEndgameTests.cpp)
    target_include_directories(mm6_hive_endgame_check PRIVATE
        "${CMAKE_BINARY_DIR}/generated" "${CMAKE_SOURCE_DIR}/third_party/doctest")
    target_compile_definitions(mm6_hive_endgame_check PRIVATE OPENYAMM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(mm6_hive_endgame_check PRIVATE openyamm_event_import openyamm_game openyamm_engine)
    add_executable(runaway_chaos_check tests/UnitTestMain.cpp tests/RunawayChaosTests.cpp
        tests/RegressionGameData.cpp)
    target_include_directories(runaway_chaos_check PRIVATE
        "${CMAKE_BINARY_DIR}/generated" "${CMAKE_SOURCE_DIR}/third_party/doctest")
    target_compile_definitions(runaway_chaos_check PRIVATE OPENYAMM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(runaway_chaos_check PRIVATE openyamm_event_import openyamm_game openyamm_engine)
    add_executable(breach_maze_layout_check tests/UnitTestMain.cpp tests/BreachMazeLayoutTests.cpp)
    add_executable(breach_maze_capture tools/BreachMazeCapture.cpp)
    target_link_libraries(breach_maze_capture PRIVATE openyamm_event_import openyamm_game openyamm_engine)
    target_include_directories(breach_maze_layout_check PRIVATE "${CMAKE_SOURCE_DIR}/third_party/doctest")
    target_compile_definitions(breach_maze_layout_check PRIVATE OPENYAMM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(breach_maze_layout_check PRIVATE openyamm_event_import openyamm_game openyamm_engine)
    add_executable(breach_maze_scene_check tests/UnitTestMain.cpp tests/BreachMazeSceneBuilderTests.cpp)
    target_include_directories(breach_maze_scene_check PRIVATE "${CMAKE_SOURCE_DIR}/third_party/doctest")
    target_compile_definitions(breach_maze_scene_check PRIVATE OPENYAMM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(breach_maze_scene_check PRIVATE openyamm_event_import openyamm_game openyamm_engine)
    add_executable(breach_maze_lifecycle_check tests/UnitTestMain.cpp tests/BreachMazeLifecycleTests.cpp)
    target_include_directories(breach_maze_lifecycle_check PRIVATE "${CMAKE_SOURCE_DIR}/third_party/doctest")
    target_compile_definitions(breach_maze_lifecycle_check PRIVATE OPENYAMM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(breach_maze_lifecycle_check PRIVATE openyamm_event_import openyamm_game openyamm_engine)
    add_executable(breach_alvar_split_plan_check tests/UnitTestMain.cpp tests/BreachAlvarSplitPlanTests.cpp)
    target_include_directories(breach_alvar_split_plan_check PRIVATE "${CMAKE_SOURCE_DIR}/third_party/doctest")
    target_link_libraries(breach_alvar_split_plan_check PRIVATE openyamm_game openyamm_engine)
    add_executable(breach_alvar_companion_check tests/UnitTestMain.cpp tests/BreachAlvarCompanionTests.cpp)
    target_include_directories(breach_alvar_companion_check PRIVATE "${CMAKE_SOURCE_DIR}/third_party/doctest")
    target_link_libraries(breach_alvar_companion_check PRIVATE openyamm_game openyamm_engine)
    add_executable(breach_alvar_session_check tests/UnitTestMain.cpp tests/BreachAlvarSessionTests.cpp)
    target_include_directories(breach_alvar_session_check PRIVATE "${CMAKE_SOURCE_DIR}/third_party/doctest")
    target_link_libraries(breach_alvar_session_check PRIVATE openyamm_game openyamm_engine)
    add_executable(breach_alvar_save_check tests/UnitTestMain.cpp tests/BreachAlvarSaveTests.cpp)
    target_include_directories(breach_alvar_save_check PRIVATE "${CMAKE_SOURCE_DIR}/third_party/doctest")
    target_compile_definitions(breach_alvar_save_check PRIVATE OPENYAMM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(breach_alvar_save_check PRIVATE openyamm_event_import openyamm_game openyamm_engine)
    add_executable(breach_alvar_resource_check tests/UnitTestMain.cpp tests/BreachAlvarCompanionResourceTests.cpp)
    target_include_directories(breach_alvar_resource_check PRIVATE
        "${CMAKE_SOURCE_DIR}/third_party/doctest" "${CMAKE_BINARY_DIR}/generated")
    target_compile_definitions(breach_alvar_resource_check PRIVATE OPENYAMM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(breach_alvar_resource_check PRIVATE openyamm_event_import openyamm_game openyamm_engine)
    add_executable(breach_alvar_world_actor_check tests/UnitTestMain.cpp tests/BreachAlvarWorldActorTests.cpp
        tests/RegressionGameData.cpp)
    target_include_directories(breach_alvar_world_actor_check PRIVATE "${CMAKE_SOURCE_DIR}/third_party/doctest")
    target_compile_definitions(breach_alvar_world_actor_check PRIVATE OPENYAMM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(breach_alvar_world_actor_check PRIVATE openyamm_game openyamm_engine)
    add_executable(outdoor_runtime_npc_picking_check tests/UnitTestMain.cpp tests/OutdoorRuntimeNpcPickingTests.cpp)
    target_include_directories(outdoor_runtime_npc_picking_check PRIVATE "${CMAKE_SOURCE_DIR}/third_party/doctest")
    target_link_libraries(outdoor_runtime_npc_picking_check PRIVATE openyamm_game openyamm_engine)
    add_executable(gameplay_world_mouse_activation_check
        tests/UnitTestMain.cpp tests/GameplayWorldMouseActivationTests.cpp)
    target_include_directories(gameplay_world_mouse_activation_check PRIVATE "${CMAKE_SOURCE_DIR}/third_party/doctest")
    target_link_libraries(gameplay_world_mouse_activation_check PRIVATE openyamm_game openyamm_engine)
    add_executable(gameplay_bolster_check tests/UnitTestMain.cpp tests/GameplayBolsterRuntimeTests.cpp)
    target_include_directories(gameplay_bolster_check PRIVATE "${CMAKE_SOURCE_DIR}/third_party/doctest")
    target_link_libraries(gameplay_bolster_check PRIVATE openyamm_game openyamm_engine)
    add_executable(merge_bolster_profile_check tests/UnitTestMain.cpp tests/MergeBolsterProfileTests.cpp
        game/gameplay/MergeBolsterProfile.cpp)
    target_include_directories(merge_bolster_profile_check PRIVATE
        "${CMAKE_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/third_party/doctest")
    target_compile_definitions(merge_bolster_profile_check PRIVATE OPENYAMM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    add_executable(breach_maze_population_check tests/UnitTestMain.cpp tests/BreachMazePopulationTests.cpp)
    target_include_directories(breach_maze_population_check PRIVATE "${CMAKE_SOURCE_DIR}/third_party/doctest")
    target_compile_definitions(breach_maze_population_check PRIVATE OPENYAMM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(breach_maze_population_check PRIVATE openyamm_event_import openyamm_game openyamm_engine)
    add_executable(outdoor_load_ordering_check tests/UnitTestMain.cpp tests/OutdoorLoadOrderingTests.cpp)
    target_include_directories(outdoor_load_ordering_check PRIVATE
        "${CMAKE_BINARY_DIR}/generated" "${CMAKE_SOURCE_DIR}/third_party/doctest")
    target_link_libraries(outdoor_load_ordering_check PRIVATE openyamm_game openyamm_engine)
    add_executable(mm7_shoals_check tests/UnitTestMain.cpp tests/Mm7ShoalsTests.cpp
        tests/OutdoorUnderwaterMovementTests.cpp)
    target_include_directories(mm7_shoals_check PRIVATE
        "${CMAKE_BINARY_DIR}/generated" "${CMAKE_SOURCE_DIR}/third_party/doctest")
    target_compile_definitions(mm7_shoals_check PRIVATE OPENYAMM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(mm7_shoals_check PRIVATE openyamm_event_import openyamm_game openyamm_engine)
    add_executable(mm8_endgame_check tests/UnitTestMain.cpp tests/Mm8EndgameTests.cpp tests/Mm8HeartSourceTests.cpp
        tests/RegressionGameData.cpp)
    target_include_directories(mm8_endgame_check PRIVATE
        "${CMAKE_BINARY_DIR}/generated" "${CMAKE_SOURCE_DIR}/third_party/doctest")
    target_compile_definitions(mm8_endgame_check PRIVATE OPENYAMM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(mm8_endgame_check PRIVATE openyamm_event_import openyamm_game openyamm_engine)
    add_executable(event_inventory_consumption_check tests/UnitTestMain.cpp
        tests/EventInventoryConsumptionTests.cpp tests/RegressionGameData.cpp)
    target_include_directories(event_inventory_consumption_check PRIVATE
        "${CMAKE_BINARY_DIR}/generated" "${CMAKE_SOURCE_DIR}/third_party/doctest")
    target_compile_definitions(event_inventory_consumption_check PRIVATE OPENYAMM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(event_inventory_consumption_check PRIVATE openyamm_game openyamm_engine)
    add_executable(detached_party_member_check tests/UnitTestMain.cpp tests/DetachedPartyMemberTests.cpp)
    target_include_directories(detached_party_member_check PRIVATE "${CMAKE_SOURCE_DIR}/third_party/doctest")
    target_compile_definitions(detached_party_member_check PRIVATE OPENYAMM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(detached_party_member_check PRIVATE openyamm_game openyamm_engine)
    add_executable(party_regression_check tests/UnitTestMain.cpp tests/PartyRegressionTests.cpp
        tests/RegressionGameData.cpp)
    target_include_directories(party_regression_check PRIVATE "${CMAKE_SOURCE_DIR}/third_party/doctest")
    target_compile_definitions(party_regression_check PRIVATE OPENYAMM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(party_regression_check PRIVATE openyamm_game openyamm_engine)
    target_include_directories(town_portal_contact_check PRIVATE
        "${CMAKE_BINARY_DIR}/generated" "${CMAKE_SOURCE_DIR}/third_party/doctest")
    target_link_libraries(town_portal_contact_check PRIVATE openyamm_game openyamm_engine)
    add_executable(dimension_door_check tests/UnitTestMain.cpp tests/DimensionDoorActivationTests.cpp
        tests/ScriptedEventTimerTests.cpp)
    target_include_directories(dimension_door_check PRIVATE
        "${CMAKE_BINARY_DIR}/generated" "${CMAKE_SOURCE_DIR}/third_party/doctest")
    target_link_libraries(dimension_door_check PRIVATE openyamm_event_import openyamm_game openyamm_engine)
    add_executable(dimension_door_destination_check tests/UnitTestMain.cpp tests/DimensionDoorDestinationTests.cpp)
    target_include_directories(dimension_door_destination_check PRIVATE "${CMAKE_SOURCE_DIR}/third_party/doctest")
    target_compile_definitions(dimension_door_destination_check PRIVATE OPENYAMM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(dimension_door_destination_check PRIVATE openyamm_game openyamm_engine)
    add_executable(dimension_door_import_check tests/UnitTestMain.cpp tests/DimensionDoorImportTests.cpp)
    target_include_directories(dimension_door_import_check PRIVATE "${CMAKE_SOURCE_DIR}/third_party/doctest")
    target_compile_definitions(dimension_door_import_check PRIVATE OPENYAMM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(dimension_door_import_check PRIVATE openyamm_event_import)
    add_executable(harmondale_castle_import_check tests/UnitTestMain.cpp tests/HarmondaleCastleImportTests.cpp)
    target_include_directories(harmondale_castle_import_check PRIVATE
        "${CMAKE_BINARY_DIR}/generated" "${CMAKE_SOURCE_DIR}/third_party/doctest")
    target_compile_definitions(harmondale_castle_import_check PRIVATE OPENYAMM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(harmondale_castle_import_check PRIVATE openyamm_event_import openyamm_game openyamm_engine)
    add_executable(mm8_blackmail_import_check tests/UnitTestMain.cpp
        tests/Mm8BlackmailImportTests.cpp tests/SmugglersCoveImportTests.cpp)
    target_include_directories(mm8_blackmail_import_check PRIVATE
        "${CMAKE_BINARY_DIR}/generated" "${CMAKE_SOURCE_DIR}/third_party/doctest")
    target_compile_definitions(mm8_blackmail_import_check PRIVATE OPENYAMM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(mm8_blackmail_import_check PRIVATE openyamm_event_import openyamm_game openyamm_engine)
    add_executable(mm8_legacy_command_check tests/UnitTestMain.cpp tests/Mm8LegacyCommandTests.cpp)
    target_include_directories(mm8_legacy_command_check PRIVATE
        "${CMAKE_BINARY_DIR}/generated" "${CMAKE_SOURCE_DIR}/third_party/doctest")
    target_compile_definitions(mm8_legacy_command_check PRIVATE OPENYAMM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(mm8_legacy_command_check PRIVATE openyamm_event_import openyamm_game openyamm_engine)
    add_executable(mm8_blackmail_dialog_check tests/UnitTestMain.cpp
        tests/HouseDialogueRegressionTests.cpp tests/RegressionGameData.cpp)
    target_include_directories(mm8_blackmail_dialog_check PRIVATE
        "${CMAKE_BINARY_DIR}/generated" "${CMAKE_SOURCE_DIR}/third_party/doctest")
    target_compile_definitions(mm8_blackmail_dialog_check PRIVATE OPENYAMM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(mm8_blackmail_dialog_check PRIVATE openyamm_event_import openyamm_game openyamm_engine)
    add_executable(decoration_import tools/decoration_import_main.cpp)
    target_link_libraries(decoration_import PRIVATE openyamm_decoration_import)
    add_executable(ui_import tools/ui_import_main.cpp)
    target_link_libraries(ui_import PRIVATE openyamm_ui_import)
    add_executable(ui_texture_region_check tests/UiTextureRegionCheck.cpp)
    target_link_libraries(ui_texture_region_check PRIVATE openyamm_game openyamm_engine)
    add_executable(openyamm_text_check tools/AppleTextCheck.cpp)
    target_link_libraries(openyamm_text_check PRIVATE openyamm_game openyamm_engine)
    add_executable(menu_import tools/menu_import_main.cpp)
    target_link_libraries(menu_import PRIVATE openyamm_menu_import)
    add_executable(story_import tools/story_import_main.cpp)
    target_link_libraries(story_import PRIVATE openyamm_story_import)
    add_executable(core_import tools/core_import_main.cpp)
    target_link_libraries(core_import PRIVATE openyamm_core_import)
    add_executable(combat_import tools/combat_import_main.cpp)
    target_link_libraries(combat_import PRIVATE openyamm_combat_import)
    add_executable(service_import tools/service_import_main.cpp)
    target_link_libraries(service_import PRIVATE openyamm_service_import)
    add_executable(house_import tools/house_import_main.cpp)
    target_link_libraries(house_import PRIVATE openyamm_service_import)
    add_executable(media_import tools/media_import_main.cpp)
    target_link_libraries(media_import PRIVATE openyamm_media_import)
    add_executable(media_playback_check tests/MediaPlaybackCheck.cpp)
    target_link_libraries(media_playback_check PRIVATE openyamm_game openyamm_engine)
    add_executable(music_playback_check tests/MusicPlaybackCheck.cpp)
    target_link_libraries(music_playback_check PRIVATE openyamm_game openyamm_engine)
    add_executable(house_import_gameplay_check tests/HouseImportGameplayCheck.cpp)
    target_link_libraries(house_import_gameplay_check PRIVATE openyamm_game openyamm_engine)
    add_executable(journal_import tools/journal_import_main.cpp)
    target_link_libraries(journal_import PRIVATE openyamm_journal_import)
    add_executable(class_import tools/class_import_main.cpp)
    target_link_libraries(class_import PRIVATE openyamm_class_import)
    add_executable(inspection_import tools/inspection_import_main.cpp)
    target_link_libraries(inspection_import PRIVATE openyamm_inspection_import)
    add_executable(race_import tools/race_import_main.cpp)
    target_link_libraries(race_import PRIVATE openyamm_race_import)
    add_executable(inventory_import tools/inventory_import_main.cpp)
    target_link_libraries(inventory_import PRIVATE openyamm_inventory_import)
    add_executable(frame_import tools/frame_import_main.cpp)
    target_link_libraries(frame_import PRIVATE openyamm_frame_import)
    add_executable(fx_import tools/fx_import_main.cpp)
    target_link_libraries(fx_import PRIVATE openyamm_fx_import)
    add_executable(transition_import tools/transition_import_main.cpp)
    target_link_libraries(transition_import PRIVATE openyamm_transition_import)
    add_executable(roster_import tools/roster_import_main.cpp)
    target_link_libraries(roster_import PRIVATE openyamm_roster_import)
    add_executable(doll_import tools/doll_import_main.cpp)
    target_link_libraries(doll_import PRIVATE openyamm_doll_import)
    add_executable(teacher_import tools/teacher_import_main.cpp)
    target_link_libraries(teacher_import PRIVATE openyamm_teacher_import)
    add_executable(npc_import tools/npc_import_main.cpp)
    target_link_libraries(npc_import PRIVATE openyamm_npc_import)
    add_executable(support_import tools/support_import_main.cpp)
    target_link_libraries(support_import PRIVATE openyamm_support_import)
    add_executable(bolster_import tools/bolster_import_main.cpp)
    target_link_libraries(bolster_import PRIVATE openyamm_bolster_import)
    add_executable(voice_import tools/voice_import_main.cpp)
    target_link_libraries(voice_import PRIVATE openyamm_voice_import)
    add_executable(equipment_picture_import tools/equipment_picture_import_main.cpp)
    target_link_libraries(equipment_picture_import PRIVATE openyamm_equipment_picture_import)
    add_executable(world_rule_import tools/world_rule_import_main.cpp)
    target_link_libraries(world_rule_import PRIVATE openyamm_world_rule_import)
    add_executable(arcomage_import tools/arcomage_import_main.cpp)
    target_link_libraries(arcomage_import PRIVATE openyamm_arcomage_import)
    add_executable(sound_import tools/sound_import_main.cpp)
    target_link_libraries(sound_import PRIVATE openyamm_sound_import)
    add_executable(sound_playback_check tests/SoundPlaybackCheck.cpp)
    target_link_libraries(sound_playback_check PRIVATE openyamm_game openyamm_engine)
    add_executable(audio_cache_check tests/AudioCacheCheck.cpp)
    target_link_libraries(audio_cache_check PRIVATE openyamm_engine)
    add_executable(speech_import tools/speech_import_main.cpp)
    target_link_libraries(speech_import PRIVATE openyamm_speech_import)
    add_executable(speech_group_check tests/SpeechGroupCheck.cpp)
    target_include_directories(speech_group_check PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(speech_group_check PRIVATE openyamm_game)
    add_executable(sound_catalog_check tests/SoundCatalogCheck.cpp)
    target_include_directories(sound_catalog_check PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(sound_catalog_check PRIVATE openyamm_game)
    add_executable(arcomage_binary_check tests/ArcomageBinaryCheck.cpp)
    target_include_directories(arcomage_binary_check PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(arcomage_binary_check PRIVATE openyamm_game)
    add_executable(openyamm_core_check tools/AppleCoreCheck.cpp)
    target_link_libraries(openyamm_core_check PRIVATE openyamm_game openyamm_engine)
    add_executable(openyamm_story_check tools/AppleStoryCheck.cpp tools/AppleStoryCheckMain.cpp)
    target_link_libraries(openyamm_story_check PRIVATE openyamm_game openyamm_engine)
    add_executable(sprite_import tools/sprite_import_main.cpp)
    target_link_libraries(sprite_import PRIVATE openyamm_menu_import)
    add_executable(outdoor_import tools/outdoor_import_main.cpp)
    target_link_libraries(outdoor_import PRIVATE openyamm_outdoor_import)
    add_executable(openyamm_outdoor_preview tools/AppleOutdoorPreview.cpp tools/AppleOutdoorPreviewMain.cpp)
    target_link_libraries(openyamm_outdoor_preview PRIVATE openyamm_game openyamm_engine)
    add_dependencies(openyamm_outdoor_preview openyamm_runtime_shaders)
    add_executable(openyamm_input_check tools/AppleInputCheck.cpp)
    target_link_libraries(openyamm_input_check PRIVATE openyamm_game openyamm_engine)
    add_executable(application_scene_capture tools/ApplicationSceneCapture.cpp)
    target_link_libraries(application_scene_capture PRIVATE openyamm_game openyamm_engine)
    add_executable(mac_endurance_capture tools/MacEnduranceCapture.cpp)
    target_link_libraries(mac_endurance_capture PRIVATE openyamm_game openyamm_engine)
    add_executable(imported_map_capture tools/ImportedMapCapture.cpp)
    target_link_libraries(imported_map_capture PRIVATE openyamm_game openyamm_engine)
    add_executable(merge_quest_capture tools/MergeQuestCapture.cpp)
    target_link_libraries(merge_quest_capture PRIVATE openyamm_game openyamm_engine)
    add_executable(connector_stone_capture tools/ConnectorStoneCapture.cpp)
    target_link_libraries(connector_stone_capture PRIVATE openyamm_game openyamm_engine)
    add_executable(continent_reward_capture tools/ContinentRewardCapture.cpp)
    target_link_libraries(continent_reward_capture PRIVATE openyamm_game openyamm_engine)
    add_executable(mm6_hive_capture tools/Mm6HiveCapture.cpp)
    target_link_libraries(mm6_hive_capture PRIVATE openyamm_game openyamm_engine)
    add_executable(mm7_shoals_capture tools/Mm7ShoalsCapture.cpp)
    target_link_libraries(mm7_shoals_capture PRIVATE openyamm_game openyamm_engine)
    add_executable(mm8_endgame_capture tools/Mm8EndgameCapture.cpp)
    target_link_libraries(mm8_endgame_capture PRIVATE openyamm_event_import openyamm_game openyamm_engine)
    add_executable(journal_text_capture tools/JournalTextCapture.cpp)
    target_link_libraries(journal_text_capture PRIVATE openyamm_game openyamm_engine)
    add_executable(breach_alvar_capture tools/BreachAlvarCapture.cpp)
    target_link_libraries(breach_alvar_capture PRIVATE openyamm_game openyamm_engine)
    add_executable(source_jump_capture tools/SourceJumpCapture.cpp)
    target_link_libraries(source_jump_capture PRIVATE openyamm_game openyamm_engine)
    add_executable(br_alvar_daylight_capture tools/BrAlvarDaylightCapture.cpp)
    target_link_libraries(br_alvar_daylight_capture PRIVATE openyamm_game openyamm_engine)
    add_executable(br_alvar_admission_capture tools/BrAlvarAdmissionCapture.cpp)
    target_link_libraries(br_alvar_admission_capture PRIVATE openyamm_game openyamm_engine)

    add_executable(breach_continuation_capture tools/BreachContinuationCapture.cpp)
    target_link_libraries(breach_continuation_capture PRIVATE openyamm_game openyamm_engine)
    add_executable(breach_mercenary_capture tools/BreachMercenaryCapture.cpp)
    target_link_libraries(breach_mercenary_capture PRIVATE openyamm_game openyamm_engine)
    add_executable(lambent_castle_capture tools/LambentCastleCapture.cpp)
    target_link_libraries(lambent_castle_capture PRIVATE openyamm_game openyamm_engine)
    add_executable(archibald_capture tools/ArchibaldCapture.cpp)
    target_link_libraries(archibald_capture PRIVATE openyamm_game openyamm_engine)
    add_executable(regna_cannon_capture tools/RegnaCannonCapture.cpp)
    target_link_libraries(regna_cannon_capture PRIVATE openyamm_game openyamm_engine)
    add_executable(temple_bottle_capture tools/TempleBottleCapture.cpp)
    target_link_libraries(temple_bottle_capture PRIVATE openyamm_game openyamm_engine)
    add_executable(prerequisite_capture tools/PrerequisiteCapture.cpp)
    target_link_libraries(prerequisite_capture PRIVATE openyamm_game openyamm_engine)
    add_executable(varn_password_capture tools/VarnPasswordCapture.cpp)
    target_link_libraries(varn_password_capture PRIVATE openyamm_game openyamm_engine)
    add_executable(submarine_journey_capture tools/SubmarineJourneyCapture.cpp)
    target_link_libraries(submarine_journey_capture PRIVATE openyamm_game openyamm_engine)
    add_executable(source_jump_state_check tests/UnitTestMain.cpp tests/GameplayRuleRegressionTests.cpp
        tests/RegressionGameData.cpp)
    target_include_directories(source_jump_state_check PRIVATE "${CMAKE_SOURCE_DIR}/third_party/doctest")
    target_compile_definitions(source_jump_state_check PRIVATE OPENYAMM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(source_jump_state_check PRIVATE openyamm_game openyamm_engine)

    add_executable(video_streaming_check tests/VideoStreamingTests.cpp)
    target_include_directories(video_streaming_check PRIVATE "${CMAKE_SOURCE_DIR}/third_party/doctest")
    target_compile_definitions(video_streaming_check PRIVATE OPENYAMM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(video_streaming_check PRIVATE openyamm_game openyamm_engine)
    add_executable(runaway_chaos_capture tools/RunawayChaosCapture.cpp)
    target_link_libraries(runaway_chaos_capture PRIVATE openyamm_game openyamm_engine)
    add_executable(loading_ui_capture tools/LoadingUiCapture.cpp)
    target_link_libraries(loading_ui_capture PRIVATE openyamm_game openyamm_engine)
    add_executable(cross_world_travel_capture tools/CrossWorldTravelCapture.cpp)
    target_link_libraries(cross_world_travel_capture PRIVATE openyamm_game openyamm_engine)
    add_executable(house_dialogue_layout_capture tools/HouseDialogueLayoutCapture.cpp)
    target_link_libraries(house_dialogue_layout_capture PRIVATE openyamm_game openyamm_engine)
    add_executable(house_dialogue_layout_check tests/UnitTestMain.cpp tests/HouseDialogueLayoutTests.cpp)
    target_include_directories(house_dialogue_layout_check PRIVATE ${CMAKE_SOURCE_DIR}/third_party/doctest)
    target_link_libraries(house_dialogue_layout_check PRIVATE openyamm_game openyamm_engine)
    add_executable(openyamm_creation_check tools/NewGameCreationCheck.cpp)
    target_link_libraries(openyamm_creation_check PRIVATE openyamm_game openyamm_engine)
    add_executable(openyamm_route_plan tools/OutdoorRoutePlan.cpp)
    target_link_libraries(openyamm_route_plan PRIVATE openyamm_game openyamm_engine)
    add_executable(openyamm_combat_check tools/NormalCombatCheck.cpp)
    target_link_libraries(openyamm_combat_check PRIVATE openyamm_game openyamm_engine)
    add_executable(rest_clock_check tests/UnitTestMain.cpp tests/RestClockTests.cpp)
    target_include_directories(rest_clock_check PRIVATE ${CMAKE_SOURCE_DIR}/third_party/doctest)
    target_link_libraries(rest_clock_check PRIVATE openyamm_game openyamm_engine)
    add_executable(travel_prompt_check tests/UnitTestMain.cpp tests/TravelPromptTests.cpp)
    target_include_directories(travel_prompt_check PRIVATE ${CMAKE_SOURCE_DIR}/third_party/doctest)
    target_link_libraries(travel_prompt_check PRIVATE openyamm_game openyamm_engine)
    add_executable(scripted_map_travel_check tests/UnitTestMain.cpp tests/TravelPromptTests.cpp
        tests/ScriptedMapTravelTests.cpp tests/RegressionGameData.cpp)
    target_include_directories(scripted_map_travel_check PRIVATE
        "${CMAKE_BINARY_DIR}/generated" "${CMAKE_SOURCE_DIR}/third_party/doctest")
    target_compile_definitions(scripted_map_travel_check PRIVATE OPENYAMM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(scripted_map_travel_check PRIVATE openyamm_game openyamm_engine)
    add_executable(journal_text_layout_check tests/UnitTestMain.cpp tests/JournalTextLayoutTests.cpp
        game/ui/JournalTextLayout.cpp)
    target_include_directories(journal_text_layout_check PRIVATE
        "${CMAKE_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/third_party/doctest")
    add_executable(history_event_check
        tests/UnitTestMain.cpp tests/HistoryEventTests.cpp tests/HistorySessionTests.cpp)
    target_include_directories(history_event_check PRIVATE ${CMAKE_SOURCE_DIR}/third_party/doctest)
    target_link_libraries(history_event_check PRIVATE openyamm_game openyamm_engine)
    add_executable(goblinwatch_event_check
        tests/UnitTestMain.cpp
        tests/ScriptedMapRegressionTests.cpp
        tests/RegressionGameData.cpp
        tests/RegressionMapLoader.cpp)
    target_include_directories(goblinwatch_event_check PRIVATE ${CMAKE_SOURCE_DIR}/third_party/doctest)
    target_compile_definitions(goblinwatch_event_check PRIVATE OPENYAMM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(goblinwatch_event_check PRIVATE openyamm_game openyamm_engine)
    add_executable(mercenary_refill_check tests/UnitTestMain.cpp tests/MercenaryRefillTests.cpp)
    target_include_directories(mercenary_refill_check PRIVATE ${CMAKE_SOURCE_DIR}/third_party/doctest)
    target_link_libraries(mercenary_refill_check PRIVATE openyamm_game openyamm_engine)
    add_executable(outdoor_step_check tests/UnitTestMain.cpp tests/OutdoorStepUpTests.cpp)
    target_include_directories(outdoor_step_check PRIVATE ${CMAKE_SOURCE_DIR}/third_party/doctest)
    target_link_libraries(outdoor_step_check PRIVATE openyamm_game openyamm_engine)
    add_executable(chest_open_state_check tests/UnitTestMain.cpp tests/ChestOpenStateTests.cpp)
    target_include_directories(chest_open_state_check PRIVATE ${CMAKE_SOURCE_DIR}/third_party/doctest)
    target_link_libraries(chest_open_state_check PRIVATE openyamm_game openyamm_engine)

    add_executable(turn_based_combat_check tests/UnitTestMain.cpp tests/TurnBasedCombatRuntimeTests.cpp)
    target_include_directories(turn_based_combat_check PRIVATE ${CMAKE_SOURCE_DIR}/third_party/doctest)
    target_link_libraries(turn_based_combat_check PRIVATE openyamm_game openyamm_engine)
    add_executable(combat_health_bar_check tests/UnitTestMain.cpp tests/CombatActorHealthBarPolicyTests.cpp)
    target_include_directories(combat_health_bar_check PRIVATE ${CMAKE_SOURCE_DIR}/third_party/doctest)
    target_link_libraries(combat_health_bar_check PRIVATE openyamm_game openyamm_engine)
    add_executable(party_health_check tests/UnitTestMain.cpp tests/PartyHealthPersistenceTests.cpp)
    target_include_directories(party_health_check PRIVATE ${CMAKE_SOURCE_DIR}/third_party/doctest)
    target_link_libraries(party_health_check PRIVATE openyamm_game openyamm_engine)
    add_executable(shop_sale_check tests/UnitTestMain.cpp tests/ShopSaleResultTests.cpp)
    target_include_directories(shop_sale_check PRIVATE ${CMAKE_SOURCE_DIR}/third_party/doctest)
    target_link_libraries(shop_sale_check PRIVATE openyamm_game openyamm_engine)
    add_executable(image_asset_check tests/UnitTestMain.cpp tests/ImageAssetLoaderTests.cpp)
    target_include_directories(image_asset_check PRIVATE ${CMAKE_SOURCE_DIR}/third_party/doctest)
    target_compile_definitions(image_asset_check PRIVATE OPENYAMM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(image_asset_check PRIVATE openyamm_engine)
    add_executable(combat_text_check tests/UnitTestMain.cpp tests/CombatTextTableTests.cpp)
    target_include_directories(combat_text_check PRIVATE ${CMAKE_SOURCE_DIR}/third_party/doctest)
    target_link_libraries(combat_text_check PRIVATE openyamm_game openyamm_engine)
    add_executable(temple_localization_check
        tests/UnitTestMain.cpp
        tests/HouseServiceTextTableTests.cpp
        tests/TempleLocalizationRuntimeTests.cpp)
    target_include_directories(temple_localization_check PRIVATE ${CMAKE_SOURCE_DIR}/third_party/doctest)
    target_compile_definitions(temple_localization_check PRIVATE OPENYAMM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(temple_localization_check PRIVATE openyamm_game openyamm_engine)
    add_executable(openyamm_spell_check tools/NormalSpellCheck.cpp)
    target_link_libraries(openyamm_spell_check PRIVATE openyamm_game openyamm_engine)
    add_executable(openyamm_save_inspect tools/SaveGameInspect.cpp)
    target_link_libraries(openyamm_save_inspect PRIVATE openyamm_game openyamm_engine)
    add_executable(openyamm_metal_smoke tools/AppleMetalSmoke.cpp tools/AppleMetalSmokeMain.cpp)
    target_link_libraries(openyamm_metal_smoke PRIVATE openyamm_engine)
    target_compile_definitions(openyamm_metal_smoke PRIVATE
        OPENYAMM_SMOKE_SHADER_DIR="${OPENYAMM_RUNTIME_SHADER_DIR}/metal")
    add_dependencies(openyamm_metal_smoke openyamm_runtime_shaders)
    add_executable(apple_import tools/apple_import_main.mm ios/ResourceImport.mm)
    target_include_directories(apple_import PRIVATE "${CMAKE_SOURCE_DIR}")
    target_compile_options(apple_import PRIVATE -fobjc-arc)
    target_link_libraries(apple_import PRIVATE openyamm_runtime_import "-framework Foundation")
endif()

if (ANDROID)
    add_subdirectory(android/app/src/main/cpp)
endif()
if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
    add_subdirectory(ios)
endif()

set(OPENYAMM_RELEASE_SYSTEM_RUNTIME_DLLS "")
set(OPENYAMM_RELEASE_COLLECT_RUNTIME_DEPENDENCIES OFF)
set(OPENYAMM_RELEASE_RUNTIME_SEARCH_DIRS "")
if (WIN32)
    include(InstallRequiredSystemLibraries)
    string(REPLACE ";" "|" OPENYAMM_RELEASE_SYSTEM_RUNTIME_DLLS "${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS}")
    get_filename_component(OPENYAMM_RELEASE_COMPILER_RUNTIME_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set(OPENYAMM_RELEASE_COLLECT_RUNTIME_DEPENDENCIES ON)
    set(OPENYAMM_RELEASE_RUNTIME_SEARCH_DIRS "${OPENYAMM_RELEASE_COMPILER_RUNTIME_DIR}")
endif()

set(OPENYAMM_RELEASE_ROOT_DIR "${CMAKE_BINARY_DIR}/release")
set(OPENYAMM_RELEASE_STAGE_DIR "${OPENYAMM_RELEASE_ROOT_DIR}/$<CONFIG>/OpenYAMM")
set(OPENYAMM_RELEASE_ZIP_PATH "${OPENYAMM_RELEASE_ROOT_DIR}/OpenYAMM-windows-$<CONFIG>.zip")
set(OPENYAMM_FLATPAK_OUTPUT_DIR
    "${CMAKE_BINARY_DIR}/flatpak"
    CACHE PATH
    "Output directory for local Flatpak release builds."
)

if (OPENYAMM_BUILD_DESKTOP_EXECUTABLE)
    add_custom_target(openyamm_windows_release_zip
        COMMAND "${CMAKE_COMMAND}"
            "-DOPENYAMM_RELEASE_STAGE_DIR=${OPENYAMM_RELEASE_STAGE_DIR}"
            "-DOPENYAMM_RELEASE_ZIP_PATH=${OPENYAMM_RELEASE_ZIP_PATH}"
            "-DOPENYAMM_RELEASE_EXECUTABLE=$<TARGET_FILE:openyamm>"
            "-DOPENYAMM_RELEASE_RUNTIME_DLLS=$<JOIN:$<TARGET_RUNTIME_DLLS:openyamm>,|>"
            "-DOPENYAMM_RELEASE_SYSTEM_RUNTIME_DLLS=${OPENYAMM_RELEASE_SYSTEM_RUNTIME_DLLS}"
            "-DOPENYAMM_RELEASE_COLLECT_RUNTIME_DEPENDENCIES=${OPENYAMM_RELEASE_COLLECT_RUNTIME_DEPENDENCIES}"
            "-DOPENYAMM_RELEASE_RUNTIME_SEARCH_DIRS=${OPENYAMM_RELEASE_RUNTIME_SEARCH_DIRS}"
            "-DOPENYAMM_RELEASE_SHADER_DIR=${OPENYAMM_RUNTIME_SHADER_DIR}"
            "-DOPENYAMM_RELEASE_ASSET_DEV_DIR=${OPENYAMM_DEV_ASSETS_DIR}"
            "-DOPENYAMM_RELEASE_SETTINGS_TEMPLATE=${CMAKE_SOURCE_DIR}/settings_release.ini"
            -P "${CMAKE_SOURCE_DIR}/cmake/CreateOpenYammReleasePackage.cmake"
        DEPENDS openyamm openyamm_runtime_shaders
        VERBATIM
    )

    add_custom_target(openyamm_flatpak_release
        COMMAND "${CMAKE_SOURCE_DIR}/packaging/flatpak/build_flatpak.sh"
            "--output-dir=${OPENYAMM_FLATPAK_OUTPUT_DIR}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        USES_TERMINAL
        VERBATIM
    )

    add_custom_target(openyamm_flatpak_clean
        COMMAND "${CMAKE_SOURCE_DIR}/packaging/flatpak/build_flatpak.sh"
            "--output-dir=${OPENYAMM_FLATPAK_OUTPUT_DIR}"
            "--clean-only"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        USES_TERMINAL
        VERBATIM
    )
endif()

if (OPENYAMM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if (OPENYAMM_BUILD_EDITOR)
    add_subdirectory(editor)
endif()

if (OPENYAMM_BUILD_TOOLS)
    add_subdirectory(tools)
endif()
