#pragma once

#include "game/audio/SoundIds.h"
#include "game/ui/MenuScreenBase.h"
#include "game/ui/UiLayoutManager.h"

#include <array>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace OpenYAMM::Game
{
class GameAudioSystem;

class MainMenuScreen : public MenuScreenBase
{
public:
    using Action = std::function<void()>;

    MainMenuScreen(
        const Engine::AssetFileSystem &assetFileSystem,
        GameAudioSystem *pGameAudioSystem,
        Action newGameAction,
        Action loadGameAction,
        Action quitAction);

    AppMode mode() const override;
    void prepareForFirstFrame();

private:
    void drawScreen(float deltaSeconds) override;
    bool ensureLayoutLoaded();
    std::optional<Rect> resolveLayoutRect(
        const std::string &layoutId,
        float fallbackWidth = 0.0f,
        float fallbackHeight = 0.0f) const;
    ButtonVisualSet resolveButtonVisuals(
        const std::string &layoutId,
        const ButtonVisualSet &fallbackVisuals) const;
    std::string resolveAssetName(const std::string &layoutId, const std::string &fallbackAssetName) const;
    void playUiClickSound(SoundId soundId) const;
    void openCredits();
    void drawCredits();
    void rebuildCreditsLayout(const Rect &bodyRect, float scale);
    bool creditsKeyPressed(SDL_Scancode scancode);

    GameAudioSystem *m_pGameAudioSystem = nullptr;
    Action m_newGameAction;
    Action m_loadGameAction;
    Action m_quitAction;
    UiLayoutManager m_layoutManager;
    bool m_layoutLoaded = false;
    bool m_creditsVisible = false;
    std::string m_creditsText;
    std::vector<std::string> m_creditsLines;
    size_t m_creditsPageIndex = 0;
    size_t m_creditsLinesPerPage = 1;
    float m_creditsLineHeight = 0.0f;
    int m_creditsLayoutWidth = 0;
    int m_creditsLayoutHeight = 0;
    std::array<bool, SDL_SCANCODE_COUNT> m_creditsKeysHeld = {};
};
}
