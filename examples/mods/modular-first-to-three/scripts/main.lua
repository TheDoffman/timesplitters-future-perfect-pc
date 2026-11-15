-- Original SDK example. Imports are linked by pack --modules, never read from disk.
local settings = require("rules.settings")
local scores = require("rules.scores")

return {
    api = 1,
    on_start = function(event)
        assert(event.authoritative)
        local players = game.players()
        scores.reset(players)
        for _, player in ipairs(players) do
            game.set_score(player.id, 0)
        end
        game.message(settings.opening)
    end,
    on_player_death = function(event)
        if event.actor_id == 0 or event.actor_id == event.target_id then return end
        local value = scores.add(event.actor_id)
        game.set_score(event.actor_id, value)
        if value >= settings.target then
            game.message("We have a modular winner")
            game.end_match(event.actor_id)
        end
    end
}
