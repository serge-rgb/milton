module(..., package.seeall)

init_tundra_lua = [====[
local CFiles = { ".c", ".h" }
Build {
  Configs = {
    Config {
      Name = "generic-gcc",
      DefaultOnHost = "linux",
      Tools = { "gcc" },
    },
    Config {
      Name = "macosx-gcc",
      DefaultOnHost = "macosx",
      Tools = { "gcc-osx" },
    },
    Config {
      Name = "win64-msvc",
      DefaultOnHost = "windows",
      Tools = { "msvc-vs2008"; TargetPlatform = "x64" },
    },
  },
  Units = function()
    require "tundra.syntax.glob"
    Program {
      Name = "a.out",
      Sources = { Glob { Dir = ".", Extensions = CFiles } },
    }

    Default "a.out"
  end,
}
]====]

