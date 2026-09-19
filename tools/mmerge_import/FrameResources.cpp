#include "FrameResources.h"
#include "LodResources.h"
#include "TextResources.h"
#include "engine/TextTable.h"
#include "game/tables/IconFrameTable.h"
#include "game/tables/PortraitFrameTable.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace OpenYAMM::Import
{
namespace
{
using Row = std::vector<std::string>;
using Rows = std::vector<Row>;

void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

uint32_t number(const std::string &text)
{
    uint32_t value = 0;
    const std::from_chars_result result = std::from_chars(text.data(), text.data() + text.size(), value);
    require(result.ec == std::errc() && result.ptr == text.data() + text.size() && value <= 65535,
        "Invalid frame table integer: " + text);
    return value;
}

std::string lower(std::string text)
{
    for (char &letter : text)
    {
        if (letter >= 'A' && letter <= 'Z')
        {
            letter += 'a' - 'A';
        }
    }
    return text;
}

bool overridesOriginalIcons(const std::filesystem::path &base, const std::filesystem::path &overlay)
{
    if (lower(base.filename().string()) != "icons.lod")
    {
        return false;
    }
    constexpr std::array knownLayers = {
        "mm6.icons.lod", "mm7.icons.lod", "mm8.icons.lod", "mmmerge.icons.lod",
        "mm6.d.lod", "mm7.d.lod", "mm8.d.lod", "mmmerge.d.lod", "00 patch.icons.lod",
        "z10 loczhcn.icons.lod", "icons.ui.lod", "patch.icons.ui.lod"
    };
    const std::string name = lower(overlay.filename().string());
    return std::find(knownLayers.begin(), knownLayers.end(), name) != knownLayers.end();
}

std::string frameName(const std::string &name)
{
    require(name.size() <= 12 && (name.empty() || (name != "." && name != ".."
        && name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-")
            == std::string::npos)), "Unsafe frame name");
    return lower(name);
}

std::filesystem::path checkedPath(const std::filesystem::path &root, const std::filesystem::path &name)
{
    const std::filesystem::path path = std::filesystem::canonical(root / name);
    const std::filesystem::path relative = path.lexically_relative(root);
    require(!relative.empty() && !relative.is_absolute() && *relative.begin() != "..", "Frame source escapes root");
    return path;
}

Rows parse(const std::string &text)
{
    const std::optional<Engine::TextTable> table = Engine::TextTable::parseTabSeparated(text);
    require(table.has_value(), "Invalid frame table quoting");
    Rows rows;
    for (size_t index = 0; index < table->getRowCount(); ++index)
    {
        const Row &row = table->getRow(index);
        if (index + 1 == table->getRowCount() && row == Row{""})
        {
            continue;
        }
        rows.push_back(row);
    }
    return rows;
}

Rows read(const std::filesystem::path &root, const std::string &name)
{
    const std::filesystem::path path = checkedPath(root, "Data/Tables/" + name);
    const uintmax_t size = std::filesystem::file_size(path);
    require(size > 0 && size <= 4 * 1024 * 1024, "Invalid frame source size");
    std::ifstream stream(path, std::ios::binary);
    std::vector<uint8_t> bytes(size);
    require(bool(stream.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(size))),
        "Cannot read frame source");
    size_t glyphs = 0;
    return parse(decodeLegacyText(bytes, false, glyphs));
}

struct Frame
{
    uint32_t id = 0;
    uint32_t texture = 0;
    uint32_t time = 0;
    uint32_t total = 0;
    uint16_t flags = 0;
    std::string name;
    std::string image;
};

struct FrameData
{
    std::vector<Frame> frames;
    std::vector<size_t> starts;
    Rows output;
};

FrameData convert(const Rows &rows, bool portrait)
{
    require(rows.size() > 1 && rows.size() <= 65536
        && rows[0] == (portrait ? Row{"GroupId", "FrameIndex", "Time", "GroupStart"}
            : Row{"#", "GroupName", "IconName", "Time", "GroupStart"}), "Unsupported frame header/count");
    FrameData result;
    std::set<uint32_t> portraitIds;
    std::set<std::string> iconNames;
    for (size_t index = 1; index < rows.size(); ++index)
    {
        const Row &row = rows[index];
        require(row.size() == (portrait ? 4 : 5) && (row.back() == "x" || row.back() == "-"),
            "Invalid frame shape/start marker");
        const bool start = row.back() == "x";
        Frame frame;
        frame.id = number(row[0]);
        frame.time = number(row[portrait ? 2 : 3]);
        frame.flags = start ? 4 : 0;
        require(index != 1 || start, "First frame must start a group");
        if (portrait)
        {
            frame.texture = number(row[1]);
            require(frame.texture > 0 && frame.time > 0, "Invalid portrait frame texture/time");
            require(start ? portraitIds.insert(frame.id).second : frame.id == 0,
                "Duplicate portrait group ID or nonzero continuation ID");
        }
        else
        {
            require(frame.id == index - 1, "Icon frame row ID/order changed");
            frame.name = frameName(row[1]);
            frame.image = frameName(row[2]);
            // The authored first row is an unnamed zero-time placeholder. Blank
            // texture cells in other rows remain explicit unresolved references.
            require(index == 1 ? frame.name.empty() && frame.image == "pending" && frame.time == 0
                : frame.time > 0 && (!start || (!frame.name.empty() && iconNames.insert(frame.name).second)),
                "Invalid icon group name/time or duplicate group");
        }
        if (start)
        {
            result.starts.push_back(result.frames.size());
        }
        result.frames.push_back(std::move(frame));
    }
    for (size_t group = 0; group < result.starts.size(); ++group)
    {
        const size_t start = result.starts[group];
        const size_t end = group + 1 < result.starts.size() ? result.starts[group + 1] : result.frames.size();
        uint32_t total = 0;
        for (size_t index = start; index < end; ++index)
        {
            Frame &frame = result.frames[index];
            frame.flags |= index + 1 < end ? 1 : 0;
            total += frame.time;
        }
        require(total <= 65535, "Frame group duration exceeds source storage");
        result.frames[start].total = total;
    }
    for (const Frame &frame : result.frames)
    {
        result.output.push_back({portrait ? std::to_string(frame.id) : frame.name,
            portrait ? std::to_string(frame.texture) : frame.image, std::to_string(frame.time),
            std::to_string(frame.total), std::to_string(frame.flags)});
    }
    return result;
}

size_t validate(const FrameData &data, bool portrait)
{
    const Rows rows = parse(serializeTextRows(data.output));
    require(rows == data.output, "Frame serialization changed fields");
    Game::PortraitFrameTable portraits;
    Game::IconFrameTable icons;
    require(portrait ? portraits.loadRows(rows) : icons.loadRows(rows), "Frame typed loading failed");
    size_t checks = 0;
    for (size_t group = 0; group < data.starts.size(); ++group)
    {
        const size_t start = data.starts[group];
        const size_t end = group + 1 < data.starts.size() ? data.starts[group + 1] : data.frames.size();
        const Frame &first = data.frames[start];
        const Game::PortraitId portraitId = static_cast<Game::PortraitId>(first.id);
        if ((portrait && first.id == 0) || (!portrait && first.name.empty()))
        {
            require(portrait ? !portraits.findAnimationId(portraitId) : !icons.findAnimationIdByName(first.name),
                "Placeholder became a named animation");
            continue;
        }
        const std::optional<size_t> id = portrait ? portraits.findAnimationId(portraitId)
            : icons.findAnimationIdByName(first.name);
        require(id == start, "Frame group index changed");
        const int32_t length = portrait ? portraits.animationLengthTicks(portraitId)
            : icons.animationLengthTicks(start);
        require(length == first.total * 8, "Frame timing units changed");
        const auto check = [&](uint32_t ticks, size_t expectedIndex)
        {
            const Frame &expected = data.frames[expectedIndex];
            if (portrait)
            {
                const Game::PortraitFrameEntry *pFrame = portraits.getFrame(portraitId, ticks);
                require(pFrame && static_cast<uint16_t>(pFrame->portraitId) == expected.id
                    && pFrame->textureIndex == expected.texture && pFrame->flags == expected.flags
                    && pFrame->frameLengthTicks == expected.time * 8
                    && pFrame->animationLengthTicks == expected.total * 8, "Portrait frame selection changed");
            }
            else
            {
                const Game::IconFrameEntry *pFrame = icons.getFrame(start, ticks);
                require(pFrame && pFrame->animationName == expected.name && pFrame->textureName == expected.image
                    && pFrame->flags == expected.flags && pFrame->frameLengthTicks == expected.time * 8
                    && pFrame->animationLengthTicks == expected.total * 8, "Icon frame selection changed");
            }
            ++checks;
        };
        uint32_t ticks = 0;
        for (size_t index = start; index < end; ++index)
        {
            check(ticks, index);
            check(ticks + data.frames[index].time * 8 - 1, index);
            ticks += data.frames[index].time * 8;
        }
        check(ticks, start);
        check(ticks * 2, start);
        const uint32_t finalTicks = UINT32_MAX % ticks;
        uint32_t accumulated = 0;
        for (size_t index = start; index < end; ++index)
        {
            accumulated += data.frames[index].time * 8;
            if (finalTicks < accumulated)
            {
                check(UINT32_MAX, index);
                break;
            }
        }
    }
    return checks;
}

void writeText(const std::filesystem::path &path, const std::string &text)
{
    writeConvertedResource(path, std::vector<uint8_t>(text.begin(), text.end()));
}
}

void convertFrameResources(const std::filesystem::path &source, const std::filesystem::path &destination)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    const std::filesystem::path output = std::filesystem::weakly_canonical(destination);
    const std::filesystem::path relative = output.lexically_relative(root);
    require(!relative.empty() && !relative.is_absolute() && *relative.begin() == "..",
        "Frame output must be outside source");
    require(!std::filesystem::exists(output), "Frame output already exists");
    const FrameData portraits = convert(read(root, "PFT.txt"), true);
    const FrameData icons = convert(read(root, "IFT.txt"), false);
    const size_t timingChecks = validate(portraits, true) + validate(icons, false);
    std::set<std::string> names;
    std::set<std::string> portraitNames;
    std::set<std::string> portraitPrefixes;
    if (std::filesystem::exists(root / "Data/Tables/Character portraits.txt"))
    {
        const Rows characters = read(root, "Character portraits.txt");
        require(!characters.empty() && characters[0].size() == 27 && characters[0][0] == "#"
            && characters[0][21] == "Face pics prefix", "Unsupported portrait image binding schema");
        for (size_t index = 1; index < characters.size(); ++index)
        {
            const Row &row = characters[index];
            require(row.size() == 27 && number(row[0]) == index - 1, "Portrait image bindings changed source IDs");
            const std::string prefix = frameName(row[21]);
            if (prefix.empty() || prefix == "none")
            {
                continue;
            }
            require(prefix.size() <= 10, "Portrait prefix leaves no room for its two-digit frame index");
            portraitPrefixes.insert(prefix);
            for (const Frame &frame : portraits.frames)
            {
                require(frame.texture <= 99, "Portrait frame cannot use the source two-digit suffix");
                std::ostringstream name;
                name << prefix << std::setw(2) << std::setfill('0') << frame.texture;
                portraitNames.insert(name.str());
            }
        }
    }
    std::vector<size_t> blankTextures;
    for (size_t index = 0; index < icons.frames.size(); ++index)
    {
        const std::string &name = icons.frames[index].image;
        if (name.empty())
        {
            blankTextures.push_back(index);
        }
        else
        {
            names.insert(name);
        }
    }
    std::map<std::string, std::filesystem::path> owners;
    // Inspect only top-level icon/presentation archives. Sprite members can have
    // the same name but a different format and are not icon fallbacks.
    for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(root / "Data"))
    {
        const std::string name = lower(entry.path().filename().string());
        if (!(name == "icons.lod" || name.ends_with(".icons.lod") || name.ends_with(".d.lod")
            || name == "icons.ui.lod" || name.ends_with(".icons.ui.lod")))
        {
            continue;
        }
        const std::filesystem::path archive = checkedPath(root, entry.path());
        for (const std::string &member : listLodMembers(archive))
        {
            if (names.contains(member) || portraitNames.contains(member))
            {
                const auto [owner, inserted] = owners.emplace(member, archive);
                if (!inserted)
                {
                    if (overridesOriginalIcons(owner->second, archive))
                    {
                        owner->second = archive;
                    }
                    else
                    {
                        require(overridesOriginalIcons(archive, owner->second),
                            "Ambiguous frame image ownership requires an explicit override profile: " + member);
                    }
                }
            }
        }
    }
    require(std::filesystem::create_directory(output), "Cannot create fresh frame output");
    try
    {
        std::vector<ConvertedLodImage> images;
        std::map<std::pair<std::filesystem::path, LodImagePixelFormat>, std::vector<std::string>> byArchive;
        for (const auto &[name, archive] : owners)
        {
            // IFT frames share original item artwork; retain the same palette/index-zero identity.
            const LodImagePixelFormat format = names.contains(name)
                ? LodImagePixelFormat::Indexed8 : LodImagePixelFormat::Rgb24;
            byArchive[{archive, format}].push_back(name);
        }
        for (const auto &[key, selected] : byArchive)
        {
            const std::vector<ConvertedLodImage> converted =
                convertLodImages({key.first}, selected, output / "engine/icons", key.second);
            images.insert(images.end(), converted.begin(), converted.end());
        }
        std::ostringstream manifest;
        manifest << "{\"schema\":1,\"stage\":\"shared-animation-frames\",\"runtime_ready\":false,\"portrait_frames\":"
            << portraits.frames.size() << ",\"portrait_groups_including_zero\":" << portraits.starts.size()
            << ",\"icon_frames\":" << icons.frames.size()
            << ",\"icon_groups_including_placeholder\":" << icons.starts.size()
            << ",\"frame_selection_checks\":" << timingChecks << ",\"missing_icon_images\":[";
        bool firstMissing = true;
        for (const std::string &name : names)
        {
            if (!owners.contains(name))
            {
                manifest << (firstMissing ? "" : ",") << std::quoted(name);
                firstMissing = false;
            }
        }
        manifest << "],\"blank_texture_frame_ids\":[";
        for (size_t index = 0; index < blankTextures.size(); ++index)
        {
            manifest << (index == 0 ? "" : ",") << blankTextures[index];
        }
        manifest << "],\"portrait_prefixes\":" << portraitPrefixes.size() << ",\"portrait_image_references\":"
            << portraitNames.size() << ",\"missing_portrait_images\":[";
        bool firstPortrait = true;
        size_t portraitImages = 0;
        for (const std::string &name : portraitNames)
        {
            if (owners.contains(name))
            {
                ++portraitImages;
                continue;
            }
            manifest << (firstPortrait ? "" : ",") << std::quoted(name);
            firstPortrait = false;
        }
        manifest << "],\"portrait_images_imported\":" << portraitImages << ",\"missing_normal_portraits\":[";
        firstPortrait = true;
        for (const std::string &prefix : portraitPrefixes)
        {
            if (!owners.contains(prefix + "01"))
            {
                manifest << (firstPortrait ? "" : ",") << std::quoted(prefix + "01");
                firstPortrait = false;
            }
        }
        manifest << "],\"images\":[";
        for (size_t index = 0; index < images.size(); ++index)
        {
            const ConvertedLodImage &image = images[index];
            manifest << (index == 0 ? "" : ",") << "{\"name\":" << std::quoted(image.name)
                << ",\"archive\":" << std::quoted(image.archive) << ",\"width\":" << image.width
                << ",\"height\":" << image.height << "}";
        }
        manifest << "],\"typed_validation\":true,"
            << "\"original_wall_clock_timing_validated\":false,"
            << "\"character_portrait_art_ready\":false,\"fx_event_bindings_ready\":false,"
            << "\"full_gameplay_loop\":false}\n";
        writeText(output / "engine/data_tables/portrait_frame_data.txt", serializeTextRows(portraits.output));
        writeText(output / "engine/data_tables/icon_frame_data.txt", serializeTextRows(icons.output));
        writeText(output / "frame_conversion.json", manifest.str());
    }
    catch (...)
    {
        std::filesystem::remove_all(output);
        throw;
    }
}
}
