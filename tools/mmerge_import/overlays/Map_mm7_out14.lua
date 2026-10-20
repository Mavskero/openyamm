-- Source-verified Avlee west boundary route. This requires the production
-- MapBoundary hook before lookup of an ordinary table-backed destination.
RegisterMapBoundaryHook(65470, nil, function(context)
    if context.boundaryEdge ~= 3 then return end -- MapBoundaryEdge::West

    for _, member in ipairs(PartyMembers()) do
        evt.ForPlayer(member)
        if not HasItem(1406) then
            evt.SetHookBlocked(true)
            if IsQBitSet(QBit(642)) or IsQBitSet(QBit(643)) or IsQBitSet(QBit(783)) then
                evt.StatusText("You must all be wearing your wetsuits!")
            end
            return
        end
    end

    -- The source requires suits, not a quest bit or a water-surface predicate.
    evt.MoveToMap(20096, -16448, 2404, 1008, 0, 0, 0, 8, "7out15.odm")
end)
