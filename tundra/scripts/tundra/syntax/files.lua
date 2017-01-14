module(..., package.seeall)

local decl     = require "tundra.decl"
local depgraph = require "tundra.depgraph"

local common_blueprint = {
  Source = {
    Required = true,
    Help = "Source filename",
    Type = "string",
  },
  Target = {
    Required = true,
    Help = "Target filename",
    Type = "string",
  },
}

local function def_copy_rule(name, command, cfg_invariant)
  DefRule {
    Name            = name,
    ConfigInvariant = cfg_invariant,
    Blueprint       = common_blueprint,
    Command         = command,
    Setup           = function (env, data)
      return {
        InputFiles = { data.Source },
        OutputFiles = { data.Target },
      }
    end,
  }
end

def_copy_rule('CopyFile', '$(_COPY_FILE)')
def_copy_rule('CopyFileInvariant', '$(_COPY_FILE)', true)
def_copy_rule('HardLinkFile', '$(_HARDLINK_FILE)')
def_copy_rule('HardLinkFileInvariant', '$(_HARDLINK_FILE)', true)


function hardlink_file(env, src, dst, pass, deps)
  return depgraph.make_node {
    Env = env,
    Annotation = "HardLink $(<)",
    Action = "$(_HARDLINK_FILE)",
    InputFiles = { src },
    OutputFiles = { dst },
    Dependencies = deps,
    Pass = pass,
  }
end

function copy_file(env, src, dst, pass, deps)
  return depgraph.make_node {
    Env = env,
    Annotation = "CopyFile $(<)",
    Action = "$(_COPY_FILE)",
    InputFiles = { src },
    OutputFiles = { dst },
    Dependencies = deps,
    Pass = pass,
  }
end


