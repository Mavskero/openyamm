-- Native stability repair for the SHA-verified original MM8 Global.evt reward branches.
-- Between these checks and the last grant there is no yield or nested item-producing callback.
-- This is a capacity query for these finite handlers, not a general transaction or reservation.
local function guardReward(eventId, count, willGrant)
    local original = assert(evt.global[eventId], "Missing source quest reward handler")
    -- Keep all metadata, CanShowTopic predicates and the original handler's _BeginEvent behavior.
    evt.global[eventId] = function(continueStep)
        if willGrant(continueStep) and not evt.CanQueueItemDeliveries(count) then
            evt._BeginEvent(eventId)
            evt.SimpleMessage(capacityMessage)
            return
        end
        return original(continueStep)
    end
end

guardReward(29, 1, function()
    return not evt.HasQuestBit(12) and not evt.HasQuestBit(24)
        and not evt.HasQuestBit(10) and not evt.HasQuestBit(9)
end)

local otherAnswers = {
    [164] = {95, 96},
    [165] = {94, 96},
    [166] = {94, 95},
}
for eventId = 164, 166 do
    local required = otherAnswers[eventId]
    guardReward(eventId, 1, function(continueStep)
        return continueStep == 4 and evt.HasQuestBit(required[1]) and evt.HasQuestBit(required[2])
    end)
end

guardReward(175, 3, function() return true end)
guardReward(188, 1, function() return true end)
