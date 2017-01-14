-- alias.lua -- support for named aliases in the DAG

module(..., package.seeall)

local nodegen  = require "tundra.nodegen"
local depgraph = require "tundra.depgraph"
local util     = require "tundra.util"

local _alias_mt = nodegen.create_eval_subclass {}

function _alias_mt:create_dag(env, data, input_deps)

  local deps = util.clone_table(input_deps)

  for _, dep in util.nil_ipairs(data.Depends) do
    deps[#deps+1] = dep:get_dag(env:get_parent())
  end

  local dag = depgraph.make_node {
    Env                 = env,
    Label               = "Named alias " .. data.Name .. " for " .. env:get('BUILD_ID'),
    Pass                = data.Pass,
    Dependencies        = deps,
  }

  -- Remember this dag node for IDE file generation purposes
  data.__DagNode = dag

  return dag
end

local alias_blueprint = {
  Name = {
    Required = true,
    Help = "Set alias name",
    Type = "string",
  },
}

nodegen.add_evaluator("Alias", _alias_mt, alias_blueprint)
