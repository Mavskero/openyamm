-- Source-verified Merge Hive ending. This supplements the exported map EVT;
-- it does not turn the exit, a quest bit, or a defeated queen alone into victory.
local originalExit = assert(evt.map[60], "MM6 Hive ending requires exported exit event60")
local exitTitle = evt.meta.map.title[60]
local exitHint = evt.meta.map.hint[60]

local function failedEnding()
    evt.ShowMovie("mm6end2", true)
    evt.PlaySound(130)
    evt.ReturnToMainMenu()
end

local function successfulEnding()
    SetQBit(QBit(784))
    evt.SetGlobalVar("MMerge.CrossContinents.ContinentFinished.3", 1)
    ClearQBit(QBit(1222))
    evt.ForPlayer(Players.All)
    RemoveItem(2164)
    SetAward(Award(78))
    evt.ShowMovie("mm6end1", true)
end

RegisterMapOnLoadEvent(65483, nil, function()
    evt.SetDeathMovieSuppressed(evt.GetMapVar("ReactorKilled", 0) ~= 0
        and evt.GetMapVar("QueenKilled", 0) == 0)
end)

RegisterMonsterDamageHook(65480, nil, function(context)
    if (context.monsterId == 647 or context.monsterId == 648) and context.damageType == Damage.Physical then
        evt.SetHookDamage(math.random(2, 40))
    end
end)

-- The original reaction creates sixteen single monsters, using the map's two
-- monster families. Retain source order and coordinates, including the repeated X.
local reinforcements = {
    {2, 4352, 20096}, {2, 6016, 21504}, {2, 2816, 22016},
    {1, 4352, 24704}, {1, 2944, 23552}, {1, 6144, 23424},
    {2, 2688, 19840}, {2, 1920, 21760}, {2, 6144, 19840},
    {2, 7168, 21760}, {1, 2584, 25728}, {1, 5248, 25728},
    {1, 1792, 23168}, {1, 2688, 25216}, {1, 7296, 23040},
    {1, 6144, 25088},
}

local restoredConditions = {
    Cursed, Weak, Asleep, Afraid, Drunk, Insane,
    PoisonedGreen, DiseasedGreen, PoisonedYellow, DiseasedYellow,
    PoisonedRed, DiseasedRed, Paralyzed, Unconscious, Stoned,
}

RegisterMonsterKilledHook(65481, nil, function(context)
    if context.monsterId == 646 then
        if evt.GetMapVar("ReactorKilled", 0) ~= 0 then
            evt.SetMapVar("QueenKilled", 1)
            evt.SetDeathMovieSuppressed(false)
            SetQBit(QBit(1226))
        end
        return
    end
    if context.monsterId ~= 647 and context.monsterId ~= 648 then
        return
    end

    evt.ForPlayer(Players.All)
    if not HasItem(2164) then
        failedEnding()
        return
    end

    evt.SetMapVar("ReactorKilled", 1)
    evt.SetDeathMovieSuppressed(true)
    for _, spawn in ipairs(reinforcements) do
        evt.SummonMonsters(spawn[1], 3, 1, spawn[2], spawn[3], -2256, 0, 0)
    end
    for _, door in ipairs({28, 51, 52}) do
        evt.SetDoorState(door, DoorAction.Open)
    end
    for _, door in ipairs({30, 53}) do
        evt.SetDoorState(door, DoorAction.Close)
    end

    local _, _, footZ = evt.GetPartyPosition()
    evt.MoveToMap(3328 + math.random(-120, 120), 25920 + math.random(-120, 120), footZ, 512)
    for _, condition in ipairs(restoredConditions) do
        SubtractValue(condition, 0)
    end
    -- Source leaves Dead, Eradicated and Zombie intact; normal rest semantics
    -- also clear temporary bonuses/buffs and retain the Zombie HP/SP rules.
    evt.RestAndHealParty()
end)

RegisterMapOnLeaveEvent(65482, nil, function()
    if evt.GetMapVar("ReactorKilled", 0) == 0 then
        return
    end
    if evt.GetMapVar("QueenKilled", 0) ~= 0 then
        successfulEnding()
    else
        failedEnding()
    end
end)

ReplaceMapEvent(60, exitTitle, function()
    if not IsQBitSet(QBit(1226)) then
        -- Preserve the imported STR's exact localized locked-door message.
        originalExit()
        return
    end
    evt.MoveToMap(0, 0, 0, 0, 0, 0, 0, 0, "oute3.odm")
end, exitHint)
SetMapContextAction(60, {kind = "leave_dungeon", source = "mmmerge_hive", targetMap = "oute3.odm"})
