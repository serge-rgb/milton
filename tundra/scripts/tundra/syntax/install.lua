-- install.lua - Express file copying in unit form.

module(..., package.seeall)

local nodegen  = require "tundra.nodegen"
local files    = require "tundra.syntax.files"
local path     = require "tundra.path"
local util     = require "tundra.util"
local depgraph = require "tundra.depgraph"

local _mt = nodegen.create_eval_subclass {}

local blueprint = {
  Sources = { Type = "source_list", Required = true },
  TargetDir = { Type = "string", Required = true },
}

function _mt:create_dag(env, data, deps)
  local my_pass = data.Pass
  local sources = data.Sources
  local target_dir = data.TargetDir

  local copies = {}

  -- all the copy operations will depend on all the incoming deps
  for _, src in util.nil_ipairs(sources) do
    local base_fn = select(2, path.split(src))
    local target = target_dir .. '/' .. base_fn
    copies[#copies + 1] = files.copy_file(env, src, target, my_pass, deps)
  end

  return depgraph.make_node {
    Env          = env,
    Label        = "Install group for " .. decl.Name,
    Pass         = my_pass,
    Dependencies = copies
  }
end

nodegen.add_evaluator("Install", _mt, blueprint)
