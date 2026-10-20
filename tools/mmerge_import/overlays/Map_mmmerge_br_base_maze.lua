-- Source: BrBase.lua events 1/2. The native lifecycle owns floor retention, locks and departure.
-- Queue intent only; App presents the real confirmation before committing a prepared transition.
ReplaceMapEvent(1, nil, function()
    evt.RequestBreachMazeTransition()
end)

ReplaceMapEvent(2, nil, function()
    evt.RequestBreachMazeTransition()
end)

-- Actual handlers keep mouse/keyboard picking enabled. The native scene callback executes
-- the source-grounded trap and bookcase rules; an unbound inspection scene must reject use.
ReplaceMapEvent(3, nil, function()
    error("BrBase trap requires a prepared native maze scene")
end)
ReplaceMapEvent(4, nil, function()
    error("BrBase bookcase requires a prepared native maze scene")
end, evt.strings[6])
