module(..., package.seeall)

local path = require "tundra.path"

DefRule {
  Name = "Bison",
  Command = "", -- Replaced on a per-instance basis.
  ConfigInvariant = true,

  Blueprint = {
    Source       = { Required = true, Type  = "string" },
    OutputFile   = { Required = false, Type = "string" },
    TokenDefines = { Required = false, Type = "boolean" },
    Pass         = { Required = true, Type = "pass" },
  },

  Setup = function (env, data)
    local src = data.Source
    local out_src
    if data.OutputFile then
      out_src = "$(OBJECTROOT)$(SEP)" .. data.OutputFile
    else
      local targetbase = "$(OBJECTROOT)$(SEP)bisongen_" .. path.get_filename_base(src)
      out_src = targetbase .. ".c"
    end
    local defopt = ""
    local outputs = { out_src }
    if data.TokenDefines then
      local out_hdr = path.drop_suffix(out_src) .. ".h"
      defopt = "--defines=" .. out_hdr
      outputs[#outputs + 1] = out_hdr
    end
    return {
      Pass = data.Pass,
      Label = "Bison $(@)",
      Command = "$(BISON) $(BISONOPT) " .. defopt .. " --output-file=$(@:[1]) $(<)",
      InputFiles = { src },
      OutputFiles = outputs,
    }
  end,
}
