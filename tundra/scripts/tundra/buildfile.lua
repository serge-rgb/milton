module(..., package.seeall)

local util   = require "tundra.util"
local native = require "tundra.native"

local function mk_defvariant(name)
  return { Name = name; Options = {} }
end

local default_variants = {
  mk_defvariant "debug",
  mk_defvariant "production",
  mk_defvariant "release"
}

local default_subvariants = {
  "default"
}

local _config_class = {}

-- Table constructor to make tundra.lua syntax a bit nicer in the Configs array
function _G.Config(args)
  local name = args.Name
  if not name then
    error("no `Name' specified for configuration")
  end
  if not name:match("^[%w_]+-[%w_]+$") then
    errorf("configuration name %s doesn't follow <platform>-<toolset> pattern", name)
  end

  if args.SubConfigs then
    if not args.DefaultSubConfig then
      errorf("configuration %s has `SubConfigs' but no `DefaultSubConfig'", name)
    end
  end

  return setmetatable(args, _config_class)
end

local function analyze_targets(configs, variants, subvariants)
  local build_tuples = {}

  local build_configs = {}
  local build_variants = {}
  local build_subvariants = {}

  for _, cfg in pairs(configs) do

    if not cfg.Virtual then -- skip virtual configs

      if not cfg.SupportedHosts then
        if cfg.DefaultOnHost then
          if type(cfg.DefaultOnHost) == "table" then
            cfg.SupportedHosts = cfg.DefaultOnHost
          else
            cfg.SupportedHosts = { cfg.DefaultOnHost }
          end
        else
          printf("1.0-compat: config %s doesn't specify SupportedHosts -- will never be built", cfg.Name);
          cfg.SupportedHosts = { }
        end
      end

      local lut = util.make_lookup_table(cfg.SupportedHosts)
      if lut[native.host_platform] then
        build_configs[#build_configs + 1] = cfg
      end
    end
  end
  for _, var in pairs(variants) do build_variants[#build_variants + 1] = var end
  for var, _ in pairs(subvariants) do build_subvariants[#build_subvariants + 1] = var end

  for _, config in ipairs(build_configs) do
    if config.Virtual then
      croak("can't build configuration %s directly; it is a support configuration only", config.Name)
    end
    for _, variant in ipairs(build_variants) do
      for _, subvariant in ipairs(build_subvariants) do
        build_tuples[#build_tuples + 1] = { Config = config, Variant = variant, SubVariant = subvariant }
      end
    end
  end

  if #build_tuples == 0 then
    errorf("no build tuples available\n")
  end

  return build_tuples
end

-- Custom pcall error handler to scan for syntax errors (thrown as tables) and
-- report them without a backtrace, trying to get the filename and line number
-- right so the user can fix their build file.
function syntax_error_catcher(err_obj)
  if type(err_obj) == "table" and err_obj.Class and err_obj.Message then
    local i = 1
    -- Walk down the stack until we find a function that isn't sourced from
    -- a file. These have 'source' names that don't start with an @ sign.
    -- Because we read all files into memory before executing them, this
    -- will give us the source filename of the user script.
    while true do
      local info = debug.getinfo(i, 'Sl')
            --print(util.tostring(info))
      if not info then
        break
      end
      if info.what == "C" or (info.source:sub(1, 1) == "@" and info.source ~= "@units.lua") then
        i = i + 1
      else
                local fn = info.source
                if info.source:sub(1, 1) == "@" then
                    fn = info.source:sub(2)
                end
                if info.currentline == -1 then
                    return string.format("%s: %s", err_obj.Class, err_obj.Message)
                else
                    return string.format("%s(%d): %s: %s", fn, info.currentline, err_obj.Class, err_obj.Message)
                end
      end
    end
        return string.format("%s: %s", err_obj.Class, err_obj.Message)
  else
    return debug.traceback(err_obj, 2)
  end
end



-- A place to store the result of the user's build script calling Build()
local build_result = nil

-- The Build function is the main entry point for "tundra.lua" when invoked.
function _G.Build(args)
  if type(args.Configs) ~= "table" or #args.Configs == 0 then
    croak("Need at least one config; got %s", util.tostring(args.Configs or "none at all"))
  end

  local configs, variants, subvariants = {}, {}, {}

  -- Legacy support: run "Config" constructor automatically on naked tables
  -- passed in Configs array.
  for idx = 1, #args.Configs do
    local cfg = args.Configs[idx]
    if getmetatable(cfg) ~= _config_class then
      cfg = Config(cfg)
      args.Configs[idx] = cfg
    end
    configs[cfg.Name] = cfg
  end

  for _, dir in util.nil_ipairs(args.ScriptDirs) do
    -- Make sure dir is sane and ends with a slash
    dir = dir:gsub("[/\\]", SEP):gsub("[/\\]$", "")
    local expr = dir .. SEP .. "?.lua"

    -- Add user toolset dir first so they can override builtin scripts.
    package.path = expr .. ";" .. package.path
  end

  if args.Variants then
    for i, x in ipairs(args.Variants) do
      if type(x) == "string" then
        args.Variants[i] = mk_defvariant(x)
      else
        assert(x.Name)
        if not x.Options then
          x.Options = {}
        end
      end
    end
  end

  local variant_array = args.Variants or default_variants
  for _, variant in ipairs(variant_array) do variants[variant.Name] = variant end

  local subvariant_array = args.SubVariants or default_subvariants
  for _, subvariant in ipairs(subvariant_array) do subvariants[subvariant] = true end

  local default_variant = variant_array[1]
  if args.DefaultVariant then
    for _, x in ipairs(variant_array) do
      if x.Name == args.DefaultVariant then
        default_variant = x
      end
    end
  end

  local default_subvariant = args.DefaultSubVariant or subvariant_array[1]
  local build_tuples = analyze_targets(configs, variants, subvariants)
  local passes = args.Passes or { Default = { Name = "Default", BuildOrder = 1 } }

  printf("%d valid build tuples", #build_tuples)

  -- Validate pass data
  for id, data in pairs(passes) do
    if not data.Name then
      croak("Pass %s has no Name attribute", id)
    elseif not data.BuildOrder then
      croak("Pass %s has no BuildOrder attribute", id)
    end
  end

  -- Assume syntax for C and DotNet is always needed
  -- for now. Could possible make an option for which generator sets to load
  -- in the future.
  require "tundra.syntax.native"
  require "tundra.syntax.dotnet"

  build_result = {
    BuildTuples             = build_tuples,
    BuildData               = args,
    Passes                  = passes,
    Configs                 = configs,
    DefaultVariant          = default_variant,
    DefaultSubVariant       = default_subvariant,
    ContentDigestExtensions = args.ContentDigestExtensions,
    Options                 = args.Options,
  }
end

function run(build_script_fn)
  local f, err = io.open(build_script_fn, 'r')

  if not f then
    croak("%s", err)
  end

  local text = f:read("*all")
  f:close()

  local script_globals, script_globals_mt = {}, {}
  script_globals_mt.__index = _G
  setmetatable(script_globals, script_globals_mt)

  local chunk, error_msg = loadstring(text, build_script_fn)
  if not chunk then
    croak("%s", error_msg)
  end
  setfenv(chunk, script_globals)

  local success, result = xpcall(chunk, syntax_error_catcher)

  if not success then
    print("Build script execution failed")
    croak("%s", result or "")
  end

  local result = build_result
  build_result = nil
  return result
end

