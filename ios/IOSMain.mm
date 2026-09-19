#import <Foundation/Foundation.h>
#import "Launcher.h"
#import "ResourceImport.h"

#include "game/app/OpenYammMain.h"
#include "game/app/GameSettings.h"
#include "tools/AppleMetalSmoke.h"
#include "tools/AppleMenuPreview.h"
#include "tools/AppleOutdoorPreview.h"
#include "tools/AppleStoryCheck.h"
#include "tools/mmerge_import/StoryResources.h"
#include "tools/mmerge_import/MenuResources.h"
#include "tools/mmerge_import/OutdoorResources.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string_view>

namespace
{
bool prepareSettings(const std::filesystem::path &assets, std::string &error)
{
    OpenYAMM::Game::GameSettings settings = OpenYAMM::Game::GameSettings::createDefault();
    if (std::filesystem::exists("settings.ini"))
    {
        const std::optional<OpenYAMM::Game::GameSettings> loadedSettings =
            OpenYAMM::Game::loadGameSettings("settings.ini", error);
        if (!loadedSettings)
        {
            return false;
        }
        settings = *loadedSettings;
    }
    else
    {
        settings.startInMainMenu = true;
        settings.windowMode = OpenYAMM::Game::WindowMode::Windowed;
        settings.controlScheme = OpenYAMM::Game::ControlScheme::Modern;
        settings.startWorldId = "mm8";
        settings.verticalSync = true;
    }
    NSURL *conversionURL = [NSURL fileURLWithPath:
        [NSString stringWithUTF8String:(assets / "conversion.json").c_str()]];
    NSData *conversionData = [NSData dataWithContentsOfURL:conversionURL];
    NSDictionary *conversion = conversionData != nil
        ? [NSJSONSerialization JSONObjectWithData:conversionData options:0 error:nil] : nil;
    if ([conversion isKindOfClass:NSDictionary.class]
        && [conversion[@"stage"] isEqual:@"native-runtime-assembly"]
        && settings.assetRoot != assets.string())
    {
        // A newly selected source import starts a normal game in the currently converted world.
        settings.startInMainMenu = true;
        settings.startWorldId = "mm6";
        settings.startMapFile.clear();
        settings.preseedParty = false;
        settings.partySeedRosterId = 0;
        settings.immortal = false;
        settings.unlimitedMana = false;
        settings.overrideStartPosition = false;
        settings.startFlying = false;
    }
    // iOS can change the container URL after an app update or restore.
    settings.assetRoot = assets.string();
    return OpenYAMM::Game::saveGameSettings("settings.ini", settings, error);
}
}

int main(int argc, char **argv)
{
    @autoreleasepool
    {
        NSFileManager *fileManager = NSFileManager.defaultManager;
        NSError *error = nil;
        NSURL *support = [fileManager URLForDirectory:NSApplicationSupportDirectory
            inDomain:NSUserDomainMask appropriateForURL:nil create:YES error:&error];
        NSURL *documents = [fileManager URLForDirectory:NSDocumentDirectory
            inDomain:NSUserDomainMask appropriateForURL:nil create:YES error:&error];
        if (support == nil || documents == nil)
        {
            std::cerr << "Unable to open application storage\n";
            return 1;
        }
        std::error_code fileError;
        std::filesystem::current_path(support.path.UTF8String, fileError);
        if (fileError)
        {
            return 1;
        }
        NSString *message = @"";
        bool initialMenu = false;
        bool initialOutdoor = false;
        bool automatedOutdoor = false;
        bool initialStory = false;
        // Development tools can stage owned data in Documents and exercise the
        // same importer as the folder picker, without an alternate import path.
        for (int i = 1; i < argc; ++i)
        {
            const std::string_view argument = argv[i];
            if (argument == "--menu-preview")
            {
                initialMenu = true;
            }
            else if (argument == "--story-check")
            {
                initialStory = true;
            }
            else if (argument == "--outdoor-preview" || argument == "--outdoor-check")
            {
                initialOutdoor = true;
                automatedOutdoor = argument == "--outdoor-check";
            }
            else if (argument == "--import-source" && i + 1 < argc)
            {
                const std::filesystem::path relative = argv[++i];
                if (relative.empty() || relative.is_absolute()
                    || std::find(relative.begin(), relative.end(), "..") != relative.end())
                {
                    std::cerr << "Import test path must be a subdirectory of Documents\n";
                    return 2;
                }
                NSURL *source = [documents URLByAppendingPathComponent:
                    [NSString stringWithUTF8String:relative.c_str()]];
                NSDictionary *record = [OYResourceImport importDirectory:source intoSupport:support
                    progress:nil cancellation:nil error:&error];
                if (record == nil)
                {
                    std::cerr << "Import failed: " << error.localizedDescription.UTF8String << std::endl;
                    return 3;
                }
                std::cout << "source_import files=" << [record[@"files"] unsignedLongLongValue]
                    << " bytes=" << [record[@"bytes"] unsignedLongLongValue] << std::endl;
                message = @"资源导入完成。";
            }
        }
        bool initialSmoke = argc > 1 && std::string_view(argv[1]) == "--metal-smoke";
        for (;;)
        {
            if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
            {
                std::cerr << SDL_GetError() << '\n';
                return 1;
            }
            std::filesystem::path assets;
            int action = initialSmoke ? 2 : 0;
            if (initialMenu || initialOutdoor || initialStory)
            {
                NSDictionary *record = [OYResourceImport activeImportInSupport:support];
                if ([record[@"kind"] isEqual:@"source"])
                {
                    assets = [OYResourceImport contentURLForImport:record support:support].path.UTF8String;
                    action = initialStory ? 5 : initialOutdoor ? 4 : 3;
                }
                else
                {
                    message = @"请先导入原始游戏资源，再运行预览。";
                }
            }
            if (action == 0)
            {
                action = showIosLauncher(support, message, assets);
            }
            initialSmoke = false;
            initialMenu = false;
            initialOutdoor = false;
            initialStory = false;
            SDL_Quit();
            if (action == 0)
            {
                return 0;
            }
            if (action == 5)
            {
                try
                {
                    const std::filesystem::path validation = std::filesystem::path(documents.path.UTF8String)
                        / (std::string("StoryValidation-") + NSUUID.UUID.UUIDString.UTF8String);
                    OpenYAMM::Import::convertStoryResources(assets, validation / "assets");
                    const int result = runAppleStoryCheck(validation / "assets", validation / "checks");
                    std::cout << "story_validation_path=" << validation.filename().string() << std::endl;
                    message = result == 0 ? @"中文 NPC、任务数据及存档序列化检查通过。完整游戏流程尚未接通。"
                        : @"剧情数据检查未通过，请查看日志。";
                }
                catch (const std::exception &failure)
                {
                    std::cerr << "Story conversion failed: " << failure.what() << std::endl;
                    message = [NSString stringWithFormat:@"剧情数据转换失败：%s", failure.what()];
                }
                continue;
            }
            if (action == 2)
            {
                const std::filesystem::path output =
                    std::filesystem::path(documents.path.UTF8String) / "metal-smoke.ppm";
                const int result = runAppleMetalSmoke(output);
                message = result == 0 ? @"渲染检查通过：已成功绘制并读取画面。"
                    : [NSString stringWithFormat:@"渲染检查未通过（错误码 %d）。", result];
                continue;
            }
            if (action == 3)
            {
                try
                {
                    const std::filesystem::path preview =
                        std::filesystem::path(support.path.UTF8String) / "MenuPreview";
                    // Only discard this generated preview, never the imported originals.
                    std::filesystem::remove_all(preview);
                    OpenYAMM::Import::convertMenuResources(assets, preview);
                    const int result = runAppleMenuPreview(preview,
                        std::filesystem::path(documents.path.UTF8String) / "menu-preview.ppm");
                    message = result == 0 ? @"原始主菜单预览完成。地图、事件和游戏内汉化仍需转换。"
                        : @"主菜单预览未完成，请检查资源和渲染日志。";
                }
                catch (const std::exception &failure)
                {
                    std::cerr << "Menu conversion failed: " << failure.what() << std::endl;
                    message = [NSString stringWithFormat:@"主菜单转换失败：%s", failure.what()];
                }
                continue;
            }
            if (action == 4)
            {
                try
                {
                    const std::filesystem::path preview =
                        std::filesystem::path(support.path.UTF8String) / "OutdoorPreview";
                    std::filesystem::remove_all(preview);
                    OpenYAMM::Import::convertOutdoorResources(assets, preview, true);
                    const int result = runAppleOutdoorPreview(preview, documents.path.UTF8String, automatedOutdoor);
                    message = result == 0 ? @"新索匹格镇预览完成。游戏测试需导入转换资源后点击“开始游戏”。"
                        : @"场景检查未完成，请查看渲染日志。";
                }
                catch (const std::exception &failure)
                {
                    std::cerr << "Outdoor preview failed: " << failure.what() << std::endl;
                    message = [NSString stringWithFormat:@"地图预览失败：%s", failure.what()];
                }
                automatedOutdoor = false;
                continue;
            }
            std::string settingsError;
            if (!prepareSettings(assets, settingsError))
            {
                message = [NSString stringWithFormat:@"无法保存游戏设置：%s", settingsError.c_str()];
                continue;
            }
            try
            {
                const int result = OpenYAMM::Game::runApplication(1, argv);
                message = result == 0 ? @"已返回启动页。"
                    : [NSString stringWithFormat:@"游戏启动失败（错误码 %d），请检查转换资源。", result];
            }
            catch (const std::exception &failure)
            {
                message = [NSString stringWithFormat:@"游戏启动失败：%s", failure.what()];
            }
        }
    }
}
