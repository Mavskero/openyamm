-- OpenYAMM translation of the audited MMerge Verdant introduction only.
-- Dialogue comes from the user's imported NPC tables. Encounter triggers, hints,
-- continent rewards and the final quest are outside this profile's supported scope.
local prefix = "MMerge.CrossContinents."
local npcId = 803
local introSlot = 1

local function get(name)
    return evt.GetGlobalVar(prefix .. name, 0) ~= 0
end

local function set(name, value)
    evt.SetGlobalVar(prefix .. name, value and 1 or 0)
end

local function introTopic()
    if get("GotMainQuest") then
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
    evt.SetNPCTopic(npcId, introSlot, introTopic())
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
        textId = 2152
        AddValue(Experience, 0)
        evt.SetNPCGreeting(npcId, 328)
    end
    evt.SimpleMessage(evt.NPCText(textId))
    evt.SetNPCTopic(npcId, introSlot, introTopic())
end

for _, topicId in ipairs({1778, 1779, 1780}) do
    ReplaceGlobalEvent(topicId, nil, advanceIntroduction)
    RegisterCanShowTopic(topicId, function()
        return introTopic() == topicId
    end)
end
