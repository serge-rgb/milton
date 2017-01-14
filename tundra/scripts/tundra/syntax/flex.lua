-- flex.lua - Support for FLEX

module(..., package.seeall)

local path     = require "tundra.path"

DefRule {
  Name = "Flex",
  Command = "flex --outfile=$(@:[1]) --header-file=$(@:[2]) $(<)",
  ConfigInvariant = true,

  Blueprint = {
    Source           = { Required = true, Type = "string" },
    OutputCFile      = { Required = false, Type = "string" },
    OutputHeaderFile = { Required = false, Type = "string" },
    Pass             = { Required = true, Type = "pass", }
  },

  Setup = function (env, data)
    local src = data.Source
    local base_name = path.drop_suffix(src)
    local gen_c = data.OutputCFile or ('$(OBJECTROOT)$(SEP)flexgen_' .. base_name .. '.c')
    local gen_h = data.OutputHeaderFile or ('$(OBJECTROOT)$(SEP)flexgen_' .. base_name .. '.h')
    return {
      InputFiles = { src },
      OutputFiles = { gen_c, gen_h },
    }
  end,
}
