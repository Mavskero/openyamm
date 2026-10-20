-- SHA-gated translation of audited cross-continent quest hooks and Verdant dialogue.
-- Dialogue comes from imported source tables; the original MMExtension script is never executed.
local prefix = "MMerge.CrossContinents."
local npcId = 803
local introSlot = 1
local refreshVerdantTopics

local function get(name)
    return evt.GetGlobalVar(prefix .. name, 0) ~= 0
end

local function set(name, value)
    evt.SetGlobalVar(prefix .. name, value and 1 or 0)
end

local function introTopic()
    if get("GotMainQuest") or get("AllStoriesFinished") then
        return 0
    elseif not get("MetVerdant") then
        return 1778
    elseif get("GotIntro2") then
        return 1780
    end
    return 1779
end

RegisterGlobalNpcEnterHook(65500, nil, function(context)
    if context.npcId ~= npcId then
        return
    end
    if not get("GotMainQuest") then
        set("GotIntro1", false)
        set("GotIntro2", false)
    end
    evt.SetNPCDialogueBranch("")
    refreshVerdantTopics()
end)

-- Original chapters2c/2d/3a, Quest_CrossContinents.lua121–186.
-- Temporary callback closures are re-created by the committed scene's Lua owner.
local firstCatchArmed = false
local minuteTicks = 256
local dayTicks = 1440 * minuteTicks
local monthTicks = 28 * dayTicks
local arenas = { ["d42.blv"] = true, ["7d05.blv"] = true, ["zarena.blv"] = true }
local function isArena()
    return arenas[string.lower(evt.GetCurrentMapName())] == true
end
local function noDetectedEnemies()
    local yellow, red = evt.GetEnemyDetectorState()
    return not yellow and not red
end
local function time(name)
    return evt.GetGlobalTimeTicks(prefix .. name, 0)
end
local function setTime(name, ticks)
    evt.SetGlobalTimeTicks(prefix .. name, ticks)
end

RegisterGlobalAfterLoadEvent(65505, nil, function()
    firstCatchArmed = false
    local now = evt.GameTimeTicks()
    local continent = evt.GetTownPortalContinent()
    if not get("MetVerdant") and not isArena() then
        if time("MeetTime") == 0 then
            setTime("MeetTime", now + monthTicks * 8)
        elseif now > time("MeetTime") then
            firstCatchArmed = true
            setTime("MeetTime", now + dayTicks * 2)
        end
    end
    -- No extra flight/enemy/screen gate in the original letter clause.
    if get("MetVerdant") and not evt.HasQuestBit(1104) and continent == 3
        and evt.GiveOwnedQuestItem(0, 2125, true) then
        evt.SetQuestBit(1104)
        evt.SetQuestBit(1105)
        -- Fixed player0 delivery or a persistent mouse-delivery owner; commit only after ownership.
        evt.SetNPCGreeting(npcId, 330)
        evt.SpeakNPC(npcId)
    end
    if get("MetVerdant") and not get("QuestFinished") and not get("HideVerdant")
        and continent >= 1 and continent <= 3
        and evt.GetGlobalVar(prefix .. "VerdantAt", 0) ~= continent then
        evt.SetGlobalVar(prefix .. "VerdantAt", continent)
        evt.MoveNPC(npcId, ({185, 641, 1195})[continent])
    end
end)

RegisterGlobalTemporaryTimerEvent(65506, 600, function()
    -- Source FirstCatch only tests screen and enemies; flying alone does not postpone it.
    if firstCatchArmed and evt.IsNormalGameplayScreen() and noDetectedEnemies() then
        firstCatchArmed = false
        evt.SpeakNPC(npcId)
    end
end)

RegisterGlobalOnLeaveEvent(65507, nil, function()
    firstCatchArmed = false
end)

local function advanceIntroduction()
    if get("GotMainQuest") then
        return
    end
    local textId
    if not get("MetVerdant") then
        set("MetVerdant", true)
        textId = 2149
    elseif not get("GotIntro1") then
        set("GotIntro1", true)
        textId = 2150
    elseif not get("GotIntro2") then
        set("GotIntro2", true)
        textId = 2151
    else
        set("GotMainQuest", true)
        -- Product compatibility policy: original supplied Lua only reads StartedAt, never initializes it.
        -- New introductions use the physical continent; old missing values are inferred after a committed load.
        local startedAt = evt.GetGlobalVar(prefix .. "StartedAt", 0)
        if startedAt == 0 then
            local continent = evt.GetCurrentContinent()
            if continent < 1 or continent > 3 then error("Invalid introduction continent") end
            evt.SetGlobalVar(prefix .. "StartedAt", continent)
        elseif startedAt < 1 or startedAt > 3 then
            error("Invalid saved StartedAt")
        end
        textId = 2152
        AddValue(Experience, 0)
        evt.SetNPCGreeting(npcId, 328)
    end
    evt.SimpleMessage(evt.NPCText(textId))
    refreshVerdantTopics()
end

for _, topicId in ipairs({1778, 1779, 1780}) do
    ReplaceGlobalEvent(topicId, nil, advanceIntroduction)
    RegisterCanShowTopic(topicId, function()
        return introTopic() == topicId
    end)
end

-- Original OpenChest records generation even if every item record is occupied.
RegisterGlobalChestOpenHook(65501, nil, function(context)
    if get("ScrollGotten") or get("ScrollGenerated") or evt.GetPlayerExperience(0) <= 50000 then
        return
    end
    local continent = evt.GetCurrentContinent()
    local scroll = ({770, 771, 772})[continent] or 770
    evt.InsertChestItem(context.chestId, scroll)
    set("ScrollGenerated", true)
    evt.SetGlobalVar("MMerge.LostItems." .. scroll, continent)
end)

RegisterGlobalAfterLoadEvent(65504, nil, function()
    if get("GotMainQuest") and not get("GotConnectorStone") then
        local startedAt = evt.GetGlobalVar(prefix .. "StartedAt", 0)
        if startedAt == 0 then
            local continent = evt.GetCurrentContinent()
            if continent < 1 or continent > 3 then error("Invalid inferred introduction continent") end
            evt.SetGlobalVar(prefix .. "StartedAt", continent)
            set("StartedAtInferred", true)
        elseif startedAt < 1 or startedAt > 3 then
            error("Invalid saved StartedAt")
        end
    end
end)

RegisterGlobalGotItemHook(65502, nil, function(context)
    if context.itemId >= 770 and context.itemId <= 772 then
        set("ScrollGotten", true)
    end
end)

-- Dispatched only after the ordinary natural-location timer activation has completed.
RegisterGlobalDimensionDoorActivatedHook(65503, nil, function()
    if get("ScrollGotten") and not get("GotMainQuest") then
        evt.SetNPCGreeting(npcId, 329)
        evt.SpeakNPC(npcId)
    end
end)

-- Original ContTopic/Hints branch. Persist quest flags, never the dialogue branch itself.
local function finished(continent)
    return get("ContinentFinished." .. continent)
end
local function physicalContinent()
    local continent = evt.GetCurrentContinent()
    if continent < 1 or continent > 3 then error("Invalid dialogue continent") end
    return continent
end
local function selectedHint()
    local continent = physicalContinent()
    if continent == 2 and evt.HasQuestBit(611) then return 894 end
    if continent == 2 and evt.HasQuestBit(612) then return 892 end
    return ({732, 888, 1359})[continent]
end
local function inHints()
    return evt.GetNPCDialogueBranch() == "Hints"
end
local function canBreachTravel()
    return inHints() and get("QuestFinished")
        and string.lower(evt.GetCurrentMapFileName()) ~= string.lower(breachTravel.mapName)
end
local function showContinent()
    return not inHints() and get("GotMainQuest") and not get("AllStoriesFinished")
        and not finished(evt.GetTownPortalContinent())
end
local function showHint()
    return inHints() and get("GotMainQuest") and not get("AllStoriesFinished")
        and not finished(math.min(evt.GetTownPortalContinent(), 3))
end
refreshVerdantTopics = function()
    for slot = 0, 5 do evt.SetNPCTopic(npcId, slot, 0) end
    if inHints() then
        evt.SetNPCTopic(npcId, 0, 1781)
        if showHint() then evt.SetNPCTopic(npcId, 1, selectedHint()) end
        if get("GotConnectorStone") then evt.SetNPCTopic(npcId, 2, 1788) end
        if canBreachTravel() then evt.SetNPCTopic(npcId, 3, breachTravel.topicId) end
    else
        if not get("QuestFinished") and get("GotFinalQuest") then
            evt.SetNPCTopic(npcId, 0, get("GotInstructions") and 1786 or 1785)
        elseif not get("QuestFinished") and not get("GotFinalQuest") and get("ShowInterlude") then
            evt.SetNPCTopic(npcId, 0, 1787)
        elseif showContinent() then
            evt.SetNPCTopic(npcId, 0, 1781 + physicalContinent())
        end
        evt.SetNPCTopic(npcId, 1, get("GotMainQuest") and 732 or introTopic())
    end
end

-- Original Quest_CrossContinents.lua447-463; both Sleep calls use real milliseconds.
RegisterNpcScopedTopic(npcId, breachTravel.topicId, function()
    if not canBreachTravel() then return end
    evt.SetNPCDialogueBranch("")
    refreshVerdantTopics()
    evt.SimpleMessage(breachTravel.message)
    if not evt.MoveToMapAfterDelay(breachTravel.mapName, 1024, 100) then
        error("Unable to queue the source BreachTravel delayed map move")
    end
end, canBreachTravel)

for _, topicId in ipairs({1782, 1783, 1784}) do
    RegisterNpcScopedTopic(npcId, topicId, function()
        evt.SimpleMessage(evt.NPCText(2153 + physicalContinent()))
    end, function() return showContinent() and topicId == 1781 + physicalContinent() end)
end
RegisterNpcScopedTopic(npcId, 1781, function()
    evt.SimpleMessage(evt.NPCText(2153))
end, function() return inHints() and get("GotMainQuest") end)
RegisterNpcScopedTopic(npcId, 732, function(original)
    if inHints() then
        if original == nil then error("Missing original StdTopic732 handler") end
        original()
    else
        evt.SetNPCDialogueBranch("Hints")
        refreshVerdantTopics()
        evt.SimpleMessage(evt.NPCGreeting(26, 1))
    end
end, function()
    return inHints() and showHint() and selectedHint() == 732
        or not inHints() and get("GotMainQuest")
end)
for _, topicId in ipairs({888, 894, 892, 1359}) do
    RegisterNpcScopedTopic(npcId, topicId, function(original)
        if original == nil then error("Missing original StdTopic handler") end
        original()
    end, function() return showHint() and selectedHint() == topicId end)
end
RegisterGlobalGameplayActionHook(65508, nil, function(context)
    if context.npcId == npcId and context.actionId == 113 and context.menuId == 4 and inHints() then
        evt.SetNPCDialogueBranch("")
        refreshVerdantTopics()
        evt.SetHookBlocked(true)
    end
end)
RegisterGlobalNpcExitHook(65509, nil, function(context)
    if context.npcId == npcId then evt.SetNPCDialogueBranch("") end
end)

-- Source3b: one-minute period on the original absolute two-minute anchor.
-- Explicit per-map closure translation avoids the original stale CCTimers closure capture.
local connectorArmed = false
local connectorStart = 0
local connectorContinent = 0
RegisterGlobalAfterLoadEvent(65510, nil, function()
    evt.PrepareConnectorState()
    connectorArmed = false
    local continent = evt.GetTownPortalContinent()
    local maps = { ["oute3.odm"] = true, ["7out02.odm"] = true, ["out03.odm"] = true }
    if not get("GotConnectorStone") and get("GotMainQuest")
        and continent ~= evt.GetGlobalVar(prefix .. "StartedAt", 0)
        and maps[string.lower(evt.GetCurrentMapName())] then
        connectorArmed = true
        connectorStart = evt.GameTimeTicks() + minuteTicks * 5
        connectorContinent = continent
    end
end)
RegisterGlobalTemporaryTimerEvent(65511, 60, function()
    if connectorArmed and not get("GotConnectorStone") and evt.IsNormalGameplayScreen()
        and noDetectedEnemies() and not evt.IsPartyFlying() and not isArena()
        and evt.GameTimeTicks() > connectorStart and evt.GiveOwnedQuestItem(0, 624) then
        connectorArmed = false
        set("GotConnectorStone", true)
        evt.SetGlobalVar("MMerge.LostItems.625", connectorContinent)
        evt.SetNPCGreeting(npcId, 331)
        evt.SpeakNPC(npcId)
    end
end, nil, nil, nil, 2)
RegisterGlobalOnLeaveEvent(65512, nil, function()
    evt.PrepareConnectorState()
    connectorArmed = false
    -- Source only changes active party inventories, never held cursor items or pending owners.
    if evt.HasItemAnywhere(625) and (evt.GetTownPortalContinent() == 4
        or evt.GameTimeTicks() > time("NextConRecharge")) then
        evt.ReplacePartyInventoryItems(625, 624)
    end
end)
RegisterNpcScopedTopic(npcId, 1788, function()
    evt.SimpleMessage(evt.NPCText(2160))
    if not evt.HasOwnedQuestItem(624) and not evt.HasOwnedQuestItem(625) then
        evt.GiveOwnedQuestItem(0, 624)
    end
end, function() return inHints() and get("GotConnectorStone") end)


-- Source chapter4. Only the automated catch checks screen/enemies/flying;
-- a real EnterNPC803 awards without those extra gates or introduction prerequisites.
local rewardContinent = 0
local finalCatchArmed = false
RegisterGlobalAfterLoadEvent(65513, nil, function()
    rewardContinent = 0
    finalCatchArmed = false
    evt.SetNPCPrimaryGreetingText(npcId)
    if get("GotFinalQuest") then return end
    for continent, bit in ipairs({228, 783, 784}) do
        set("ContinentFinished." .. continent, evt.HasQuestBit(bit))
    end
    local continent = evt.GetTownPortalContinent()
    if continent < 4 and finished(continent) and not get("GotReward." .. continent) and not isArena() then
        rewardContinent = continent
        evt.SetNPCGreeting(npcId, 328 - continent)
    end
    if get("AllStoriesFinished") then
        set("HideVerdant", true)
        evt.MoveNPC(npcId, 0)
        if time("FQCatchTime") == 0 then setTime("FQCatchTime", evt.GameTimeTicks() + dayTicks * 7) end
        finalCatchArmed = evt.GameTimeTicks() >= time("FQCatchTime")
    end
end)
RegisterGlobalNpcEnterHook(65514, nil, function(context)
    if context.npcId ~= npcId or rewardContinent == 0 then return end
    local continent = rewardContinent
    if get("GotReward." .. continent) then rewardContinent = 0; return end
    local count = 0
    for value = 1, 3 do if get("GotReward." .. value) then count = count + 1 end end
    if count < 2 then
        if not evt.GiveOwnedQuestItem(-1, 543, true) then return end
    elseif count == 2 then
        -- Original LocalFile changes only primary until LeaveMap/LeaveGame. Keep it NPC803-local,
        -- transient and unsaved; cold reload does not recreate the append from persistent flags.
        evt.SetNPCPrimaryGreetingText(npcId, evt.NPCGreeting(328 - continent, 0) .. " " .. evt.NPCText(2161))
        set("ImporvedConnector", true)
        set("AllStoriesFinished", true)
        set("ShowInterlude", true)
        setTime("MeetTime", evt.GameTimeTicks() + monthTicks * 2)
    end
    set("GotReward." .. continent, true)
    rewardContinent = 0
    refreshVerdantTopics()
end)
RegisterGlobalTemporaryTimerEvent(65515, 600, function()
    if not evt.IsNormalGameplayScreen() or not noDetectedEnemies() or evt.IsPartyFlying() then return end
    if finalCatchArmed and not get("GotFinalQuest") then
        finalCatchArmed = false
        evt.SetQuestBit(1713)
        evt.SetNPCGreeting(npcId, 324)
        AddValue(Experience, 0)
        set("GotFinalQuest", true)
        set("ShowInterlude", false)
        evt.SpeakNPC(npcId)
    elseif rewardContinent ~= 0 then
        evt.SpeakNPC(npcId)
    end
end)
RegisterGlobalOnLeaveEvent(65516, nil, function()
    rewardContinent = 0
    finalCatchArmed = false
    evt.SetNPCPrimaryGreetingText(npcId)
end)
for _, topicId in ipairs({1785, 1786, 1787}) do
    RegisterNpcScopedTopic(npcId, topicId, function()
        if topicId == 1785 then
            set("GotInstructions", true)
        elseif topicId == 1786 then
            set("GotInstructions", true)
            evt.SetQuestBit(1713)
        end
        evt.SimpleMessage(evt.NPCText(topicId == 1785 and 2157 or topicId == 1786 and 2158 or 2159))
        refreshVerdantTopics()
    end, function()
        if inHints() or get("QuestFinished") then return false end
        if topicId == 1787 then return not get("GotFinalQuest") and get("ShowInterlude") end
        return get("GotFinalQuest") and (topicId == 1785 and not get("GotInstructions")
            or topicId == 1786 and get("GotInstructions"))
    end)
end
