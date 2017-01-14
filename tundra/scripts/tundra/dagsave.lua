module(..., package.seeall)

local depgraph = require "tundra.depgraph"
local util     = require "tundra.util"
local scanner  = require "tundra.scanner"
local dirwalk  = require "tundra.dirwalk"
local platform = require "tundra.platform"
local native   = require "tundra.native"
local njson    = require "tundra.native.json"
local path     = require "tundra.path"

local dag_dag_magic = 0x15890105

local function get_passes(nodes)
  local result = {}
  local seen_passes = {}

  for _, node in ipairs(nodes) do
    local p = node.pass
    if not seen_passes[p] then
      assert(type(p) == "table", "Passes must be tables, have " .. util.tostring(p))
      assert(type(p.BuildOrder) == "number", "Pass BuildOrder must be a number")
      result[#result + 1] = p
      seen_passes[p] = true
    end
  end

  table.sort(result, function (a, b) return a.BuildOrder < b.BuildOrder end)

  local pass_lookup = {}

  for index, pass in ipairs(result) do
    pass_lookup[pass] = index - 1
  end

  return result, pass_lookup
end

local function setup_input_deps(nodes)
  local producers = {}

  local cwd = native.getcwd() .. SEP
  local filter
  if native.host_platform == 'windows' or native.host_platform == 'macosx' then
    filter = function (str) return str:lower() end
  else
    filter = function (str) return str end
  end

  local node_deps = {}

  -- Record producing node for all output files
  for _, n in ipairs(nodes) do
    for _, output in util.nil_ipairs(n.outputs) do
      if not path.is_absolute(output) then
        output = cwd .. output
      end
      output = filter(output)
      if producers[output] then
        errorf("file %s set to be written by more than one target:\n%s\n%s\n",
          output, n.annotation, producers[output].annotation)
      end
      producers[output] = n
    end
    if n.deps then
      node_deps[n] = util.make_lookup_table(n.deps)
    end
  end

  -- Map input files to dependencies
  for _, n in ipairs(nodes) do
    for _, inputf in util.nil_ipairs(n.inputs) do
      if not path.is_absolute(inputf) then
        inputf = cwd .. inputf
      end
      inputf = filter(inputf)
      local producer = producers[inputf]
      local deps_lut = node_deps[n]
      if producer and (not deps_lut or not deps_lut[producer]) then
        n.deps[#n.deps + 1] = producer
        if not deps_lut then
          deps_lut = {}
          node_deps[n] = deps_lut
        end
        deps_lut[producer] = true
      end
    end
  end

end

local function get_scanners(nodes)
  local scanners = {}
  local scanner_to_index = {}
  for _, node in ipairs(nodes) do
    local scanner = node.scanner
    if scanner and not scanner_to_index[scanner] then
      scanner_to_index[scanner] = #scanners
      scanners[#scanners + 1] = scanner
    end
  end
  return scanners, scanner_to_index
end

local function save_passes(w, passes)
  w:begin_array("Passes")
  for _, s in ipairs(passes) do
    w:write_string(s.Name)
  end
  w:end_array()
end

local function save_scanners(w, scanners)
  w:begin_array("Scanners")
  for _, s in ipairs(scanners) do
    w:begin_object()
    w:write_string(s.Kind, 'Kind')
    w:begin_array("IncludePaths")
    for _, path in util.nil_ipairs(s.Paths) do
      w:write_string(path)
    end
    w:end_array()
    -- Serialize specialized state for generic scanners
    if s.Kind == 'generic' then
      w:write_bool(s.RequireWhitespace, 'RequireWhitespace')
      w:write_bool(s.UseSeparators, 'UseSeparators')
      w:write_bool(s.BareMeansSystem, 'BareMeansSystem')
      w:begin_array('Keywords')
      for _, kw in util.nil_ipairs(s.Keywords) do
        w:write_string(kw)
      end
      w:end_array()
      w:begin_array('KeywordsNoFollow')
      for _, kw in util.nil_ipairs(s.KeywordsNoFollow) do
        w:write_string(kw)
      end
      w:end_array()
    end
    w:end_object()
  end
  w:end_array()
end

local function save_nodes(w, nodes, pass_to_index, scanner_to_index)
  w:begin_array("Nodes")
  for idx, node in ipairs(nodes) do
    w:begin_object()
    assert(idx - 1 == node.index)
    if node.action then
      w:write_string(node.action, "Action")
    end
    if node.preaction then
      w:write_string(node.preaction, "PreAction")
    end
    w:write_string(node.annotation, "Annotation")

    w:write_number(pass_to_index[node.pass], "PassIndex")

    if #node.deps > 0 then
      w:begin_array("Deps")
      for _, dep in ipairs(node.deps) do
        w:write_number(dep.index)
      end
      w:end_array()
    end

    local function dump_file_list(list, name)
      if list and #list > 0 then
        w:begin_array(name)
        for _, fn in ipairs(list) do
          w:write_string(fn)
        end
        w:end_array(name)
      end
    end

    dump_file_list(node.inputs, "Inputs")
    dump_file_list(node.outputs, "Outputs")
    dump_file_list(node.aux_outputs, "AuxOutputs")

    -- Save environment strings
    local env_count = 0
    for k, v in util.nil_pairs(node.env) do
      env_count = env_count + 1
    end

    if env_count > 0 then
      w:begin_array("Env")
      for k, v in pairs(node.env) do
        w:begin_object()
        w:write_string(k, "Key")
        w:write_string(v, "Value")
        w:end_object()
      end
      w:end_array()
    end

    if node.scanner then
      w:write_number(scanner_to_index[node.scanner], "ScannerIndex")
    end

    if node.overwrite_outputs then
      w:write_bool(true, "OverwriteOutputs")
    end

    if node.is_precious then
      w:write_bool(true, "PreciousOutputs")
    end

    if node.expensive then
      w:write_bool(true, "Expensive")
    end

    w:end_object()
  end
  w:end_array()
end

local function save_configs(w, bindings, default_variant, default_subvariant)
  local configs = {}
  local variants = {}
  local subvariants = {}
  local config_index = {}
  local variant_index = {}
  local subvariant_index = {}
  local default_config = nil
  local host_platform = platform.host_platform() 

  for _, b in ipairs(bindings) do
    if not configs[b.Config.Name] then
      configs[b.Config.Name]        = #config_index
      config_index[#config_index+1] = b.Config.Name
    end
    if not variants[b.Variant.Name] then
      variants[b.Variant.Name]        = #variant_index
      variant_index[#variant_index+1] = b.Variant.Name
    end
    if not subvariants[b.SubVariant] then
      subvariants[b.SubVariant]             = #subvariant_index
      subvariant_index[#subvariant_index+1] = b.SubVariant
    end

    if b.Config.DefaultOnHost == host_platform then
      default_config = b.Config
    end
  end

  assert(#config_index > 0)
  assert(#variant_index > 0)
  assert(#subvariant_index > 0)

  local function dump_str_array(array, name)
    if array and #array > 0 then
      w:begin_array(name)
      for _, name in ipairs(array) do
        w:write_string(name)
      end
      w:end_array()
    end
  end

  w:begin_object("Setup")
  dump_str_array(config_index, "Configs")
  dump_str_array(variant_index, "Variants")
  dump_str_array(subvariant_index, "SubVariants")

  w:begin_array("BuildTuples")
  for index, binding in ipairs(bindings) do
    w:begin_object()
    w:write_number(configs[binding.Config.Name], "ConfigIndex")
    w:write_number(variants[binding.Variant.Name], "VariantIndex")
    w:write_number(subvariants[binding.SubVariant], "SubVariantIndex")
    local function store_node_index_array(nodes, name)
      w:begin_array(name)
      for _, node in util.nil_ipairs(nodes) do
        w:write_number(node.index)
      end
      w:end_array()
    end
    store_node_index_array(binding.AlwaysNodes, "AlwaysNodes")
    store_node_index_array(binding.DefaultNodes, "DefaultNodes")

    w:begin_object("NamedNodes")
    for name, node in pairs(binding.NamedNodes) do
      w:write_number(node.index, name)
    end
    w:end_object()

    w:end_object()
  end
  w:end_array()

  -- m_DefaultBuildTuple
  w:begin_object("DefaultBuildTuple")
  if default_config then
    w:write_number(configs[default_config.Name], "ConfigIndex")
  else
    w:write_number(-1, "ConfigIndex")
  end

  if default_variant then
    w:write_number(variants[default_variant.Name], "VariantIndex")
  else
    w:write_number(-1, "VariantIndex")
  end

  if default_subvariant then
    w:write_number(subvariants[default_subvariant], "SubVariantIndex")
  else
    w:write_number(-1, "SubVariantIndex")
  end
  w:end_object()

  w:end_object()

end

local function save_signatures(w, accessed_lua_files)
  w:begin_array("FileSignatures")
  for _, fn in ipairs(accessed_lua_files) do
    w:begin_object()
    local stat = native.stat_file(fn)
    if not stat.exists then
      errorf("accessed file %s is gone: %s", fn, err)
    end
    w:write_string(fn, "File")
    w:write_number(stat.timestamp, "Timestamp")
    w:end_object()
  end
  w:end_array()

  w:begin_array("GlobSignatures")
  local globs = dirwalk.all_queries()
  for _, glob in ipairs(globs) do
    w:begin_object()
    w:write_string(glob.Path, "Path")
    w:begin_array("Files")
    for _, fn in ipairs(glob.Files) do w:write_string(fn) end
    w:end_array()
    w:begin_array("SubDirs")
    for _, fn in ipairs(glob.SubDirs) do w:write_string(fn) end
    w:end_array()
    w:end_object()
  end
  w:end_array()
end

local function check_deps(nodes)
  for _, node in ipairs(nodes) do
    for _ , dep in ipairs(node.deps) do
      if dep.pass.BuildOrder > node.pass.BuildOrder then
        errorf("%s (pass: %s) depends on %s in later pass (%s)", node.annotation, node.pass.Name, dep.annotation, dep.pass.Name)
      end
    end
  end
end

function save_dag_data(bindings, default_variant, default_subvariant, content_digest_exts, misc_options)

  -- Call builtin function to get at accessed file table
  local accessed_lua_files = util.table_keys(get_accessed_files())

  misc_options = misc_options or {}
  local max_expensive_jobs = misc_options.MaxExpensiveJobs or -1

  printf("save_dag_data: %d bindings, %d accessed files", #bindings, #accessed_lua_files)

  local nodes = depgraph.get_all_nodes()

  -- Set node indices
  for idx, node in ipairs(nodes) do
    node.index = idx - 1
  end

  -- Set up array of passes
  local passes, pass_to_index = get_passes(nodes)

  -- Hook up dependencies due to input files
  setup_input_deps(nodes)

  check_deps(nodes)

  -- Find scanners
  local scanners, scanner_to_index = get_scanners(nodes)

  local w = njson.new('.tundra2.dag.json')

  w:begin_object()
  save_configs(w, bindings, default_variant, default_subvariant)
  save_passes(w, passes)
  save_scanners(w, scanners)
  save_nodes(w, nodes, pass_to_index, scanner_to_index)
  save_signatures(w, accessed_lua_files)

  if content_digest_exts and #content_digest_exts > 0 then
    w:begin_array("ContentDigestExtensions")
    for _, ext in ipairs(content_digest_exts) do
      w:write_string(ext)
    end
    w:end_array()
  end

  w:write_number(max_expensive_jobs, "MaxExpensiveCount")

  w:end_object()

  w:close()
end

