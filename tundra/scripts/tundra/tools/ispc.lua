module(..., package.seeall)

function apply(env, options)
  env:set_many {
    ["ISPC_SUFFIXES"] = { ".ispc", },
    ["ISPC"] = "ispc",
    ["ISPCOPTS"] = "",
    ["ISPCCOM"] = "$(ISPC) $(ISPCOPTS) -o $(@:[1]) -h $(@:[2]) $(<)",
  }
end

