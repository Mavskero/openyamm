#include "engine/AssetFileSystem.h"
#include "game/app/GameApplication.h"
#include "game/data/GameDataLoader.h"
#include "game/gameplay/GameplayHeldItemController.h"
#include "game/ui/Utf8Text.h"
#include "game/ui/screens/NewGameScreen.h"

#include <algorithm>
#include <filesystem>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace OpenYAMM::Game
{
struct NewGameScreenTestAccess
{
    static void require(bool condition, const std::string &message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    static void input(NewGameScreen &screen, const char *pText)
    {
        SDL_Event event = {};
        event.type = SDL_EVENT_TEXT_INPUT;
        event.text.text = pText;
        screen.handleSdlEvent(event);
    }

    static void backspace(NewGameScreen &screen, bool repeated = false)
    {
        SDL_Event event = {};
        event.type = SDL_EVENT_KEY_DOWN;
        event.key.key = SDLK_BACKSPACE;
        event.key.repeat = repeated;
        screen.handleSdlEvent(event);
    }

    static void checkName(NewGameScreen &screen)
    {
        screen.m_state.name = "";
        screen.beginNameEditing();
        const SDL_Rect compactArea = screen.textInputArea(640, 480);
        const SDL_Rect ipadArea = screen.textInputArea(1210, 834);
        const SDL_Rect retinaArea = screen.textInputArea(2420, 1668);
        require(compactArea.x == 72 && compactArea.y == 77 && compactArea.w == 110 && compactArea.h == 24
            && ipadArea.x == 174 && ipadArea.y == 134 && ipadArea.w == 191 && ipadArea.h == 42
            && retinaArea.x == 348 && retinaArea.y == 268 && retinaArea.w == 382 && retinaArea.h == 83,
            "Character editor keyboard area does not match the authored name field at different window sizes");
        input(screen, "中文骑士");
        require(screen.m_state.nameEditBuffer == "中文骑士", "Character editor dropped committed Chinese input");
        backspace(screen);
        require(screen.m_state.nameEditBuffer == "中文骑", "Backspace split a Chinese character");
        backspace(screen, true);
        require(screen.m_state.nameEditBuffer == "中文骑", "SDL key repeat duplicated manual name deletion");
        screen.updateNameBackspaceRepeat(0.36f);
        require(screen.m_state.nameEditBuffer == "中文", "Held backspace split a Chinese character");
        SDL_Event release = {};
        release.type = SDL_EVENT_KEY_UP;
        release.key.key = SDLK_BACKSPACE;
        screen.handleSdlEvent(release);
        screen.updateNameBackspaceRepeat(1.0f);
        require(screen.m_state.nameEditBuffer == "中文", "Released backspace kept deleting");
        input(screen, "\xe4\xb8");
        input(screen, "\n\t\x7f");
        require(screen.m_state.nameEditBuffer == "中文", "Invalid UTF-8 or controls changed the character name");
        screen.endNameEditing(true);
        require(screen.m_state.name == "中文" && !screen.textInputActive(), "Chinese name commit failed");
        require(screen.textInputArea(1210, 834).w == 0, "Inactive editor retained a keyboard avoidance area");
        screen.beginNameEditing();
        input(screen, "取消");
        screen.endNameEditing(false);
        require(screen.m_state.name == "中文", "Canceled editing changed the character name");
        input(screen, "未编辑");
        require(screen.m_state.name == "中文", "Inactive editor accepted text");

        screen.m_state.name = "";
        screen.beginNameEditing();
        input(screen, "一二三四五六七八九十一二三四五六");
        require(screen.m_state.nameEditBuffer == "一二三四五六七八九十一二三四五",
            "Character name limit must be 15 Unicode code points, not 15 bytes");
        backspace(screen);
        input(screen, "𠮷");
        require(utf8CharacterCount(screen.m_state.nameEditBuffer) == 15
            && screen.m_state.nameEditBuffer.ends_with("𠮷"), "Four-byte name character did not fit the final slot");
        backspace(screen);
        require(utf8CharacterCount(screen.m_state.nameEditBuffer) == 14, "Four-byte name deletion corrupted UTF-8");
        screen.endNameEditing(false);
        screen.m_state.name = "O'Brien-2";
        screen.beginNameEditing();
        input(screen, " 甲");
        screen.endNameEditing(true);
        require(screen.m_state.name == "O'Brien-2 甲", "Mixed Latin and Chinese name was not preserved");
        screen.beginNameEditing();
        backspace(screen);
        const std::string beforeFocusLoss = screen.m_state.nameEditBuffer;
        SDL_Event lostFocus = {};
        lostFocus.type = SDL_EVENT_WINDOW_FOCUS_LOST;
        screen.handleSdlEvent(lostFocus);
        screen.updateNameBackspaceRepeat(2.0f);
        require(screen.m_state.nameEditBuffer == beforeFocusLoss, "Focus loss left backspace deleting name characters");
        screen.endNameEditing(false);
    }

    static std::vector<Character> checkParty(const Engine::AssetFileSystem &files, const GameDataRepository &data,
        const std::string &continent, uint32_t expectedContinentId, const std::string &className = "Knight",
        bool extraSchools = false)
    {
        std::vector<Character> created;
        uint32_t continentId = 0;
        bool debugLoadout = true;
        NewGameScreen screen(files, nullptr, data, false, false, false,
            [&](const std::vector<Character> &characters, uint32_t selectedContinent, bool preserveDebugLoadout)
            {
                created = characters;
                continentId = selectedContinent;
                debugLoadout = preserveDebugLoadout;
            }, [] {});
        screen.selectContinent(continent);
        require(screen.m_stage == NewGameScreen::FlowStage::CharacterCreation,
            "Imported continent could not enter character creation: " + continent);
        require(screen.candidateCount() > 0 && !screen.isGodLichSelected(), "No normal creation candidates");
        for (size_t index = 0; index < screen.selectedCandidate().availableClassIds.size()
            && screen.selectedClassName() != className; ++index)
        {
            screen.cycleClass(1);
        }
        require(screen.selectedClassName() == className, "Requested normal class is unavailable: " + className);
        checkName(screen);
        screen.confirmCreation();
        require(created.empty(), "Incomplete party bypassed creation requirements");

        const int initialPool = screen.currentBonusPool();
        require(initialPool > 0, "Normal party has no bonus points to allocate");
        if (className == "Sorcerer")
        {
            while (screen.tryIncreaseStat(NewGameScreen::StatId::Intellect))
            {
            }
        }
        for (size_t index = 0; index < static_cast<size_t>(NewGameScreen::StatId::Count); ++index)
        {
            while (screen.tryIncreaseStat(static_cast<NewGameScreen::StatId>(index)))
            {
            }
        }
        require(screen.currentBonusPool() == 0, "Cannot allocate normal character's bonus points");
        require(!screen.tryIncreaseStat(NewGameScreen::StatId::Luck), "Stat increase overspent the bonus pool");
        require(screen.m_state.optionalSkills.size() >= 3, "Imported normal candidate lacks expected optional skills");
        screen.confirmCreation();
        require(created.empty(), "Missing optional skills allowed normal party creation");
        std::vector<std::string> options = screen.m_state.optionalSkills;
        if (extraSchools)
        {
            size_t slot = 0;
            for (const char *pSkill : {"AirMagic", "WaterMagic"})
            {
                const auto found = std::find(options.begin(), options.end(), pSkill);
                require(found != options.end(), "Normal caster cannot select the requested optional magic school");
                std::iter_swap(options.begin() + slot++, found);
            }
        }
        require(screen.tryToggleOptionalSkill(options[0]) && screen.tryToggleOptionalSkill(options[1]),
            "Could not select two optional skills");
        require(!screen.tryToggleOptionalSkill(options[2]), "Third optional skill was accepted");
        require(screen.tryToggleOptionalSkill(options[1]), "Could not deselect an optional skill");
        screen.confirmCreation();
        require(created.empty(), "One optional skill allowed normal party creation");
        require(screen.tryToggleOptionalSkill(options[1]), "Could not reselect an optional skill");
        screen.m_state.name = "";
        screen.beginNameEditing();
        const char *pName = className == "Sorcerer" ? "中文法师" : "中文骑士";
        input(screen, pName);
        screen.confirmCreation();
        require(created.size() == 1 && continentId == expectedContinentId && !debugLoadout,
            "Normal creation selected wrong continent, member count or debug loadout");
        const Character &member = created.front();
        require(member.name == pName && member.className == className && member.level == 1 && member.skillPoints == 0,
            "Normal creation lost Chinese name or created debug levels");
        require(member.skills.size() == screen.m_state.defaultSkills.size() + 2,
            "Normal creation did not preserve selected skill count");
        require(member.skills.contains(options[0]) && member.skills.contains(options[1])
            && !member.skills.contains(options[2]), "Normal creation changed chosen skill identities");
        for (const auto &[name, skill] : member.skills)
        {
            require(skill.level == 1 && skill.mastery == SkillMastery::Normal,
                "Normal creation gave a nonstarting skill level: " + name);
        }
        require(member.might == uint32_t(screen.m_state.currentStats[0])
            && member.luck == uint32_t(screen.m_state.currentStats[6]), "Created stats differ from allocation");
        std::cout << "Creation passed: " << continent << ", candidates=" << screen.candidateCount()
            << ", class=" << member.className << ", name=" << member.name
            << ", level=" << member.level << ", skills=" << member.skills.size() << '\n';
        return created;
    }

    static std::vector<Character> checkFourMemberParty(const Engine::AssetFileSystem &files,
        const GameDataRepository &data, const std::string &world)
    {
        const uint32_t expectedContinent = world == "mm6" ? 3 : world == "mm7" ? 2 : 1;
        std::vector<Character> created;
        NewGameScreen screen(files, nullptr, data, false, false, false,
            [&](const std::vector<Character> &members, uint32_t continent, bool debugLoadout)
            {
                require(continent == expectedContinent && !debugLoadout, "Four-member creation changed world/loadout");
                created = members;
            }, [] {});
        screen.selectContinent(world == "mm6" ? "enroth" : world == "mm7" ? "antagarich" : "jadame");
        const std::array<const char *, 4> classes = world == "mm8"
            ? std::array<const char *, 4>{"Knight", "Priest", "Necromancer", "Vampire"}
            : std::array<const char *, 4>{"Knight", "Cleric", "Sorcerer", "Archer"};
        const std::array<const char *, 4> names = world == "mm8"
            ? std::array<const char *, 4>{"探路骑士", "晨光牧师", "幽影法师", "夜行血族"}
            : std::array<const char *, 4>{"探路骑士", "晨光牧师", "火焰法师", "远行弓手"};
        for (size_t slot = 0; slot < classes.size(); ++slot)
        {
            if (slot != 0) screen.addPartySlot();
            require(screen.m_partySize == slot + 1 && screen.m_activePartySlot == slot,
                "Add member did not select the new party slot");
            if (world == "mm8")
            {
                // Jadame offers its original classes and race restrictions. Find an eligible
                // candidate through the normal screen instead of changing the continent profile.
                for (size_t candidate = 0; candidate < screen.candidateCount(); ++candidate)
                {
                    for (size_t index = 0; index < screen.selectedCandidate().availableClassIds.size()
                        && screen.selectedClassName() != classes[slot]; ++index)
                    {
                        screen.cycleClass(1);
                    }
                    if (screen.selectedClassName() == classes[slot] && !screen.isGodLichSelected())
                    {
                        break;
                    }
                    screen.cycleCandidate(1);
                }
            }
            else
            {
                // Preserve the accepted MM6/MM7 routes and their four original portraits.
                const uint32_t portraitId = uint32_t(38 + slot);
                for (size_t index = 0; index < screen.candidateCount()
                    && screen.selectedCandidate().characterDataId != portraitId; ++index)
                {
                    screen.cycleCandidate(1);
                }
                require(screen.selectedCandidate().characterDataId == portraitId && !screen.isGodLichSelected(),
                    "Requested original portrait is unavailable in normal creation");
            }
            for (size_t index = 0; index < screen.selectedCandidate().availableClassIds.size()
                && screen.selectedClassName() != classes[slot]; ++index)
            {
                screen.cycleClass(1);
            }
            require(screen.selectedClassName() == classes[slot],
                "Requested normal party class is unavailable: " + std::string(classes[slot]));
            const auto primary = slot == 1 ? NewGameScreen::StatId::Personality
                : slot == 2 ? NewGameScreen::StatId::Intellect : NewGameScreen::StatId::Might;
            while (screen.tryIncreaseStat(primary)) {}
            for (size_t stat = 0; stat < size_t(NewGameScreen::StatId::Count); ++stat)
            {
                while (screen.tryIncreaseStat(NewGameScreen::StatId(stat))) {}
            }
            require(screen.currentBonusPool() == 0, "Party member still has unallocated bonus points");
            const std::vector<std::string> options = screen.m_state.optionalSkills;
            require(options.size() >= 2 && screen.tryToggleOptionalSkill(options[0])
                && screen.tryToggleOptionalSkill(options[1]), "Party member cannot select two normal skills");
            screen.m_state.name.clear();
            screen.beginNameEditing();
            input(screen, names[slot]);
            screen.endNameEditing(true);
        }
        for (size_t slot = 0; slot < classes.size(); ++slot)
        {
            screen.switchActivePartySlot(slot);
            require(screen.m_state.name == names[slot] && screen.selectedClassName() == classes[slot]
                && screen.currentBonusPool() == 0 && screen.m_state.selectedOptionalSkills.size() == 2,
                "Switching creation slots lost name, class, stats or skills");
        }
        screen.confirmCreation();
        require(created.size() == classes.size(), "Normal creation did not return all four members");
        for (size_t slot = 0; slot < created.size(); ++slot)
        {
            const Character &member = created[slot];
            require(member.name == names[slot] && member.className == classes[slot] && member.level == 1
                && member.experience == 0 && member.skillPoints == 0 && member.health == member.maxHealth,
                "Four-member creation changed identity or starting progression");
            for (const auto &[name, skill] : member.skills)
            {
                require(skill.level == 1 && skill.mastery == SkillMastery::Normal,
                    "Four-member creation gave advanced skills");
            }
            std::cout << "Normal party member " << slot << ": " << member.name << " class=" << member.className
                << " portrait=" << member.characterDataId << " hp=" << member.health
                << " mana=" << member.spellPoints << '\n';
        }
        return created;
    }
};

struct NewGameSessionTestAccess
{
    static void checkNormalSession(Engine::AssetFileSystem &files, const std::vector<Character> &characters,
        const std::filesystem::path &root, const std::filesystem::path &output,
        const std::optional<std::filesystem::path> &reload, const std::string &world)
    {
        const auto require = &NewGameScreenTestAccess::require;
        require(world == "mm6" || world == "mm7" || world == "mm8", "Unsupported normal-session diagnostic world");
        const uint32_t continentId = world == "mm6" ? 3 : world == "mm7" ? 2 : 1;
        const std::string expectedMap = world == "mm6" ? "oute3.odm" : world == "mm7" ? "7out01.odm" : "out01.odm";
        const float expectedX = world == "mm6" ? -9728.0f : world == "mm7" ? 12552.0f : 3560.0f;
        const float expectedY = world == "mm6" ? -11319.0f : world == "mm7" ? 800.0f : 7696.0f;
        require(!std::filesystem::exists(output), "Session check output must be a new directory");
        std::filesystem::create_directories(output);
        std::filesystem::current_path(output);
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        require(setenv("OPENYAMM_DISABLE_LOADING_OVERLAY", "1", 1) == 0, "Cannot disable headless loading graphics");
        Engine::ApplicationConfig config = Engine::ApplicationConfig::createDefault();
        config.activeWorldId = "mm6";
        config.assetRoot = root.string();
        GameSettings settings = GameSettings::createDefault();
        settings.startInMainMenu = true;
        settings.preseedParty = false;
        settings.immortal = false;
        settings.unlimitedMana = false;
        settings.debugConsole = false;
        std::string settingsError;
        require(saveGameSettings(output / "settings.ini", settings, settingsError),
            "Cannot write isolated normal-play settings: " + settingsError);
        GameApplication application(config);
        require(application.loadGameData(files), "Cannot initialize the actual game application");
        const bool started = reload
            ? application.quickLoadFromPath(*reload, false)
            : application.startNewSessionFromCharacterCreation(characters, continentId, false, false);
        require(started, "Actual game application could not start or reload the normal created party");
        if (reload)
        {
            std::string saveError;
            const std::optional<GameSaveData> expected = loadGameDataFromPath(*reload, saveError);
            require(expected.has_value(), "Cannot inspect input save: " + saveError);
            require(std::abs(application.m_outdoorGameView.cameraYawRadians() - expected->outdoorCameraYawRadians)
                    < 0.00001f
                && std::abs(application.m_outdoorGameView.cameraPitchRadians() - expected->outdoorCameraPitchRadians)
                    < 0.00001f, "Headless reload lost saved camera orientation");
        }
        require(application.m_gameSession.currentMapFileName() == expectedMap
            && application.m_activeWorldManifest.id == world && files.getActiveWorldId() == world,
            "Creation/reload did not activate the selected continent's map and resource world");
        GameplayScreenRuntime &runtime = application.m_gameSession.gameplayScreenRuntime();
        EventRuntimeState *pEvents = application.m_gameplayController.eventRuntimeState();
        require(pEvents != nullptr && runtime.party() != nullptr, "Normal session has no bound gameplay context");
        // Loading queues the entry reward; the first gameplay frame presents it through this shared controller.
        GameplayHeldItemController::applyGrantedEventItemsToHeldInventory(runtime, *pEvents,
            application.m_gameDataLoader.getItemTable());
        if (runtime.heldInventoryItem().active)
        {
            require(world == "mm6" && runtime.heldInventoryItem().item.objectDescriptionId == 2125,
                "First normal-play event presented an unexpected held item");
            std::string placementError;
            require(GameplayHeldItemController::tryAutoPlaceHeldInventoryItemOnPartyMember(
                runtime.heldInventoryItem(), *runtime.party(), 0, placementError),
                "Cannot place the starting letter into the normal character's inventory: " + placementError);
        }
        GameplayHeldItemController::applyGrantedEventItemsToHeldInventory(runtime, *pEvents,
            application.m_gameDataLoader.getItemTable());
        const std::filesystem::path savePath = output / (reload ? "normal-reload.oysav" : "normal-start.oysav");
        require(application.quickSaveToPath(savePath, "正常开局", {}), "Actual application failed to save new party");
        require(application.m_pOutdoorPartyRuntime != nullptr, "Normal creation has no movement runtime");
        const Party *pParty = &application.m_pOutdoorPartyRuntime->party();
        require(pParty->members().size() == characters.size(), "Normal session lost created party members");
        for (size_t index = 0; index < characters.size(); ++index)
        {
            const Character &current = pParty->members()[index];
            const Character &created = characters[index];
            require(current.name == created.name && current.className == created.className
                && current.characterDataId == created.characterDataId && current.level == 1
                && current.experience == 0 && current.skillPoints == 0 && current.inventory.size() >= 5,
                "Normal session changed a party member or omitted starting items");
            for (const InventoryItem &item : current.inventory)
            {
                require(application.m_gameDataLoader.getItemTable().get(item.objectDescriptionId),
                    "Party member has an undefined starter item");
            }
        }
        const Character &member = pParty->members().front();
        require(member.name == characters.front().name && member.className == characters.front().className
            && member.level == 1 && member.experience == 0 && member.skillPoints == 0,
            "Actual new session changed the created character name or gave debug progression");
        require(pParty->gold() == 200 && pParty->food() == 5, "Normal session has unexpected gold or food");
        if (world == "mm6")
        {
            require(pParty->hasQuestBit(1104) && pParty->hasQuestBit(1105) && pParty->inventoryItemCount(2125) == 1,
                "MM6 entry did not retain exactly one starting letter and its quest flags");
        }
        else if (world == "mm7")
        {
            // Original 7out01 event 100 initializes the six Emerald Island scavenger-hunt tasks.
            for (uint32_t id = 513; id <= 518; ++id)
            {
                require(pParty->hasQuestBit(id), "MM7 entry/reload lost scavenger-hunt quest " + std::to_string(id));
            }
            require(!pParty->hasQuestBit(519) && !pParty->hasQuestBit(1104) && !pParty->hasQuestBit(1105)
                && pParty->inventoryItemCount(2125) == 0, "MM7 startup completed tasks or inherited MM6 rewards");
        }
        else
        {
            // Original out01 load event 3 initializes these four bits once.
            for (uint32_t id : {226, 185, 401, 407})
            {
                require(pParty->hasQuestBit(id), "MM8 startup lost original initialization bit " + std::to_string(id));
            }
            require(!pParty->hasQuestBit(232) && !pParty->hasQuestBit(513) && !pParty->hasQuestBit(1104)
                && !pParty->hasQuestBit(1105) && pParty->inventoryItemCount(2125) == 0,
                "MM8 startup triggered the later daily event or inherited another continent's opening quests");
        }
        require(member.inventory.size() >= 5, "Normal creation did not grant starter inventory");
        if (member.className == "Sorcerer")
        {
            require(member.knowsSpell(1) && !member.knowsSpell(2),
                "Normal Sorcerer must start with Torch Light learned and Fire Bolt still in its book");
            require(pParty->inventoryItemCount(400) == 0 && pParty->inventoryItemCount(401) == 1,
                "Normal Sorcerer must receive one Fire Bolt book and no redundant Torch Light book");
            require(member.spellPoints > 0 && member.spellPoints == member.maxSpellPoints,
                "Normal Sorcerer did not start with full, nonzero spell points");
            std::cout << "Starting magic: learned=1, book=401, mana=" << member.spellPoints << '\n';
        }
        for (const InventoryItem &item : member.inventory)
        {
            require(application.m_gameDataLoader.getItemTable().get(item.objectDescriptionId) != nullptr,
                "Normal starting item is absent from imported item definitions");
        }
        const OutdoorMoveState &position = application.m_pOutdoorPartyRuntime->movementState();
        require(std::abs(position.x - expectedX) < 1.0f && std::abs(position.y - expectedY) < 1.0f,
            "Normal new game or reload did not retain the imported town entrance coordinates");
        std::cout << "Normal application " << (reload ? "reload" : "start") << " passed: world=" << world
            << ", map=" << expectedMap << ", name=" << member.name
            << ", gold=" << pParty->gold() << ", food=" << pParty->food()
            << ", starter_inventory=" << member.inventory.size() << ", letter=" << pParty->inventoryItemCount(2125)
            << ", position=("
            << position.x << ',' << position.y << ',' << position.footZ << "), save=" << savePath << '\n';
        application.shutdownApplication();
    }
};
}

int main(int argc, char **argv)
{
    using namespace OpenYAMM;
    try
    {
        std::string world = "mm6";
        std::string className = "Knight";
        bool extraSchools = false;
        bool fourMembers = false;
        std::vector<std::string> arguments;
        for (int index = 1; index < argc; ++index)
        {
            const std::string argument = argv[index];
            if (argument.starts_with("--world="))
            {
                world = argument.substr(8);
            }
            else if (argument == "--extra-schools")
            {
                extraSchools = true;
            }
            else if (argument == "--four-members")
            {
                fourMembers = true;
            }
            else if (argument.starts_with("--class="))
            {
                className = argument.substr(8);
            }
            else
            {
                arguments.push_back(argument);
            }
        }
        Game::NewGameScreenTestAccess::require(!arguments.empty() && arguments.size() <= 3
            && (world == "mm6" || world == "mm7" || world == "mm8")
            && (className == "Knight" || className == "Sorcerer")
            && (!extraSchools || className == "Sorcerer") && (!fourMembers || (className == "Knight" && !extraSchools)),
            "Usage: openyamm_creation_check ROOT [NEW_OUTPUT [SAVE_TO_RELOAD]] "
            "[--world=mm6|mm7|mm8] [--class=Knight|Sorcerer] [--extra-schools] [--four-members]");
        Game::NewGameScreenTestAccess::require(SDL_Init(SDL_INIT_EVENTS), "Cannot initialize SDL events");
        const std::filesystem::path root = std::filesystem::absolute(arguments[0]);
        Engine::AssetFileSystem files;
        Game::NewGameScreenTestAccess::require(files.initialize(root, root, Engine::AssetScaleTier::X1, "mm6"),
            "Cannot mount imported runtime packages");
        Game::GameDataLoader loader;
        Game::NewGameScreenTestAccess::require(loader.loadCommonForGameplay(files), "Cannot load imported common data");
        Game::GameDataRepository data;
        data.bind(loader);
        const std::vector<Game::Character> mm6Characters =
            Game::NewGameScreenTestAccess::checkParty(files, data, "enroth", 3);
        const std::vector<Game::Character> mm7Characters =
            Game::NewGameScreenTestAccess::checkParty(files, data, "antagarich", 2);
        const std::vector<Game::Character> mm8Characters =
            Game::NewGameScreenTestAccess::checkParty(files, data, "jadame", 1);
        std::cout << "Chinese character editing and normal creation passed for all three continent selections.\n";
        const std::vector<Game::Character> selectedCharacters = fourMembers
            ? Game::NewGameScreenTestAccess::checkFourMemberParty(files, data, world) : className == "Knight"
            ? (world == "mm6" ? mm6Characters : world == "mm7" ? mm7Characters : mm8Characters)
            : Game::NewGameScreenTestAccess::checkParty(files, data,
                world == "mm6" ? "enroth" : world == "mm7" ? "antagarich" : "jadame",
                world == "mm6" ? 3 : world == "mm7" ? 2 : 1, className, extraSchools);
        if (arguments.size() >= 2)
        {
            const std::filesystem::path output = std::filesystem::absolute(arguments[1]);
            const std::optional<std::filesystem::path> reload = arguments.size() == 3
                ? std::optional(std::filesystem::absolute(arguments[2])) : std::nullopt;
            Game::NewGameSessionTestAccess::checkNormalSession(files, selectedCharacters,
                root, output, reload, world);
        }
        SDL_Quit();
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        SDL_Quit();
        return 1;
    }
}
