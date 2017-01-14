-- lemon.lua - Support for the Lemon parser generator

module(..., package.seeall)

local path     = require "tundra.path"

DefRule {
  Name = "Lemon",
  Command = "lemon $(<)",
  ConfigInvariant = true,

  Blueprint = {
    Source = { Required = true, Type = "string" },
  },

  Setup = function (env, data)
    local src = data.Source
    local base_name = path.drop_suffix(src)
    local gen_c = base_name .. '.c'
    local gen_h = base_name .. '.h'
    local gen_out = base_name .. '.out'
    return {
      InputFiles = { src },
      OutputFiles = { gen_c, gen_h, gen_out },
    }
  end,
}
