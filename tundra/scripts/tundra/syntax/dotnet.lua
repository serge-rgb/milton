module(..., package.seeall)

local util     = require "tundra.util"
local nodegen  = require "tundra.nodegen"
local depgraph = require "tundra.depgraph"

local _csbase_mt = nodegen.create_eval_subclass {
  DeclToEnvMappings = {
    References = "CSLIBS",
    RefPaths = "CSLIBPATH",
  },
}

local _csexe_mt = nodegen.create_eval_subclass({
  Label = "CSharpExe $(@)",
  Suffix = "$(CSPROGSUFFIX)",
  Action = "$(CSCEXECOM)"
}, _csbase_mt)

local _cslib_mt = nodegen.create_eval_subclass({
  Label = "CSharpLib $(@)",
  Suffix = "$(CSLIBSUFFIX)",
  Action = "$(CSCLIBCOM)"
}, _csbase_mt)


local csSourceExts = { ".cs" }
local csResXExts = { ".resx" }

local function setup_refs_from_dependencies(env, dep_nodes, deps)
  local dll_exts = { env:interpolate("$(CSLIBSUFFIX)") }
  local refs = {}
  local parent_env = env:get_parent()
  for _, x in util.nil_ipairs(dep_nodes) do
    if x.Keyword == "CSharpLib" then
      local outputs = {}
      local dag = x:get_dag(parent_env)
      deps[#deps + 1] = dag
      dag:insert_output_files(refs, dll_exts)
    end
  end
  for _, r in ipairs(refs) do
    env:append("CSLIBS", r)
  end
end

local function setup_resources(generator, env, assembly_name, resx_files, pass)
  local result_files = {}
  local deps = {}
  local i = 1
  for _, resx in util.nil_ipairs(resx_files) do
    local basename = path.get_filename_base(resx)
    local result_file = string.format("$(OBJECTDIR)/_rescompile/%s.%s.resources", assembly_name, basename)
    result_files[i] = result_file
    deps[i] = depgraph.make_node {
      Env         = env,
      Pass        = pass,
      Label       = "resgen $(@)",
      Action      = "$(CSRESGEN)",
      InputFiles  = { resx },
      OutputFiles = { result_file },
    }
    env:append("CSRESOURCES", result_file)
    i = i + 1
  end
  return result_files, deps
end

function _csbase_mt:create_dag(env, data, deps)
  local sources = data.Sources
  local resources = data.Resources or {}
  for _, r in util.nil_ipairs(resources) do
    env:append("CSRESOURCES", r)
  end

  sources = util.merge_arrays_2(sources, resources)

  setup_refs_from_dependencies(env, data.Depends, deps)

  return depgraph.make_node {
    Env          = env,
    Pass         = data.Pass,
    Label        = self.Label,
    Action       = self.Action,
    InputFiles   = sources,
    OutputFiles  = { nodegen.get_target(data, self.Suffix, self.Prefix) },
    Dependencies = util.uniq(deps),
  }
end

do
  local csblueprint = {
    Name = {
      Required = true,
      Help = "Set output (base) filename",
      Type = "string",
    },
    Sources = {
      Required = true,
      Help = "List of source files",
      Type = "source_list",
      ExtensionKey = "DOTNET_SUFFIXES",
    },
    Resources = {
      Help = "List of resource files",
      Type = "source_list",
      ExtensionKey = "DOTNET_SUFFIXES_RESOURCE",
    },
    Target = {
      Help = "Override target location",
      Type = "string",
    },
  }

  nodegen.add_evaluator("CSharpExe", _csexe_mt, csblueprint)
  nodegen.add_evaluator("CSharpLib", _cslib_mt, csblueprint)
end
