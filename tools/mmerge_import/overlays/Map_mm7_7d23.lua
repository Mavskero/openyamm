-- Source-verified Lincoln return route and noncombat group visibility.
local originalExit = assert(evt.map[501], "MM7 Lincoln overlay requires exported exit501")
local exitTitle = evt.meta.map.title[501]
local exitHint = evt.meta.map.hint[501]

RegisterMapOnLoadEvent(65471, nil, function()
    evt.SetMonGroupBit(56, MonsterBits.Hostile, false)
    evt.SetMonGroupBit(56, MonsterBits.Invisible, true)
end)

ReplaceMapEvent(501, exitTitle, function()
    for _, member in ipairs(PartyMembers()) do
        evt.ForPlayer(member)
        if not HasItem(1406) then
            -- The exported event fails on the same existing unsuited member.
            -- Keep its imported STR20 message rather than embedding a translation.
            originalExit()
            return
        end
    end
    evt.MoveToMap(-7005, 7856, 225, 128, 0, 0, 0, 8, "7out15.odm")
end, exitHint)
SetMapContextAction(501, {kind = "leave_dungeon", source = "mmmerge_shoals", targetMap = "7out15.odm"})
