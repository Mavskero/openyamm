#include "engine/AppleText.h"
#include "game/ui/GameplayHudCommon.h"
#include "game/ui/CharacterStatLayout.h"
#include "game/ui/CharacterSkillRows.h"
#include "game/tables/CharacterInspectTable.h"
#include "game/party/Party.h"
#include "game/ui/Utf8Text.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>

int main(int argc, char **argv)
{
    using namespace OpenYAMM::Engine;
    try
    {
        const auto require = [](bool condition, const char *pMessage)
        {
            if (!condition)
            {
                throw std::runtime_error(pMessage);
            }
        };
        const std::string text = "简体中文与繁體漢字，魔法门 MM678。";
        OpenYAMM::Game::CharacterInspectTable statText;
        require(statText.loadStatRows({{"Might", "description", "力量"},
            {"SkillPoints", "description", "技能点数"}, {"Hit Points", "description", "生命值"}}),
            "Cannot load localized character labels");
        require(OpenYAMM::Game::characterStatDisplayLabel(&statText, "characterstatmightlabel") == "力量"
            && OpenYAMM::Game::characterStatDisplayLabel(&statText, "characterstathitpointslabel") == "生命值"
            && OpenYAMM::Game::characterStatDisplayLabel(&statText, "characterskillsskillpointstextlabel") == "技能点数:",
            "Character page bypassed imported display names");
        require(!OpenYAMM::Game::characterStatDisplayLabel(&statText, "characterstatmightvalue")
            && !OpenYAMM::Game::characterStatDisplayLabel(nullptr, "characterstatmightlabel")
            && statText.getStat("Might") != nullptr && statText.getStat("力量") == nullptr,
            "Display labels must not replace numeric cells or semantic lookup keys");
        require(statText.loadSkillRows({{"Sword", "description", "normal", "expert", "master", "grand", "长剑"}}),
            "Cannot load localized skill display name");
        OpenYAMM::Game::Character skillCharacter;
        skillCharacter.skillPoints = 4;
        skillCharacter.skills["Sword"] = {"Sword", 2, OpenYAMM::Game::SkillMastery::Expert};
        const auto localizedSkills = OpenYAMM::Game::buildCharacterSkillUiData(&skillCharacter, &statText);
        const auto readOnlySkills = OpenYAMM::Game::buildCharacterSkillUiData(&skillCharacter, &statText, false);
        const auto baseSkills = OpenYAMM::Game::buildCharacterSkillUiData(&skillCharacter, nullptr);
        require(localizedSkills.weaponRows.size() == 1 && localizedSkills.weaponRows[0].label == "长剑 Expert"
            && localizedSkills.weaponRows[0].canonicalName == "Sword" && localizedSkills.weaponRows[0].level == "2"
            && localizedSkills.weaponRows[0].interactive && !readOnlySkills.weaponRows[0].interactive
            && baseSkills.weaponRows[0].label == "Sword Expert" && skillCharacter.skillPoints == 4,
            "Localized skill rows changed identity, level, readonly behavior or character data");
        OpenYAMM::Game::GameplayHudFontData font;
        font.fontHeight = 24;
        for (const float scale : {0.5f, 1.0f, 2.0f, 3.5f})
        {
            OpenYAMM::Game::UiLayoutManager::LayoutElement row = {};
            row.fontName = "Lucida";
            row.textFitHeight = true;
            row.textAlignY = OpenYAMM::Game::UiLayoutManager::TextAlignY::Middle;
            OpenYAMM::Game::GameplayResolvedHudLayoutElement bounds = {};
            bounds.width = 210 * scale;
            bounds.height = 14 * scale;
            bounds.scale = scale;
            int layers = 0;
            OpenYAMM::Game::GameplayHudCommon::renderLayoutLabel(
                row, bounds, "长剑 Grand",
                [&font](const std::string &) { return &font; },
                [](const OpenYAMM::Game::GameplayHudFontData &, uint32_t)
                { return bgfx::TextureHandle{0}; },
                [&](const OpenYAMM::Game::GameplayHudFontData &drawFont, bgfx::TextureHandle,
                    const std::string &drawText, float x, float y, float drawScale)
                {
                    const std::optional<AppleTextImage> bitmap =
                        rasterizeAppleText(drawText, drawFont.fontHeight * drawScale, 0xffffffffu);
                    const float shadowOffset = std::max(1.0f, drawScale);
                    require(bitmap && y >= 0 && y + bitmap->height + shadowOffset <= bounds.height + 0.01f,
                        "Chinese skill glyph bitmap and shadow exceed their row height");
                    require(x >= 0 && x + bitmap->width + shadowOffset <= bounds.width + 0.01f,
                        "Chinese skill glyph bitmap and shadow exceed their row width");
                    ++layers;
                });
            require(layers == 2, "Fitted Chinese skill row must retain both text layers");
        }
        const float hudWidth = OpenYAMM::Game::GameplayHudCommon::measureHudTextWidth(font, text);
        require(hudWidth == measureAppleText(text, 24).value_or(0) && hudWidth > 100,
            "Gameplay HUD measured UTF-8 as bitmap bytes");
        const std::string hudClipped = OpenYAMM::Game::GameplayHudCommon::clampHudTextToWidth(font, text, 130);
        require(OpenYAMM::Game::utf8CharacterCount(hudClipped).has_value()
            && measureAppleText(hudClipped, 24).value_or(1000) <= 130, "HUD clamp broke UTF-8 or width");
        require(OpenYAMM::Game::GameplayHudCommon::wrapHudTextToWidth(font, text, 150)
            == wrapAppleText(text, 24, 150).value_or(std::vector<std::string>{}),
            "HUD wrapping bypassed Unicode shaping");
        // Capture the actual label renderer's output without requiring a GPU.
        // Bounds are pixels; bitmap and CJK measurements are unscaled font units.
        OpenYAMM::Game::GameplayHudFontData layoutFont = font;
        layoutFont.firstChar = 32;
        layoutFont.lastChar = 126;
        layoutFont.atlasCellWidth = 8;
        for (OpenYAMM::Game::GameplayHudFontGlyphMetricsData &glyph : layoutFont.glyphMetrics)
        {
            glyph.width = 8;
        }
        for (const std::string &label : {std::string("Throwing Weapons Grand"), std::string("11000"), text})
        {
            for (const float scale : {0.5f, 1.0f, 2.0f, 3.5f})
            {
                OpenYAMM::Game::UiLayoutManager::LayoutElement layout = {};
                layout.fontName = "Lucida";
                layout.textPadX = 2;
                OpenYAMM::Game::GameplayResolvedHudLayoutElement bounds = {};
                bounds.width = 100;
                bounds.height = 100;
                bounds.scale = scale;
                int layers = 0;
                OpenYAMM::Game::GameplayHudCommon::renderLayoutLabel(
                    layout, bounds, label,
                    [&layoutFont](const std::string &) { return &layoutFont; },
                    [](const OpenYAMM::Game::GameplayHudFontData &, uint32_t)
                    { return bgfx::TextureHandle{0}; },
                    [&](const OpenYAMM::Game::GameplayHudFontData &drawFont, bgfx::TextureHandle,
                        const std::string &drawText, float x, float, float drawScale)
                    {
                        const float right = x + OpenYAMM::Game::GameplayHudCommon::measureHudTextWidth(
                            drawFont, drawText) * drawScale;
                        require(right <= bounds.width + 0.01f, "Scaled HUD label exceeds its pixel bounds");
                        require(drawText == label, "Fitting a HUD label must preserve its complete value");
                        ++layers;
                    });
                require(layers == 2, "A label with enough width should render both layers");
                bounds.width = 1;
                OpenYAMM::Game::GameplayHudCommon::renderLayoutLabel(
                    layout, bounds, label,
                    [&layoutFont](const std::string &) { return &layoutFont; },
                    [](const OpenYAMM::Game::GameplayHudFontData &, uint32_t)
                    { return bgfx::TextureHandle{0}; },
                    [&](const OpenYAMM::Game::GameplayHudFontData &, bgfx::TextureHandle,
                        const std::string &, float, float, float) { ++layers; });
                require(layers == 2, "A fully clipped label must not fall back to unbounded text");
            }
        }
        const std::optional<std::vector<std::string>> lines = wrapAppleText(text, 24, 150);
        require(lines.has_value() && lines->size() > 2, "Chinese did not wrap");
        std::string joined;
        for (const std::string &line : *lines)
        {
            joined += line;
            require(measureAppleText(line, 24).value_or(10000) <= 151, "Wrapped line exceeds width");
        }
        require(joined == text, "Wrapping changed the original UTF-8 bytes");
        require(!measureAppleText(std::string("\xff"), 24), "Invalid UTF-8 was accepted");
        require(!wrapAppleText(text, 24, 0), "Invalid width was accepted");
        const std::string composed = "e\xcc\x81";
        const std::string emoji = "👨‍👩‍👧‍👦";
        for (const std::string &cluster : {composed, emoji, std::string("𠀀")})
        {
            const std::optional<std::vector<std::string>> narrow = wrapAppleText(cluster + cluster, 24, 1);
            require(narrow && narrow->size() == 2 && (*narrow)[0] == cluster && (*narrow)[1] == cluster,
                "Wrapping split a composed character");
            const std::optional<std::string> clipped = truncateAppleText(cluster + cluster, 24,
                *measureAppleText(cluster + "…", 24));
            require(clipped && (*clipped == cluster + "…" || *clipped == cluster + cluster),
                "Truncation split a composed character");
        }
        const std::optional<std::string> clipped = truncateAppleText(text, 24, 130);
        require(clipped && clipped->ends_with("…") && *measureAppleText(*clipped, 24) <= 130,
            "Chinese truncation failed");
        const std::optional<std::vector<std::string>> paragraphs = wrapAppleText("中文\n\n测试", 24, 500);
        require(paragraphs && paragraphs->size() == 3 && (*paragraphs)[1].empty(), "Empty paragraph lost");
        const std::optional<AppleTextImage> bitmap = rasterizeAppleText("魔法门　新索匹格镇　MM678", 36, 0xff40c0ff);
        require(bitmap && bitmap->width > 100, "Chinese rasterization failed");
        size_t ink = 0;
        for (size_t i = 3; i < bitmap->bgra.size(); i += 4)
        {
            ink += bitmap->bgra[i] > 0;
        }
        require(ink > 100, "Chinese text bitmap is blank");
        if (argc == 2)
        {
            std::ofstream output(argv[1], std::ios::binary);
            output << "P6\n" << bitmap->width << ' ' << bitmap->height << "\n255\n";
            for (size_t i = 0; i < bitmap->bgra.size(); i += 4)
            {
                const unsigned alpha = bitmap->bgra[i + 3];
                for (int channel : {2, 1, 0})
                {
                    output.put(char(bitmap->bgra[i + channel] * alpha / 255));
                }
            }
            output.close();
            require(output.good(), "Cannot write text proof image");
        }
        std::cout << "UTF-8 shaping, CJK wrapping, grapheme truncation, invalid input and rasterization passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
