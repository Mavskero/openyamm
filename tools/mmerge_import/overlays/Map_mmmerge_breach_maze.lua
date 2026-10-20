-- Source: Breach.lua event 54. Queue maze entry intent; the destination request owns its transaction.
ReplaceMapEvent(54, nil, function()
    evt.RequestBreachMazeTransition()
end)

-- Breach.lua 100–115. The original end card is independent of the MM8 ending movie.
local crossContinents = "MMerge.CrossContinents."
RegisterMapOnLoadEvent(65462, nil, function()
    evt.SetGlobalVar(crossContinents .. "QuestFinished", 1)
    if evt.GetGlobalVar(crossContinents .. "GotEndCard", 0) == 0 and evt.RequestBreachEndCard() then
        evt.SetGlobalVar(crossContinents .. "GotEndCard", 1)
    end
    ClearQBit(1713)
    ClearQBit(1714)
    ClearQBit(1715)
end)

-- Breach.lua 89-98: coordinate and pose writes retain the current movement state.
RegisterMapTimerEvent(65463, 60, function()
    local _, _, footZ = evt.GetPartyPosition(true)
    if footZ < 500 then
        if not evt.SetPartyPosition(4, 34, 854) or not evt.SetPartyDirection(1310, 0) then
            error("Breach low-void displacement failed")
        end
        evt.PlaySound(165)
    end
end)

-- Breach.lua 16-33. This delay belongs to the live map script, not the save or campaign.
local portalDelay = 0
local telelocatorKey = "MMerge.Breach.TelelocatorSet"
local telelocatorItem = 666
local function telelocatorSet()
    return evt.GetMapVar(telelocatorKey, 0) ~= 0
end

ReplaceMapEvent(81, nil, function(step)
    if step == 1 then return end
    if step == 2 then
        -- The question continuation never repeats the initial delay or draws a second destination.
        local starts = {"out01.odm", "7out01.odm", "oute3.odm"}
        evt.Add(eventSupport.varTag.Experience, 0)
        local destination = starts[evt.RandomBetween(1, #starts)]
        if not evt.RestartCampaignPreservingParty(destination) then
            error("Breach campaign restart request failed")
        end
        return
    end

    local now = evt.GameTimeTicks()
    if portalDelay > now then return end
    if not telelocatorSet() then
        portalDelay = now + 2 * 256
        evt.ActivateDimensionDoor()
        evt.OpenDimensionDoor()
    else
        -- Source string.lower(answer)=="y" accepts Y but does not trim surrounding whitespace.
        evt.AskQuestionWithAnswerSteps(81, 1, evt.strings[1], {{Answer = "y", Step = 2}}, false)
    end
end)

-- Breach.lua 35-64. Hints use the imported source item name, with no native table lookup in Lua.
RegisterEvent(400, nil, nil, evt.strings[2])
ReplaceMapEvent(401, nil, function()
    evt.StatusText(evt.strings[3])
end, evt.importedItemNames[telelocatorItem])

ReplaceMapEvent(402, nil, function()
    evt.ForPlayer(eventSupport.partySelector.All)
    if HasItem(telelocatorItem) then
        RemoveItem(telelocatorItem)
        evt.SetFacetBit(400, eventSupport.facetBits.Invisible, false)
        evt.SetMapVar(telelocatorKey, 1)
        evt.StatusText(evt.strings[4])
    elseif not telelocatorSet() then
        evt.StatusText(evt.strings[5])
    end
end, evt.strings[2])

ReplaceMapEvent(403, nil, function()
    evt.ForPlayer(eventSupport.partySelector.Member0)
    evt.Add(InventoryItem(telelocatorItem), telelocatorItem)
    evt.SetFacetBit(400, eventSupport.facetBits.Invisible, true)
    evt.SetMapVar(telelocatorKey, 0)
end, evt.importedItemNames[telelocatorItem])

RegisterMapAfterLoadEvent(65464, nil, function()
    evt.SetFacetBit(400, eventSupport.facetBits.Invisible, not telelocatorSet())
end)

-- Breach.lua 66-87: raw chest indices and the original library sum key are intentional.
for index = 1, 15 do
    local chest = index
    ReplaceMapEvent(300 + chest, nil, function()
        evt.OpenChest(chest)
    end)
end

ReplaceMapEvent(82, nil, function()
    local x, y, z = evt.GetPartyPosition()
    local key = "MMerge.Breach.LibKeys." .. (math.ceil(x / 200) + math.ceil(y / 200) + math.ceil(z / 100))
    if evt.GetMapVar(key, 0) ~= 0 then return end
    evt.SetMapVar(key, 1)
    evt.ForPlayer(eventSupport.partySelector.Member0)
    evt.GiveItem(evt.RandomBetween(3, 6), evt.RandomBetween(16, 17), 0)
end)

ReplaceMapEvent(900, nil, function()
    evt.EnterHouse(1607)
end)

-- Breach.lua 118-121 reflects both coordinates without replacing motion or entering another map.
RegisterMapBoundaryHook(65465, nil, function()
    evt.SetHookBlocked(true)
    local x, y, z = evt.GetPartyPosition(true)
    if not evt.SetPartyPosition(-x, -y, z) then error("Breach boundary reflection failed") end
end)

local daylightStateKey = "MMerge.Breach.DayState"
local daylightVersionKey = "MMerge.Breach.DaylightVersion"
local function triggerModel(eventId)
    if not evt.SetOutdoorModelMechanismState(eventId, 2) then
        error("Breach tower mechanism is unavailable: " .. eventId)
    end
end

-- Breach.lua 170-190: open all three targets before toggling the selected model.
local function switchDaylight(eventId, state)
    if not (evt.SetOutdoorModelMechanismState(101, 0)
        and evt.SetOutdoorModelMechanismState(102, 0)
        and evt.SetOutdoorModelMechanismState(103, 0)) then
        error("Breach daylight mechanisms are unavailable")
    end
    local hour, minute = 11, false
    if state == 1 then
        hour, minute = 4, 12
    elseif state == 2 then
        hour, minute = 6, 12
    end
    if not evt.SetOutdoorLightTime(hour, minute) then error("Breach daylight projection failed") end
    evt.SetMapVar(daylightStateKey, state)
    triggerModel(eventId)
end

for index = 1, 3 do
    local state, eventId = index, 100 + index
    ReplaceMapEvent(eventId, nil, function()
        switchDaylight(eventId, state)
    end)
end

local function initializeDaylight()
    -- Source207 toggles Time_1 even when the retained DayState is2 or3; Breach defaults to3.
    switchDaylight(101, evt.GetMapVar(daylightStateKey, 3))
    evt.SetMapVar(daylightVersionKey, 1)
end
RegisterMapOnLoadEvent(65466, nil, initializeDaylight)
RegisterMapAfterLoadEvent(65467, nil, function()
    -- Old cold saves initialize only this new feature. Modern saves retain light and moving-model fractions.
    if evt.GetMapVar(daylightVersionKey, 0) == 0 then initializeDaylight() end
end)

-- Breach.lua 213-317 has no BrAlvar progression gates or first-visit monster summons.
for index = 104, 125 do
    local eventId = index
    ReplaceMapEvent(eventId, nil, function()
        triggerModel(eventId)
    end)
end

local launchPoints = {{10, -2334}, {1971, 1139}, {-1962, 1137}}
for index = 1, 3 do
    local eventId, point = 125 + index, launchPoints[index]
    ReplaceMapEvent(eventId, nil, function()
        triggerModel(eventId)
        local x, y, z = evt.GetPartyPosition()
        local dx, dy = point[1] - x, point[2] - y
        if dx * dx + dy * dy < 100 * 100 then
            if z > 4410 then error("Breach tower launch is above its source apex") end
            -- Source four game minutes equal eight real seconds at the shared ordinary clock rate.
            if not evt.ExtendFeatherFall(8) then error("Breach Feather Fall extension failed") end
            if not evt.Jump(0, 512, math.sqrt(2 * 800 * (4410 - z))) then
                error("Breach tower jump failed")
            end
        end
    end)
end
