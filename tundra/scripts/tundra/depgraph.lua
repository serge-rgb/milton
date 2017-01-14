module(..., package.seeall)

local boot        = require "tundra.boot"
local util        = require "tundra.util"
local path        = require "tundra.path"
local native      = require "tundra.native"
local environment = require "tundra.environment"

local default_pass = { Name = "Default", BuildOrder = 100000 }

local all_nodes = {}

local _node_mt = {}
_node_mt.__index = _node_mt

function make_node(data_)
  local env_ = data_.Env
  assert(environment.is_environment(env_), "Env must be provided")

  local root_path = native.getcwd() .. env_:get('SEP')

  local function path_for_cmdline(p)
    local full_path
    if path.is_absolute(p) then
      full_path = p
    else
      full_path = root_path .. p
    end
    if full_path:find(' ', 1, true) then
      return '"' .. full_path .. '"'
    else
      return full_path
    end
  end

  local function normalize_paths(paths)
    return util.mapnil(paths, function (x)
      if type(x) == "string" then
        local v = env_:interpolate(x)
        v = path.normalize(v)
        return v
      else
        return x
      end
    end)
  end

  -- these are the inputs that $(<) expand to
  local regular_inputs    = normalize_paths(data_.InputFiles)

  -- these are other, auxillary input files that shouldn't appear on the command line
  -- useful to e.g. add an input dependency on a tool
  local implicit_inputs = normalize_paths(data_.ImplicitInputs)

  local inputs = util.merge_arrays_2(regular_inputs, implicit_inputs)
  local outputs = normalize_paths(data_.OutputFiles)

  local inputs_sorted = inputs and util.clone_array(inputs) or {}
  local outputs_sorted = outputs and util.clone_array(outputs) or {}

  local cmdline_inputs = util.merge_arrays(regular_inputs, data_.InputFilesUntracked)

  table.sort(inputs_sorted)
  table.sort(outputs_sorted)
  
  -- Quote the paths before interpolation into the command line
  local expand_env = {
    ['<'] = util.mapnil(cmdline_inputs, path_for_cmdline),
    ['@'] = util.mapnil(outputs, path_for_cmdline),
  }

  local expand_env_pretty = {
    ['<'] = cmdline_inputs,
    ['@'] = outputs,
  }

  local overwrite = true
  if type(data_.OverwriteOutputs) ~= "nil" then
    overwrite = data_.OverwriteOutputs
  end

  if data_.Scanner and not data_.Scanner.Kind then
    errorf("Missing scanner kind")
  end

  -- make sure dependencies are unique
  local unique_deps = util.uniq(data_.Dependencies or {})

  local params = {
    pass              = data_.Pass or default_pass,
    scanner           = data_.Scanner,
    deps              = unique_deps,
    inputs            = inputs_sorted,
    outputs           = outputs_sorted,
    is_precious       = data_.Precious,
    expensive         = data_.Expensive,
    overwrite_outputs = overwrite,
    src_env           = env_,
    env               = env_.external_vars,
    aux_outputs       = util.mapnil(data_.AuxOutputFiles, function (x)
      local result = env_:interpolate(x, expand_env)
      return path.normalize(result)
    end),
  }

  if data_.Action then
    params.action = env_:interpolate(data_.Action, expand_env)
  else
    assert(0 == #params.outputs, "can't have output files without an action")
    params.action = ""
  end

  if data_.PreAction then
    params.preaction = env_:interpolate(data_.PreAction, expand_env)
  end

  params.annotation = env_:interpolate(data_.Label or "?", expand_env_pretty)

  local result = setmetatable(params, _node_mt)

  -- Stash node
  all_nodes[#all_nodes + 1] = result

  return result
end

function is_node(obj)
  return getmetatable(obj) == _node_mt
end

function _node_mt:insert_output_files(tab, exts)
  if exts then
    local lut = util.make_lookup_table(exts)
    for _, fn in ipairs(self.outputs) do
      local ext = path.get_extension(fn)
      if lut[ext] then
        tab[#tab + 1] = fn
      end
    end
  else
    for _, fn in ipairs(self.outputs) do
      tab[#tab + 1] = fn
    end
  end
end

function _node_mt:insert_deps(tab)
  for _, dep in util.nil_ipairs(self.deps) do
    tab[#tab + 1] = dep
  end
end

function get_all_nodes()
  return all_nodes
end
