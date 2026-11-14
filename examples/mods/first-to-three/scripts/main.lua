-- Original SDK example, not recovered game code.
-- The native game bridge must dispatch authoritative events exactly once.
local kills = {}
local target = 3

return {
    api = 1,
    on_start = function(event)
        assert(event.authoritative)
        for _, player in ipairs(game.players()) do
            kills[player.id] = 0
            game.set_score(player.id, 0)
        end
        game.message("First to three kills")
    end,
    on_player_death = function(event)
        if event.actor_id == 0 or event.actor_id == event.target_id then
            return
        end
        local id = event.actor_id
        kills[id] = (kills[id] or 0) + 1
        game.set_score(id, kills[id])
        if kills[id] >= target then
            game.message("We have a winner")
            game.end_match(id)
        end
    end,
    on_stop = function(event)
        game.message("Mode stopped")
    end
}
