-- SHA-gated translation of Quest_CrossContinents.lua554–764.
-- BrAlvar split/join, doors, maze, travel, rewards and QuestFinished are separate map profiles.
-- Source Sleep/one-second result presentation is not implemented by this dialogue subset.
local chaosPrefix = "MMerge.CrossContinents."
local chaosNpc = 1092
local captureOffered = false -- Source topic text is transient, not saved quest state.

local function number(name)
    return evt.GetGlobalVar(chaosPrefix .. name, 0)
end
local function flag(name)
    return number(name) ~= 0
end
local function write(name, value)
    evt.SetGlobalVar(chaosPrefix .. name, value)
end
local function active()
    return not flag("QuestFinished")
end
local function text(id)
    local value = evt.NPCText(id)
    if value == "" then error("Missing Runaway Chaos NPC text " .. id) end
    return value
end
local function answeredCount()
    local count = number("RiddlesAnswered")
    if count < 0 or count > 11 then error("Invalid Runaway Chaos riddle count") end
    local solved = 0
    for id = 1, 11 do
        if flag("RiddleSolved." .. id) then solved = solved + 1 end
    end
    if solved ~= count then error("Runaway Chaos solved riddles do not match their count") end
    return count
end
local function refreshAppearance()
    local index = math.min(answeredCount() + 1, 6)
    evt.SetNPCPicture(chaosNpc, ({354, 358, 66, 167, 257, 273})[index])
    evt.SetNPCProfession(chaosNpc, ({24, 33, 77, 42, 65, 0})[index])
    if index == 6 or captureOffered then
        evt.SetNPCName(chaosNpc, "") -- Remove the disguise; use the imported NPCDataTxt name.
        evt.SetNPCName(chaosNpc, evt.NPCName(chaosNpc))
    else
        evt.SetNPCName(chaosNpc, text(2708 + index))
    end
end
local function canAskRiddle()
    return active() and (flag("GotFQHint3") and answeredCount() < 5 or flag("CoughtChaos"))
end
local function canRequestCapture()
    return active() and flag("GotFQHint2") and not flag("CoughtChaos")
end
local function refreshChaosTopics()
    evt.SetNPCTopic(chaosNpc, 0, canRequestCapture() and (captureOffered and 1792 or 1794) or 0)
    evt.SetNPCTopic(chaosNpc, 1, canAskRiddle() and 1790 or 0)
end
local function friendAvailable(friend)
    -- Preserve the original unsplit hint subset. After a split, native receipt/custody owns availability;
    -- returned NPCs stay removed when AfterLoad/NpcEnter rebuilds the quest topics.
    return active() and (not flag("BreachSplit") or evt.IsBreachAlvarCompanionAvailable(friend))
end
local function refreshFriendTopic(friend)
    evt.SetNPCTopic(friend, 0, friendAvailable(friend) and (number("HintByNPC." .. friend) ~= 0 and 1789 or 1793) or 0)
end

-- Original hint assignment follows encounter order, independently of which friend was found first.
for place = 1, 4 do
    local friend = 771 + place
    RegisterNpcScopedTopic(friend, 1793, function()
        if not friendAvailable(friend) then return end
        local hint = number("HintByNPC." .. friend)
        if hint == 0 then
            hint = number("GotFQHints") + 1
            if hint < 1 or hint > 4 then error("Invalid Runaway Chaos hint sequence") end
            write("GotFQHints", hint)
            write("GotFQHint" .. hint, 1)
            write("HintByNPC." .. friend, hint)
            AddValue(Experience, 0)
        end
        if hint < 1 or hint > 4 then error("Invalid Runaway Chaos friend hint") end
        local greeting
        if flag("FQMercRnd." .. friend) then
            greeting = text(place % 2 == 1 and 2245 or 2246)
        else
            greeting = string.format(text(2240 + place), evt.GetPlayerName(0))
        end
        evt.SimpleMessage(greeting .. "\n" .. text(2246 + place) .. "\n" .. text(2250 + hint))
        refreshFriendTopic(friend)
        refreshChaosTopics()
    end, function() return friendAvailable(friend) end)
end

local function requestCapture()
    if not canRequestCapture() then return end
    if flag("GotFQHint3") and answeredCount() >= 5 then
        if captureOffered then
            -- Commit only after the real party follower exists. Capture itself grants no experience.
            if evt.AddFollowerNpc(chaosNpc, 0, 0) then
                write("CoughtChaos", 1) -- Original spelling is authoritative.
                evt.MoveNPC(chaosNpc, 0)
            end
        else
            captureOffered = true
            evt.SetNPCName(chaosNpc, "")
            evt.SetNPCName(chaosNpc, evt.NPCName(chaosNpc))
            evt.SimpleMessage(text(2262))
        end
    else
        local refusal = evt.RandomBetween(1, 3)
        if refusal == 1 then evt.CloseNPCDialogue() end
        evt.SimpleMessage(text(2254 + refusal))
    end
    refreshChaosTopics()
end
for _, topic in ipairs({1794, 1792}) do
    RegisterNpcScopedTopic(chaosNpc, topic, function()
        if topic == (captureOffered and 1792 or 1794) then requestCapture() end
    end, function()
        -- The actual NPC slot selects 1794/1792. Readonly UI queries use a separate Lua session,
        -- so their visibility must follow the original flag gate, not its private transient text state.
        return canRequestCapture()
    end)
end

RegisterNpcScopedTopic(chaosNpc, 1790, function(_, step)
    if not active() then return end
    if step ~= nil then
        local id = number("PendingRiddle")
        if id < 1 or id > 11 or flag("RiddleSolved." .. id) then return end
        if step ~= 1 and step ~= 2 then error("Invalid Runaway Chaos answer continuation") end
        local count = answeredCount()
        write("PendingRiddle", 0)
        local inHouse = evt.IsHouseNPCDialogue()
        if inHouse then evt.MoveNPC(chaosNpc, 0) end
        if step == 2 then
            write("RiddleSolved." .. id, 1)
            count = count + 1
            write("RiddlesAnswered", count)
        end
        if count <= 5 or not flag("GotFQHint3") then
            if inHouse then evt.CloseNPCDialogue() end
            refreshAppearance()
        end
        evt.SimpleMessage(text(step == 2 and 2264 or 2265))
        refreshChaosTopics()
        return
    end
    if not canAskRiddle() then return end
    local remaining = {}
    for id = 1, 11 do
        if not flag("RiddleSolved." .. id) then table.insert(remaining, id) end
    end
    if #remaining == 0 then
        evt.SimpleMessage(text(2263))
        return
    end
    local id = remaining[evt.RandomBetween(1, #remaining)]
    write("PendingRiddle", id)
    local question = text(2257 + evt.RandomBetween(1, 4)) .. "\n\n" .. text(2216 + id * 2)
    evt.AskQuestionWithAnswerSteps(1790, 1, question, {{Answer = text(2217 + id * 2), Step = 2}}, false)
end, canAskRiddle)

RegisterGlobalAfterLoadEvent(65520, nil, function()
    -- UI prompts are deliberately absent from native saves; discard their stale continuation on cold load.
    if not evt.HasPendingInputPrompt() and number("PendingRiddle") ~= 0 then write("PendingRiddle", 0) end
    captureOffered = false
    if active() then refreshAppearance() end
    refreshChaosTopics()
    for friend = 772, 775 do refreshFriendTopic(friend) end
end)
RegisterGlobalNpcEnterHook(65521, nil, function(context)
    if context.npcId == chaosNpc then
        -- Identity is reconstructed on committed load, not every house entry: BrAlvar may disguise Chaos.
        refreshChaosTopics()
    elseif context.npcId >= 772 and context.npcId <= 775 then
        refreshFriendTopic(context.npcId)
    end
end)
RegisterGlobalOnLeaveEvent(65522, nil, function()
    -- An abandoned UI prompt cannot answer a different map's next question.
    if number("PendingRiddle") ~= 0 then write("PendingRiddle", 0) end
    captureOffered = false
end)
