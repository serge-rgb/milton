-- ispc.lua - Support for Intel SPMD Program Compiler

module(..., package.seeall)

local path     = require "tundra.path"

DefRule {
  Name = "ISPC",
  Command = "$(ISPCCOM)",

  Blueprint = {
    Source = { Required = true, Type = "string" },
  },

  Setup = function (env, data)
    local src = data.Source
    local base_name = path.drop_suffix(src) 
    local objFile = "$(OBJECTDIR)$(SEP)" .. base_name .. "__" .. path.get_extension(src):sub(2) .. "$(OBJECTSUFFIX)"
    local hFile = "$(OBJECTDIR)$(SEP)" .. base_name .. "_ispc.h"
    return {
      InputFiles = { src },
      OutputFiles = { objFile, hFile },
    }
  end,
}
