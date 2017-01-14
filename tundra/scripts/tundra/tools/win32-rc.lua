module(..., package.seeall)

local path = require("tundra.path")
local depgraph = require("tundra.depgraph")
local gencpp = require("tundra.tools.generic-cpp")

local function compile_resource_file(env, pass, fn)
  return depgraph.make_node {
    Env = env,
    Label = 'Rc $(@)',
    Pass = pass,
    Action = "$(RCCOM)",
    InputFiles = { fn },
    OutputFiles = { path.make_object_filename(env, fn, env:get('W32RESSUFFIX')) },
    Scanner = gencpp.get_cpp_scanner(env, fn),
  }
end

function apply(env, options)
  env:register_implicit_make_fn("rc", compile_resource_file)
end
