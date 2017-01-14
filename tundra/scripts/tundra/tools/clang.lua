module(..., package.seeall)

function apply(env, options)
  tundra.unitgen.load_toolset("gcc", env)

  env:set_many {
    ["CC"] = "clang",
    ["CXX"] = "clang++",
    ["LD"] = "clang",
  }
end
