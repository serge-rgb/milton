module(..., package.seeall)

local util = require "tundra.util"
local path = require "tundra.path"
local glob = require "tundra.syntax.glob"
local nodegen = require "tundra.nodegen"
local depgraph = require "tundra.depgraph"

local lua_exts = { ".lua" }
local luac_mt_ = nodegen.create_eval_subclass {}

local function luac(env, src, pass)
  local target = "$(OBJECTDIR)/" .. path.drop_suffix(src) .. ".luac" 
  return target, depgraph.make_node {
    Env            = env,
    Pass           = pass,
    Label          = "LuaC $(@)",
    Action         = "$(LUAC) -o $(@) -- $(<)",
    InputFiles     = { src },
    OutputFiles    = { target },
    ImplicitInputs = { "$(LUAC)" },
  }
end

function luac_mt_:create_dag(env, data, deps)
  local files = {}
  local deps = {}
  local inputs = {}
  local action_fragments = {}

  for _, base_dir in ipairs(data.Dirs) do
    local lua_files = glob.Glob { Dir = base_dir, Extensions = lua_exts }
    local dir_len = base_dir:len()
    for _, filename in pairs(lua_files) do
      local rel_name = filename:sub(dir_len+2)
      local pkg_name = rel_name:gsub("[/\\]", "."):gsub("%.lua$", "")
      inputs[#inputs + 1] = filename
      if env:get("LUA_EMBED_ASCII", "no") == "no" then
        files[#files + 1], deps[#deps + 1] = luac(env, filename, data.Pass)
      else
        files[#files + 1] = filename
      end
      action_fragments[#action_fragments + 1] = pkg_name
      action_fragments[#action_fragments + 1] = files[#files]
    end
  end

  return depgraph.make_node {
    Env = env,
    Label = "EmbedLuaSources $(@)",
    Pass = data.Pass,
    Action = "$(GEN_LUA_DATA) " .. table.concat(action_fragments, " ") .. " > $(@)",
    InputFiles = inputs,
    OutputFiles = { "$(OBJECTDIR)/" .. data.OutputFile },
    Dependencies = deps,
    ImplicitInputs = { "$(GEN_LUA_DATA)" },
  }
end

local blueprint = {
  Dirs = { Type = "table", Required = "true" },
  OutputFile = { Type = "string", Required = "true" },
}

nodegen.add_evaluator("EmbedLuaSources", luac_mt_, blueprint)
