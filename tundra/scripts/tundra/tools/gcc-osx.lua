module(..., package.seeall)

function apply(env, options)
  -- load the generic GCC toolset first
  tundra.unitgen.load_toolset("gcc", env)

  env:set_many {
    ["NATIVE_SUFFIXES"] = { ".c", ".cpp", ".cc", ".cxx", ".m", ".mm", ".a", ".o" },
    ["CXXEXTS"] = { "cpp", "cxx", "cc", "mm" },
    ["FRAMEWORKS"] = "",
    ["FRAMEWORKPATH"] = {},
    ["SHLIBPREFIX"] = "lib",
    ["SHLIBOPTS"] = "-shared",
    ["_OS_CCOPTS"] = "$(FRAMEWORKPATH:p-F)",
    ["_OS_CXXOPTS"] = "$(FRAMEWORKPATH:p-F)",
    ["SHLIBCOM"] = "$(LD) $(SHLIBOPTS) $(LIBPATH:p-L) $(LIBS:p-l) $(FRAMEWORKPATH:p-F) $(FRAMEWORKS:p-framework ) -o $(@) $(<)",
    ["PROGCOM"] = "$(LD) $(PROGOPTS) $(LIBPATH:p-L) $(LIBS:p-l) $(FRAMEWORKPATH:p-F)  $(FRAMEWORKS:p-framework ) -o $(@) $(<)",
    ["OBJCCOM"] = "$(CCCOM)", -- objc uses same commandline
    ["NIBCC"] = "ibtool --output-format binary1 --compile $(@) $(<)",
  }
end
