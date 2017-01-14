module(..., package.seeall)

-- Use "strict" when developing to flag accesses to nil global variables
-- This has very low perf impact (<0.1%), so always leave it on.
require "strict"

local os        = require "os"
local platform  = require "tundra.platform"
local util      = require "tundra.util"
local depgraph  = require "tundra.depgraph"
local unitgen   = require "tundra.unitgen"
local buildfile = require "tundra.buildfile"
local native    = require "tundra.native"

-- This trio is so useful we want them everywhere without imports.
function _G.printf(msg, ...)
  local str = string.format(msg, ...)
  print(str)
end

function _G.errorf(msg, ...)
  local str = string.format(msg, ...)
  error(str)
end

function _G.croak(msg, ...)
  local str = string.format(msg, ...)
  io.stderr:write(str, "\n")
  os.exit(1)
end

-- Expose benchmarking function for those times everything sucks
--
-- Wrap a function so that it prints execution times.
--
-- Usage:
--   foo = bench("foo", foo) -- benchmark function foo
function _G.bench(name, fn) 
  return function (...)
    local t1 = native.get_timer()
    local result = { fn(...) }
    local t2 = native.get_timer()
    printf("%s: %ss", name, native.timerdiff(t1, t2))
    return unpack(result)
  end
end

local environment = require "tundra.environment"
local nodegen     = require "tundra.nodegen"
local decl        = require "tundra.decl"
local path        = require "tundra.path"
local depgraph    = require "tundra.depgraph"
local dagsave     = require "tundra.dagsave"

_G.SEP = platform.host_platform() == "windows" and "\\" or "/"

_G.Options = {
  FullPaths = 1
}

local function make_default_env(build_data, add_unfiltered_vars)
  local default_env = environment.create()

  default_env:set_many {
    ["OBJECTROOT"] = "t2-output",
    ["SEP"] = SEP,
  }

  local host_platform = platform.host_platform()
  do
    local mod_name = "tundra.host." .. host_platform
    local mod = require(mod_name)
    mod.apply_host(default_env)
  end

  -- Add any unfiltered entries from the build data's Env and ReplaceEnv to the 
  -- default environment. For config environments, this will be false, because we
  -- want to wait until the config's tools have run before adding any user
  -- customizations.
  if add_unfiltered_vars then
    if build_data.Env then 
      nodegen.append_filtered_env_vars(default_env, build_data.Env, nil, true)
    end
    if build_data.ReplaceEnv then
      nodegen.replace_filtered_env_vars(default_env, build_data.ReplaceEnv, nil, true)
    end
  end

  return default_env
end

function generate_dag_data(build_script_fn)
  local build_data = buildfile.run(build_script_fn)
  local env = make_default_env(build_data.BuildData, false)
  local raw_nodes, node_bindings = unitgen.generate_dag(
    build_data.BuildTuples,
    build_data.BuildData,
    build_data.Passes,
    build_data.Configs,
    env)

  dagsave.save_dag_data(
    node_bindings,
    build_data.DefaultVariant,
    build_data.DefaultSubVariant,
    build_data.ContentDigestExtensions,
    build_data.Options)
end

function generate_ide_files(build_script_fn, ide_script)
  -- We are generating IDE integration files. Load the specified
  -- integration module rather than DAG builders.
  --
  -- Also, default to using full paths for all commands to aid with locating
  -- sources better.
  Options.FullPaths = 1

  local build_data = buildfile.run(build_script_fn)
  local build_tuples = assert(build_data.BuildTuples)
  local raw_data     = assert(build_data.BuildData)
  local passes       = assert(build_data.Passes)

  local env = make_default_env(raw_data, true)

  if not ide_script:find('.', 1, true) then
    ide_script = 'tundra.ide.' .. ide_script
  end

  require(ide_script)

  -- Generate dag
  local raw_nodes, node_bindings = unitgen.generate_dag(
    build_data.BuildTuples,
    build_data.BuildData,
    build_data.Passes,
    build_data.Configs,
    env)

  -- Pass the build tuples directly to the generator and let it write
  -- files.
  nodegen.generate_ide_files(build_tuples, build_data.DefaultNodes, raw_nodes, env, raw_data.IdeGenerationHints, ide_script)
end
