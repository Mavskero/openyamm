#include "game/tables/CombatTextTable.h"
#include "game/gameplay/GameplayCombatController.h"

#include <doctest/doctest.h>

using OpenYAMM::Game::CombatTextTable;
using Message = CombatTextTable::Message;

namespace
{
std::vector<std::vector<std::string>> chineseRows()
{
    return {{"combat.melee_hit", "%s攻击%s，造成%lu点破坏"},
        {"combat.projectile_hit", "%s射击%s，造成%lu点伤害"},
        {"combat.kill", "%s造成%lu点伤害，杀死%s"},
        {"combat.stun", "%s打昏了%s"}, {"combat.paralyze", "%s麻痹了%s"}};
}
}

TEST_CASE("original Chinese combat templates retain argument order")
{
    CombatTextTable text;
    REQUIRE(text.loadFromRows(chineseRows()));
    CHECK(text.format(Message::MeleeHit, "骑士", "蝙蝠", 12) == "骑士攻击蝙蝠，造成12点破坏");
    CHECK(text.format(Message::ProjectileHit, "法师", "地精", 4) == "法师射击地精，造成4点伤害");
    CHECK(text.format(Message::Kill, "弓手", "蜘蛛", 17) == "弓手造成17点伤害，杀死蜘蛛");
    CHECK(text.format(Message::Stun, "牧师", "蛇", 2) == "牧师打昏了蛇");
    CHECK(text.format(Message::Paralyze, "法师", "龙", 3) == "法师麻痹了龙");
}

TEST_CASE("combat templates do not interpret names as format strings")
{
    CombatTextTable text;
    auto rows = chineseRows();
    rows[0][1] = "100%% %s attacks %s: %lu";
    REQUIRE(text.loadFromRows(rows));
    CHECK(text.format(Message::MeleeHit, "%n𠮷", "%lu", -8) == "100% %n𠮷 attacks %lu: 0");
    CHECK(text.format(Message::Kill, "A", "B", 2147483647) == "A造成2147483647点伤害，杀死B");
    CHECK(text.format(Message::Count, "A", "B", 1).empty());
}

TEST_CASE("original English combat templates retain their wording")
{
    CombatTextTable text;
    REQUIRE(text.loadFromRows({
        {"combat.melee_hit", "%s hits %s for %lu damage"},
        {"combat.projectile_hit", "%s shoots %s for %lu points"},
        {"combat.kill", "%s inflicts %lu points killing %s"},
        {"combat.stun", "%s stuns %s"}, {"combat.paralyze", "%s paralyzes %s"}}));
    CHECK(text.format(Message::MeleeHit, "Ariel", "Goblin", 12) == "Ariel hits Goblin for 12 damage");
    CHECK(text.format(Message::ProjectileHit, "Ariel", "Goblin", 4) == "Ariel shoots Goblin for 4 points");
    CHECK(text.format(Message::Kill, "Ariel", "Goblin", 48) == "Ariel inflicts 48 points killing Goblin");
    CHECK(text.format(Message::Stun, "Ariel", "Goblin", 0) == "Ariel stuns Goblin");
    CHECK(text.format(Message::Paralyze, "Ariel", "Goblin", 0) == "Ariel paralyzes Goblin");
}

TEST_CASE("invalid combat templates fail without retaining the old language")
{
    for (const std::string &pattern : {"%s hits %s for %n", "%s hits %s for %d", "%s %lu %s",
             "%s %s", "%s %s %lu %s", "%s %s %lu%", "%s %s %lu\n", "%10s %s %lu"})
    {
        CAPTURE(pattern);
        CombatTextTable text;
        REQUIRE(text.loadFromRows(chineseRows()));
        auto rows = chineseRows();
        rows[0][1] = pattern;
        CHECK_FALSE(text.loadFromRows(rows));
        CHECK(text.format(Message::Kill, "A", "B", 1).empty());
    }
}

TEST_CASE("combat catalog rejects missing duplicate unknown and oversized rows")
{
    CombatTextTable text;
    auto rows = chineseRows();
    rows.pop_back();
    CHECK_FALSE(text.loadFromRows(rows));
    rows = chineseRows();
    rows[4] = rows[0];
    CHECK_FALSE(text.loadFromRows(rows));
    rows = chineseRows();
    rows[0][0] = "unknown";
    CHECK_FALSE(text.loadFromRows(rows));
    rows = chineseRows();
    rows[0][1] = std::string(4097, 'a');
    CHECK_FALSE(text.loadFromRows(rows));
    rows = chineseRows();
    rows[0].push_back("extra");
    CHECK_FALSE(text.loadFromRows(rows));
}

TEST_CASE("actual attack presentation uses applied damage and status priority")
{
    using namespace OpenYAMM::Game;
    CombatTextTable text;
    REQUIRE(text.loadFromRows(chineseRows()));
    CharacterAttackResult attack{};
    attack.canAttack = true;
    attack.hit = true;
    attack.damage = 48;
    const auto format = [&](bool killed)
    {
        return GameplayCombatController::formatPartyAttackStatusText(text, "骑士", "蝙蝠", attack, killed, 12);
    };
    CHECK(format(false) == "骑士攻击蝙蝠，造成12点破坏");
    for (const CharacterAttackMode mode : {CharacterAttackMode::Bow, CharacterAttackMode::Wand,
             CharacterAttackMode::Blaster, CharacterAttackMode::DragonBreath})
    {
        attack.mode = mode;
        CHECK(format(false) == "骑士射击蝙蝠，造成12点伤害");
    }
    attack.mode = CharacterAttackMode::Melee;
    attack.stunTarget = true;
    CHECK(format(false) == "骑士打昏了蝙蝠");
    attack.paralyzeTarget = true;
    CHECK(format(false) == "骑士麻痹了蝙蝠");
    CHECK(format(true) == "骑士造成12点伤害，杀死蝙蝠");
    attack.resolvesOnImpact = true;
    CHECK(format(false).empty());
    attack.resolvesOnImpact = false;
    attack.hit = false;
    CHECK(format(false).empty());
    attack.hit = true;
    attack.canAttack = false;
    CHECK(format(false).empty());
}
