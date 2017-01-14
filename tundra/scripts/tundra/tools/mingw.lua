module(..., package.seeall)

function apply(env, options)

  -- load the generic GCC toolset first
  tundra.unitgen.load_toolset("gcc", env)

  -- load support for win32 resource compilation
  tundra.unitgen.load_toolset("win32-rc", env)

  env:set_many {
    ["NATIVE_SUFFIXES"] = { ".c", ".cpp", ".cc", ".cxx", ".a", ".o", ".rc" },
    ["OBJECTSUFFIX"] = ".o",
    ["LIBPREFIX"] = "",
    ["LIBSUFFIX"] = ".a",
    ["W32RESSUFFIX"] = ".o",
    ["CPPDEFS"] = "_WIN32",
    ["_CPPDEFS"] = "$(CPPDEFS:p/D) $(CPPDEFS_$(CURRENT_VARIANT:u):p/D)",
    ["RC"] = "windres",
    ["RCOPTS"] = "",
    ["RCCOM"] = "$(RC) $(RCOPTS) --output=$(@:b) $(CPPPATH:b:p-I) --input=$(<:b)",
    ["SHLIBLINKSUFFIX"] = ".a",
  }
end
