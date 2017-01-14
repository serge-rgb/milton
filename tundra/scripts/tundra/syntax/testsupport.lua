-- testsupport.lua: A simple UpperCaseFile unit used for Tundra's test harness

module(..., package.seeall)

local util = require 'tundra.util'
local nodegen = require 'tundra.nodegen'
local depgraph = require 'tundra.depgraph'

local mt = nodegen.create_eval_subclass {}

function mt:create_dag(env, data, deps)
  return depgraph.make_node {
    Env          = env,
    Pass         = data.Pass,
    Label        = "UpperCaseFile \$(@)",
    Action       = "tr a-z A-Z < \$(<) > \$(@)",
    InputFiles   = { data.InputFile },
    OutputFiles  = { data.OutputFile },
    Dependencies = deps,
  }
end

nodegen.add_evaluator("UpperCaseFile", mt, {
  Name = { Type = "string", Required = "true" },
  InputFile = { Type = "string", Required = "true" },
  OutputFile = { Type = "string", Required = "true" },
})


