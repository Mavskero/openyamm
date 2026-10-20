-- Source-verified Shoals surface exit. A native timer supplies the source Tick
-- predicate every thirty game seconds (at most about one real second late).
local lastBelowSurface = true

RegisterMapTimerEvent(65472, 30, function()
    local _, _, z = evt.GetPartyPosition(true)
    if z <= 3900 then
        lastBelowSurface = true
    elseif lastBelowSurface and evt.IsNormalGameplayScreen() then
        lastBelowSurface = false
        evt.MoveToMap(-18584, -16562, 1, 290, 0, 0, 0, 8, "out14.odm")
    end
end)
