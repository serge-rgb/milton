module(..., package.seeall)

function apply(env, options)
  tundra.unitgen.load_toolset("generic-dotnet", env)

  env:set_many {
    ["DOTNET_SUFFIXES"] = { ".cs" },
    ["DOTNET_SUFFIXES_RESOURCE"] = { ".resource" },
    ["CSC"] = "gmcs",
    ["CSPROGSUFFIX"] = ".exe",
    ["CSLIBSUFFIX"] = ".dll",
    ["CSRESGEN"] = "resgen2 $(<) $(@)",
    ["_CSC_COMMON"] = "-warn:$(CSC_WARNING_LEVEL) /nologo $(CSLIBPATH:p-lib\\:) $(CSRESOURCES:p-resource\\:) $(CSLIBS:p-reference\\::A.dll)",
    ["CSCLIBCOM"] = "$(CSC) $(_CSC_COMMON) $(CSCOPTS) -target:library -out:$(@) $(<)",
    ["CSCEXECOM"] = "$(CSC) $(_CSC_COMMON) $(CSCOPTS) -target:exe -out:$(@) $(<)",
  }
end
