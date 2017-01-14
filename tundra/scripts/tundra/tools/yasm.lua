module(..., package.seeall)

function apply(env, options)
  -- load the generic assembly toolset first
  tundra.unitgen.load_toolset("generic-asm", env)

  env:set_many {
    ["YASM"] = "yasm",
    ["ASMCOM"] = "$(YASM) -o $(@) $(ASMDEFS:p-D ) $(ASMOPTS) $(<)",
    ["ASMINC_KEYWORDS"] = { "%include" },
  }
end
