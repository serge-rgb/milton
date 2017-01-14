module(..., package.seeall)

local util      = require "tundra.util"
local buildfile = require "tundra.buildfile"
local decl      = require "tundra.decl"
local nodegen   = require "tundra.nodegen"

function match_build_id(id, default)
  assert(id)
  local i = id:gmatch("[^-]+")
  local platform_name, toolset, variant, subvariant = i() or default, i() or default, i() or default, i() or default
  return platform_name, toolset, variant, subvariant
end

local function iter_inherits(config, name)
  local tab = config
  return function()
    while tab do
      local my_tab = tab
      if not my_tab then break end
      tab = my_tab.Inherit
      local v = my_tab[name]
      if v then return v end
    end
  end
end

function load_toolset(id, ...)
  -- For non-qualified packages, use a default package
  if not id:find("%.") then
    id = "tundra.tools." .. id
  end

  local pkg, err = require(id)

  if err then
    errorf("couldn't load extension module %s: %s", id, err)
  end
  
  pkg.apply(...)
end

local function setup_env(env, build_data, tuple, build_id)
  local config = tuple.Config
  local variant_name = tuple.Variant.Name

  if not build_id then
    build_id = config.Name .. "-" .. variant_name .. "-" .. tuple.SubVariant
  end

  local naked_platform, naked_toolset = match_build_id(build_id)

  env:set("CURRENT_PLATFORM", naked_platform) -- e.g. linux or macosx
  env:set("CURRENT_TOOLSET", naked_toolset) -- e.g. gcc or msvc
  env:set("CURRENT_VARIANT", tuple.Variant.Name) -- e.g. debug or release
  env:set("BUILD_ID", build_id) -- e.g. linux-gcc-debug
  env:set("OBJECTDIR", "$(OBJECTROOT)" .. SEP .. "$(BUILD_ID)")

  for tools in iter_inherits(config, "Tools") do
    for k, v in pairs(tools) do
      if type(k) == "string" then
        error("Tools must be a plain array - to include options keys wrap them in their own tables:\n " ..
              "e.g. Tools = { { 'foo'; Option = ... }, ... }.\n Your Tools:\n" .. util.tostring(tools))
      end
    end
    for _, data in ipairs(tools) do
      local id, options

      if type(data) == "table" then
        id = assert(data[1])
        options = data
        data = id
      end

      if type(data) == "string" then
        load_toolset(data, env, options)
      elseif type(data) == "function" then
        data(env, options)
      else
        error("bad parameters")
      end
    end
  end

  -- Incorporate matching values from the build data's Env and ReplaceEnv.
  if build_data.Env then 
    nodegen.append_filtered_env_vars(env, build_data.Env, build_id, false)
  end
  if build_data.ReplaceEnv then
    nodegen.replace_filtered_env_vars(env, build_data.ReplaceEnv, build_id, false)
  end

  -- Incorporate matching values from the config's Env and ReplaceEnv.
  for env_tab in iter_inherits(config, "Env") do
    nodegen.append_filtered_env_vars(env, env_tab, build_id, false)
  end
  for env_tab in iter_inherits(config, "ReplaceEnv") do
    nodegen.replace_filtered_env_vars(env, env_tab, build_id, false)
  end

  -- Run post-setup functions. This typically sets up implicit make functions.
  env:run_setup_functions()
  
  return env
end



local function setup_envs(tuple, configs, default_env, build_data)
  local result = {}

  local top_env = setup_env(default_env:clone(), build_data, tuple)
  result["__default"] = top_env

  -- Use the same build id for all subconfigurations
  local build_id = top_env:get("BUILD_ID")

  local cfg = configs[tuple.Config.Name]
  for moniker, x in util.nil_pairs(cfg.SubConfigs) do
    if result[x] then
      croak("duplicate subconfig name: %s", x)
    end
    local sub_tuple = { Config = configs[x], Variant = tuple.Variant, SubVariant = tuple.SubVariant }
    if not sub_tuple.Config then
      errorf("%s: no such config (in SubConfigs specification)", x)
    end
    local sub_env = setup_env(default_env:clone(), build_data, sub_tuple, build_id)
    result[moniker] = sub_env
  end
  return result
end


function parse_units(build_tuples, args, passes)
  if args.SyntaxExtensions then
    print("*WARNING* SyntaxExtensions has been deprecated. Use require instead.")
  end
  for _, id in util.nil_ipairs(args.SyntaxExtensions) do
    require(id)
  end

  local function chunk ()
    local raw_nodes, default_nodes, always_nodes = decl.parse(args.Units or "units.lua")
    assert(#default_nodes > 0 or #always_nodes > 0, "no default unit name to build was set")
    return { raw_nodes, default_nodes, always_nodes }
  end

  local success, result = xpcall(chunk, buildfile.syntax_error_catcher)

  if success then
    return result[1], result[2], result[3]
  else
    print("Build script execution failed")
    croak("%s", result or "")
  end
end


-- Inputs
--   build_tuples - the config/variant/subvariant pairs to include in the DAG
--   args - Raw data from Build() call
--   passes - Passes specified in Build() call
--   configs - Configs specified in Build() call
function generate_dag(build_tuples, args, passes, configs, default_env)
  local raw_nodes, default_nodes, always_nodes = parse_units(build_tuples, args, passes)

  local results = {}

  -- Let the nodegen code generate DAG nodes for all active
  -- configurations/variants.
  for _, tuple in pairs(build_tuples) do
    printf("Generating DAG for %s-%s-%s", tuple.Config.Name, tuple.Variant.Name, tuple.SubVariant)
    local envs = setup_envs(tuple, configs, default_env, args)
    local always_nodes, default_nodes, named_nodes = nodegen.generate_dag {
      Envs         = envs,
      Config       = tuple.Config,
      Variant      = tuple.Variant,
      Declarations = raw_nodes,
      DefaultNodes = default_nodes,
      AlwaysNodes  = always_nodes,
      Passes       = passes,
    }

    results[#results + 1] = {
      Config       = assert(tuple.Config),
      Variant      = assert(tuple.Variant),
      SubVariant   = assert(tuple.SubVariant),
      AlwaysNodes  = always_nodes,
      DefaultNodes = default_nodes,
      NamedNodes   = named_nodes,
    }
  end

  return raw_nodes, results
end


