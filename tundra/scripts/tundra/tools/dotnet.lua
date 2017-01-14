module(..., package.seeall)

local frameworkDir = "c:\\Windows\\Microsoft.NET\\Framework"
local defaultFrameworkVersion = "v3.5"

function apply(env, options)
  tundra.unitgen.load_toolset("generic-dotnet", env)

  local version = options and assert(options.Version) or defaultFrameworkVersion
  env:set_external_env_var('FrameworkDir', frameworkDir)
  env:set_external_env_var('FrameworkVersion', version)

  local binPath = frameworkDir .. "\\" .. version
  env:set_external_env_var('PATH', binPath .. ";" .. env:get_external_env_var('PATH'))

  -- C# support
  env:set_many {
    ["DOTNET_SUFFIXES"] = { ".cs" },
    ["DOTNET_SUFFIXES_RESOURCE"] = { ".resource" },
    ["CSC"] = "csc.exe",
    ["CSPROGSUFFIX"] = ".exe",
    ["CSLIBSUFFIX"] = ".dll",
    ["CSRESGEN"] = "resgen $(<) $(@)",
    ["_CSC_COMMON"] = "-warn:$(CSC_WARNING_LEVEL) /nologo $(CSLIBPATH:b:p/lib\\:) $(CSRESOURCES:b:p/resource\\:) $(CSLIBS:p/reference\\::A.dll)",
    ["CSCLIBCOM"] = "$(CSC) $(_CSC_COMMON) $(CSCOPTS) -target:library -out:$(@:b) $(<:b)",
    ["CSCEXECOM"] = "$(CSC) $(_CSC_COMMON) $(CSCOPTS) -target:exe -out:$(@:b) $(<:b)",
  }
end
