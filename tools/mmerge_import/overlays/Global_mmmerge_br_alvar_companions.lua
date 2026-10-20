-- Append after Global_mmmerge_runaway_chaos.lua, in the global program.
-- Source Hint 1793 remains owned by that overlay. Session alone commits Join 1789.
-- Availability checks receipt/custody and real scene context, including outside dialogue.
-- The return request separately validates the current NPC/house/actor interaction.
for _, npcId in ipairs({772, 773, 774, 775}) do
    RegisterNpcScopedTopic(npcId, 1789, function()
        if evt.IsBreachAlvarCompanionAvailable(npcId) then
            evt.RequestBreachAlvarCompanionReturn(npcId)
        end
    end, function()
        return evt.IsBreachAlvarCompanionAvailable(npcId)
    end)
end
