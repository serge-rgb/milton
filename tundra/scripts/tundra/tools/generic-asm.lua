module(..., package.seeall)

local path     = require "tundra.path"
local util     = require "tundra.util"
local boot     = require "tundra.boot"
local scanner  = require "tundra.scanner"
local depgraph = require "tundra.depgraph"

local default_keywords = { "include" }
local default_bin_keywords = { "incbin" }

local function get_asm_scanner(env, fn)
  local function test_bool(name, default)
    val = env:get(name, default)
    if val == "yes" or val == "true" or val == "1" then
      return 1
    else
      return 0
    end
  end
  local function new_scanner()
    local paths = util.map(env:get_list("ASMINCPATH"), function (v) return env:interpolate(v) end)
    local data = {
      Paths = paths,
      Keywords = env:get_list("ASMINC_KEYWORDS", default_keywords),
      KeywordsNoFollow = env:get_list("ASMINC_BINARY_KEYWORDS", default_bin_keywords),
      RequireWhitespace = test_bool("ASMINC_REQUIRE_WHITESPACE", "yes"),
      UseSeparators = test_bool("ASMINC_USE_SEPARATORS", "yes"),
      BareMeansSystem = test_bool("ASMINC_BARE_MEANS_SYSTEM", "no"),
    }
    return scanner.make_generic_scanner(data)
  end
  return env:memoize("ASMINCPATH", "_asm_scanner", new_scanner)
end

-- Register implicit make functions for assembly files.
-- These functions are called to transform source files in unit lists into
-- object files. This function is registered as a setup function so it will be
-- run after user modifications to the environment, but before nodes are
-- processed. This way users can override the extension lists.
local function generic_asm_setup(env)
  local _assemble = function(env, pass, fn)
    local object_fn = path.make_object_filename(env, fn, '$(OBJECTSUFFIX)')

    return depgraph.make_node {
      Env         = env,
      Label       = 'Asm $(@)',
      Pass        = pass,
      Action      = "$(ASMCOM)",
      InputFiles  = { fn },
      OutputFiles = { object_fn },
      Scanner     = get_asm_scanner(env, fn),
    }
  end

  for _, ext in ipairs(env:get_list("ASM_EXTS")) do
    env:register_implicit_make_fn(ext, _assemble)
  end
end

function apply(_outer_env, options)

  _outer_env:add_setup_function(generic_asm_setup)

  _outer_env:set_many {
    ["ASM_EXTS"] = { ".s", ".asm" },
    ["ASMINCPATH"] = {},
    ["ASMDEFS"] = "",
    ["ASMDEFS_DEBUG"] = "",
    ["ASMDEFS_PRODUCTION"] = "",
    ["ASMDEFS_RELEASE"] = "",
    ["ASMOPTS"] = "",
    ["ASMOPTS_DEBUG"] = "",
    ["ASMOPTS_PRODUCTION"] = "",
    ["ASMOPTS_RELEASE"] = "",
  }
end

