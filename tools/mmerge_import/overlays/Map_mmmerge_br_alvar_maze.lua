-- Source: SHA-gated BrAlvar.lua. Native App owns confirmation and committed maze entry effects.
-- This controlled program does not admit dynamic maps through ordinary Merge travel.
ReplaceMapEvent(53, nil, function()
    evt.RequestBreachMazeTransition()
end)

ReplaceMapEvent(54, nil, function()
    evt.RequestBreachMazeTransition()
end)

-- BrAlvar.lua 342-345: reuse the original house231 and its normal Tavern services.
ReplaceMapEvent(7, nil, function()
    evt.EnterHouse(231)
end)

-- BrAlvar.lua 278–284: the alarm depends on the Prince's actual residence, even after quest completion.
ReplaceMapEvent(60, nil, function()
    local key = "MMerge.BrAlvar.SwordsAggro"
    if evt.GetMapVar(key, 0) ~= 0 then return end
    if evt.IsBreachAlvarPrinceOutsideHouse705() then
        evt.SimpleMessage(evt.strings[2])
        evt.SetMonGroupBit(15, eventSupport.actorAttribute.Hostile, true)
        evt.SetMapVar(key, 1)
    end
end)

-- BrAlvar.lua 348-355: every search gives a Jump scroll before the Repair check;
-- source variable 114 applies medium disease even though its Set argument is 0.
ReplaceMapEvent(30, nil, function()
    evt.ForPlayer(eventSupport.partySelector.Current)
    evt.Add(InventoryItem(315), 315)
    if not evt.Cmp(eventSupport.varTag.RepairSkill, 1) then
        evt.Set(eventSupport.varTag.DiseasedYellow, 0)
        evt.StatusText(evt.NPCText(726))
    end
end, evt.importedNpcTopics[284])

-- BrAlvar.lua 369-381: the original sum key is intentional, including collisions.
-- Record the search before drawing both random values; retain it across map reentry.
ReplaceMapEvent(82, nil, function()
    local x, y, z = evt.GetPartyPosition()
    local key = "MMerge.BrAlvar.LibKeys." .. (math.ceil(x / 200) + math.ceil(y / 200) + math.ceil(z / 100))
    if evt.GetMapVar(key, 0) ~= 0 then return end
    evt.SetMapVar(key, 1)
    evt.ForPlayer(eventSupport.partySelector.Member0)
    evt.GiveItem(evt.RandomBetween(3, 6), evt.RandomBetween(16, 17), 0)
end)

-- BrAlvar.lua 384-391: these source chests only produce the active member's locked-door reaction.
-- FaceAnimation(DoorLocked) already selects the current member in the shared event runtime.
local function lockedChest()
    evt.FaceAnimation(eventSupport.faceAnimation.DoorLocked)
end
for eventId = 301, 315 do
    ReplaceMapEvent(eventId, nil, lockedChest)
end

-- BrAlvar.lua 358–366. The original map binds 16 to its existing bridge facets;
-- 15 remains a source handler without adding a decoration or changing its event binding.
local function randomJump()
    local angle = math.abs(evt.GetPartyDirection() - 2048)
    -- Source Jump fields are Direction, ZAngle and Speed (2048 angle units per turn).
    if not evt.Jump(angle, evt.RandomBetween(128, 256), 1800) then
        error("BrAlvar bridge jump failed")
    end
end
ReplaceMapEvent(15, nil, randomJump)
ReplaceMapEvent(16, nil, randomJump)

local crossContinents = "MMerge.CrossContinents."
local actorRulesVersionKey = "MMerge.BrAlvar.ActorRulesVersion"

-- BrAlvar.lua 685,693-742: map-local directed relations and existing monster294's Wimp AI type.
-- The importer authenticates and expands the 24 source group rules to 198 concrete monster-ID pairs.
local function initializeActorRules()
    local relations = evt.importedBrAlvarMonsterRelations
    if type(relations) ~= "table" or #relations ~= 198 then
        error("BrAlvar source monster relations are unavailable")
    end
    for _, relation in ipairs(relations) do
        evt.SetLocalMonsterRelation(relation[1], relation[2], relation[3])
    end
    if not evt.SetMapMonsterAiType(294, 1) then
        error("BrAlvar source monster AI override is unavailable")
    end
    evt.SetMapVar(actorRulesVersionKey, 1)
end

RegisterMapOnLoadEvent(65453, nil, initializeActorRules)
RegisterMapAfterLoadEvent(65452, nil, function()
    -- Entry already initialized the rules. Only old cold saves need this isolated migration;
    -- modern cold restores keep their saved relations/AI and never replay split, doors or daylight.
    if evt.GetMapVar(actorRulesVersionKey, 0) == 0 then
        initializeActorRules()
    end
end)

local function triggerModel(eventId)
    -- EvtMechanismAction::Trigger is 2. Definitions and sounds come from the map companion,
    -- so saved motion can bind before OnLoad, which is intentionally skipped on a cold load.
    if not evt.SetOutdoorModelMechanismState(eventId, 2) then
        error("BrAlvar tower mechanism is unavailable: " .. eventId)
    end
end

local daylightDayStateKey = "MMerge.BrAlvar.DayState"
local daylightVersionKey = "MMerge.BrAlvar.DaylightVersion"

-- BrAlvar.lua 65-84. False means Closed=false in the source, so open all three
-- before toggling the selected model. Success means the models exist, not that
-- they are stationary; another click may reverse their current motion.
local function switchDaylight(eventId, state)
    if not (evt.SetOutdoorModelMechanismState(101, 0)
        and evt.SetOutdoorModelMechanismState(102, 0)
        and evt.SetOutdoorModelMechanismState(103, 0)) then
        error("BrAlvar daylight mechanisms are unavailable")
    end

    local hour, minute = 11, false
    if state == 1 then
        hour, minute = 4, 12
    elseif state == 2 then
        hour, minute = 6, 12
    end
    if not evt.SetOutdoorLightTime(hour, minute) then
        error("BrAlvar daylight override is unavailable")
    end
    evt.SetMapVar(daylightDayStateKey, state)
    triggerModel(eventId)
end

for index = 1, 3 do
    local state = index
    local eventId = 100 + index
    ReplaceMapEvent(eventId, nil, function()
        switchDaylight(eventId, state)
    end)
end

local function initializeDaylight()
    -- Source entry first applies Hour or4 / Minute or12. A saved false minute
    -- temporarily becomes12, then the successful three-model switch overwrites
    -- both values before light refresh (state3 restores false, inheriting the
    -- actual minute). The mandatory definitions make only that final projection
    -- observable; this never changes Game.Time. Lua's valid minute0 is not false.
    -- Source line102 always toggles Time_1, even when the saved DayState is2 or3.
    switchDaylight(101, evt.GetMapVar(daylightDayStateKey, 1))
    evt.SetMapVar(daylightVersionKey, 1)
end

RegisterMapOnLoadEvent(65459, nil, initializeDaylight)
RegisterMapAfterLoadEvent(65458, nil, function()
    -- Only migrate saves from before this feature. Modern cold restores retain
    -- their typed light override, false minute and saved model motion verbatim;
    -- they must not replay normal reentry or unrelated split/door-key OnLoad.
    if evt.GetMapVar(daylightVersionKey, 0) == 0 then
        initializeDaylight()
    end
end)

-- BrAlvar.lua 687-689,771-775. These are exact source decoration indices, not shared event keys.
local sourceJourneyVersionKey = "MMerge.BrAlvar.SourceJourneyVersion"
local function initializeSourceJourney()
    if evt.GetGlobalVar(crossContinents .. "QuestFinished", 0) == 0 then
        SetQBit(QBit(1715))
    end
    for _, index in ipairs({381, 676, 792}) do
        if not evt.SetMapDecorationVisible(index, false) then
            error("BrAlvar source decoration is unavailable: " .. index)
        end
    end
    evt.SetMapVar(sourceJourneyVersionKey, 1)
end

RegisterMapOnLoadEvent(65456, nil, initializeSourceJourney)
RegisterMapAfterLoadEvent(65455, nil, function()
    -- Older cold saves initialize only this new feature; existing timer progress is reconciled by the runtime.
    if evt.GetMapVar(sourceJourneyVersionKey, 0) == 0 then
        initializeSourceJourney()
    end
end)

local function summon(monsterType, level, count, x, y, z, group)
    if not evt.SummonMonsters(monsterType, level, count, x, y, z, group) then
        error("BrAlvar tower monster summon failed")
    end
end

-- BrAlvar.lua 412-429,691. The interval is sixty game seconds, not real seconds.
-- Friend maintenance already belongs to the native companion runtime; NoFly is not scheduled by the source.
RegisterMapTimerEvent(65457, 60, function()
    local x, y, z = evt.GetPartyPosition()
    if evt.GetPartyFlying() then
        local fromX = evt.RandomBetween(x - 500, x + 500)
        local fromY = evt.RandomBetween(y - 500, y + 500)
        local fromZ = evt.RandomBetween(z - 200, z + 200)
        -- The ordinary runtime ABI takes Skill before Mastery; source fields are Spell/Mastery/Skill.
        evt.CastSpell(18, 7, 3, fromX, fromY, fromZ, x, y, z)
        summon(3, 2, 1, fromX, fromY, fromZ, 13)
    end

    local dx, dy = 332 - x, -268 - y
    if evt.GetGlobalVar(crossContinents .. "EnteredBreach", 0) == 0
        and dx * dx + dy * dy < 2500 * 2500 then
        local nextX = evt.RandomBetween(3000, 4000)
        if evt.RandomBetween(1, 2) == 1 then nextX = -nextX end
        local nextY = evt.RandomBetween(3000, 4000)
        if evt.RandomBetween(1, 2) == 1 then nextY = -nextY end
        local _, _, footZ = evt.GetPartyPosition(true)
        if not evt.SetPartyPosition(nextX, nextY, footZ) then
            error("BrAlvar center displacement failed")
        end
        evt.PlaySound(165)
    end
end)

-- BrAlvar.lua 778-786. Stop ordinary edge travel, then nudge before queuing the source jump.
local boundaryDirections = {[0] = 1532, [1] = 500, [2] = 968, [3] = 0}
RegisterMapBoundaryHook(65454, nil, function(context)
    local direction = boundaryDirections[context.boundaryEdge]
    if direction == nil then error("BrAlvar boundary side is unavailable") end
    evt.SetHookBlocked(true)
    local x, y, z = evt.GetPartyPosition(true)
    x = x + (x < 0 and 100 or -100)
    y = y + (y < 0 and 100 or -100)
    if not evt.SetPartyPosition(x, y, z) then
        error("BrAlvar boundary displacement failed")
    end
    if not evt.Jump(direction, evt.RandomBetween(128, 256), 2500) then
        error("BrAlvar boundary jump failed")
    end
end)

-- BrAlvar.lua 457–485: Elev_1/2/3_Door share one source progression flag and one population.
for index = 1, 3 do
    local eventId = 103 + index
    ReplaceMapEvent(eventId, nil, function()
        if evt.GetGlobalVar(crossContinents .. "BrFirstFloor", 0) ~= 0 then
            triggerModel(eventId)
        else
            local _, _, z = evt.GetPartyPosition()
            if z >= 1980 then
                summon(3, 2, 3, 1343, 763, 1992, 13)
                summon(3, 2, 2, -1397, -731, 2071, 13)
                summon(3, 2, 2, -1390, 677, 2069, 13)
                summon(3, 2, 2, 1354, -646, 2487, 13)
                evt.SetGlobalVar(crossContinents .. "BrFirstFloor", 1)
                triggerModel(eventId)
            else
                evt.StatusText(evt.strings[3])
            end
        end
    end)
end

-- BrAlvar.lua 488–516 sets BrSecFloor before its summons. Preserve that source ordering.
for index = 1, 3 do
    local eventId = 106 + index
    ReplaceMapEvent(eventId, nil, function()
        if evt.GetGlobalVar(crossContinents .. "BrSecFloor", 0) ~= 0 then
            triggerModel(eventId)
        else
            local _, _, z = evt.GetPartyPosition()
            if z >= 1990 then
                evt.SetGlobalVar(crossContinents .. "BrSecFloor", 1)
                summon(3, 2, 3, 75, -1077, 3126, 13)
                summon(3, 2, 3, 1067, -100, 3127, 13)
                summon(3, 2, 3, -1025, 647, 3127, 13)
                summon(3, 2, 3, 401, 1156, 3127, 13)
                triggerModel(eventId)
            else
                evt.StatusText(evt.strings[3])
            end
        end
    end)
end

-- BrAlvar.lua 519–632: 110–113 LibDoor_1..4; 114–116 Mus_Door_2..4; 117 Mus_Door_1;
-- 118–124 RoomDoor_1..7; 125 RoomElev. Only LibDoor_3/4 and Mus_Door_1 require BrThirdFloor.
-- In particular, the locked museum door is not a shortcut for the first ascent.
for sourceEventId = 110, 125 do
    local eventId = sourceEventId
    local needsThirdFloor = eventId == 112 or eventId == 113 or eventId == 117
    ReplaceMapEvent(eventId, nil, function()
        if not needsThirdFloor or evt.GetGlobalVar(crossContinents .. "BrThirdFloor", 0) ~= 0 then
            triggerModel(eventId)
        else
            evt.StatusText(evt.strings[3])
        end
    end)
end

-- BrAlvar.lua 634–677. These are source launch points, distinct from the button models.
local launchPoints = {{10, -2334}, {1971, 1139}, {-1962, 1137}}
local function jumpToTop(point)
    local x, y, z = evt.GetPartyPosition()
    local dx, dy = point[1] - x, point[2] - y
    if dx * dx + dy * dy < 100 * 100 then
        if z > 4410 then
            error("BrAlvar tower launch is above its source apex")
        end
        if not evt.ExtendFeatherFall(240) then
            error("BrAlvar tower Feather Fall extension failed")
        end
        if not evt.Jump(0, 512, math.sqrt(2 * 800 * (4410 - z))) then
            error("BrAlvar tower jump failed")
        end
    end
end

for index = 1, 3 do
    local eventId = 125 + index
    local point = launchPoints[index]
    ReplaceMapEvent(eventId, nil, function()
        if evt.GetGlobalVar(crossContinents .. "BrThirdFloor", 0) ~= 0 then
            triggerModel(eventId)
            jumpToTop(point)
        else
            local _, _, z = evt.GetPartyPosition()
            if z >= 3120 then
                summon(3, 3, 1, 14, 86, 5184, 13)
                summon(3, 2, 4, 14, 86, 5184, 13)
                evt.SetGlobalVar(crossContinents .. "BrThirdFloor", 1)
                triggerModel(eventId)
                jumpToTop(point)
            else
                evt.StatusText(evt.strings[4])
            end
        end
    end)
end
