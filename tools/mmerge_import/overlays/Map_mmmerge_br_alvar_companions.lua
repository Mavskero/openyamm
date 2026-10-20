-- Source-verified five-member BrAlvar split request, appended to its controlled map program.
-- Session owns admission, real actor preparation, identity receipt and atomic publication.
-- A queued request is not success. Rejection has no Lua fallback or state mutation.
RegisterMapOnLoadEvent(65469, nil, function()
    evt.RequestBreachAlvarSplit()
end)

-- BrAlvar.lua 237–255. The shared service importer supplies the source Name/Picture;
-- normal house entry owns resident discovery, dialogue, Hint and the existing Join request.
ReplaceMapEvent(50, nil, function()
    evt.EnterHouse(705)
end)
ReplaceMapEvent(51, nil, function()
    evt.EnterHouse(706)
end)
ReplaceMapEvent(52, nil, function()
    evt.EnterHouse(707)
end)

-- BrAlvar.lua 270–275 and 292–334. The App validates and commits the source door/capture transactions.
-- These requests carry the real local event identity; Lua never consumes RNG or edits Party membership.
ReplaceMapEvent(5, nil, function()
    evt.RequestBreachAlvarChaosAction()
end)
ReplaceMapEvent(81, nil, function()
    evt.RequestBreachAlvarChaosAction()
end)

-- Source mapvars.DoorKeys is empty on every entry and resets once per game hour.
RegisterMapOnLoadEvent(65460, nil, function()
    evt.ClearBreachAlvarChaosDoorKeys()
end)
RegisterMapTimerEvent(65461, 60 * 60, function()
    evt.ClearBreachAlvarChaosDoorKeys()
end)
