local values = {}
return {
    reset = function(players)
        values = {}
        for _, player in ipairs(players) do values[player.id] = 0 end
    end,
    add = function(id)
        values[id] = (values[id] or 0) + 1
        return values[id]
    end
}
