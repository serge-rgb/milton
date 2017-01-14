-- native.lua -- support for programs, static libraries and such

module(..., package.seeall)

local util     = require "tundra.util"
local nodegen  = require "tundra.nodegen"
local path     = require "tundra.path"
local depgraph = require "tundra.depgraph"

local _native_mt = nodegen.create_eval_subclass {
  DeclToEnvMappings = {
    Libs = "LIBS",
    Defines = "CPPDEFS",
    Includes = "CPPPATH",
    Frameworks = "FRAMEWORKS",
    LibPaths = "LIBPATH",
  },
}

local _object_mt = nodegen.create_eval_subclass({
  Suffix = "$(OBJECTSUFFIX)",
  Prefix = "",
  Action = "$(OBJCOM)",
  Label = "Object $(<)",
  OverwriteOutputs = true,
}, _native_mt)

local _program_mt = nodegen.create_eval_subclass({
  Suffix = "$(PROGSUFFIX)",
  Prefix = "$(PROGPREFIX)",
  Action = "$(PROGCOM)",
  Label = "Program $(@)",
  PreciousOutputs = true,
  OverwriteOutputs = true,
  Expensive = true,
}, _native_mt)

local _staticlib_mt = nodegen.create_eval_subclass({
  Suffix = "$(LIBSUFFIX)",
  Prefix = "$(LIBPREFIX)",
  Action = "$(LIBCOM)",
  Label = "StaticLibrary $(@)",
    -- Play it safe and delete the output files of this node before re-running it.
    -- Solves iterative issues with e.g. AR
  OverwriteOutputs = false,
  IsStaticLib = true,
}, _native_mt)

local _objgroup_mt = nodegen.create_eval_subclass({
  Label = "ObjGroup $(<)",
}, _native_mt)

local _shlib_mt = nodegen.create_eval_subclass({
  Suffix = "$(SHLIBSUFFIX)",
  Prefix = "$(SHLIBPREFIX)",
  Action = "$(SHLIBCOM)",
  Label = "SharedLibrary $(@)",
  PreciousOutputs = true,
  OverwriteOutputs = true,
  Expensive = true,
}, _native_mt)

local _extlib_mt = nodegen.create_eval_subclass({
  Suffix = "",
  Prefix = "",
  Label = "",
}, _native_mt)

local cpp_exts = util.make_lookup_table { ".cpp", ".cc", ".cxx", ".C" }

local _is_native_mt = util.make_lookup_table { _object_mt, _program_mt, _staticlib_mt, _shlib_mt, _extlib_mt, _objgroup_mt }

-- sources can be a mix of raw strings like {"file1.cpp", "file2.cpp"}
-- or files with filters like {{"file1.cpp", "file2.cpp"; Config = "..."}}
local function is_file_in_sources(sources, fn)
  for k,v in pairs(sources) do
    if type(v) == 'table' then
      return util.array_contains(v, fn)
    elseif v == fn then
      return true
    end
  end
  return false
end

function _native_mt:customize_env(env, raw_data)
  if env:get('GENERATE_PDB', '0') ~= '0' then
    -- Figure out the final linked PDB (the one next to the dll or exe)
    if raw_data.Target then
      local target = env:interpolate(raw_data.Target)
      local link_pdb = path.drop_suffix(target) .. '.pdb'
      env:set('_PDB_LINK_FILE', link_pdb)
    else
      env:set('_PDB_LINK_FILE', "$(OBJECTDIR)/" .. raw_data.Name .. ".pdb")
    end
    -- Keep the compiler's idea of the PDB file separate
    env:set('_PDB_CC_FILE', "$(OBJECTDIR)/" .. raw_data.Name .. "_ccpdb.pdb")
    env:set('_USE_PDB_CC', '$(_USE_PDB_CC_OPT)')
    env:set('_USE_PDB_LINK', '$(_USE_PDB_LINK_OPT)')
  end

  local pch = raw_data.PrecompiledHeader

  if pch and env:get('_PCH_SUPPORTED', '0') ~= '0' then
    assert(pch.Header)
    if not nodegen.resolve_pass(pch.Pass) then
      croak("%s: PrecompiledHeader requires a valid Pass", raw_data.Name)
    end
    local pch_dir = "$(OBJECTDIR)/__" .. raw_data.Name
    env:set('_PCH_HEADER', pch.Header)
    env:set('_PCH_FILE', pch_dir .. '/' .. pch.Header .. '$(_PCH_SUFFIX)')
    env:set('_PCH_INCLUDE_PATH', path.join(pch_dir, pch.Header))
    env:set('_USE_PCH', '$(_USE_PCH_OPT)')
    env:set('_PCH_SOURCE', path.normalize(pch.Source))
    env:set('_PCH_PASS', pch.Pass)
    if cpp_exts[path.get_extension(pch.Source)] then
      env:set('PCHCOMPILE', '$(PCHCOMPILE_CXX)')
    else
      env:set('PCHCOMPILE', '$(PCHCOMPILE_CC)')
    end
    local pch_source = path.remove_prefix(raw_data.SourceDir or '', pch.Source)
    if not is_file_in_sources(raw_data.Sources, pch_source) then
      raw_data.Sources[#raw_data.Sources + 1] = pch_source
    end
  end

  if env:has_key('MODDEF') then
    env:set('_USE_MODDEF', '$(_USE_MODDEF_OPT)')
  end
end

function _native_mt:create_dag(env, data, input_deps)

  local build_id = env:get("BUILD_ID")
  local my_pass = data.Pass
  local sources = data.Sources
  local libsuffix = { env:get("LIBSUFFIX") }
  local shlibsuffix = { env:get("SHLIBLINKSUFFIX") }
  local my_extra_deps = {}

  -- Link with libraries in dependencies.
  for _, dep in util.nil_ipairs(data.Depends) do

    if dep.Keyword == "SharedLibrary" then
      -- On Win32 toolsets, we need foo.lib
      -- On UNIX toolsets, we need -lfoo
      --
      -- We only want to add this if the node actually produced any output (i.e
      -- it's not completely filtered out.)
      local node = dep:get_dag(env:get_parent())
      if #node.outputs > 0 then
        my_extra_deps[#my_extra_deps + 1] = node
        local target = dep.Decl.Target or dep.Decl.Name
        target = env:interpolate(target)
        local dir, fn = path.split(target)
        local libpfx = env:interpolate("$(LIBPREFIX)")
        if fn:sub(1, libpfx:len()) == libpfx then
          fn = fn:sub(libpfx:len()+1)
        end
        local link_lib = path.drop_suffix(fn) .. '$(SHLIBLINKSUFFIX)'
        env:append('LIBS', link_lib)
        if dir:len() > 0 then
          env:append('LIBPATH', dir)
        end
      end
    elseif dep.Keyword == "StaticLibrary" then
      local node = dep:get_dag(env:get_parent())
      my_extra_deps[#my_extra_deps + 1] = node
      if not self.IsStaticLib then
        node:insert_output_files(sources, libsuffix)
      end
    elseif dep.Keyword == "ObjGroup" then
      -- We want all .obj files
      local objsuffix = { env:get("OBJECTSUFFIX") }

      -- And also .res files, if we know what that is
      if env:has_key("W32RESSUFFIX") then
        objsuffix[#objsuffix + 1] = env:get("W32RESSUFFIX")
      end

      local node = dep:get_dag(env:get_parent())
      my_extra_deps[#my_extra_deps + 1] = node
      if not sources then sources = {} end
      for _, dep in util.nil_ipairs(node.deps) do
        my_extra_deps[#my_extra_deps + 1] = dep
        dep:insert_output_files(sources, objsuffix)
      end
    else

      --[[

      A note about win32 import libraries:

      It is tempting to add an implicit input dependency on the import
      library of the linked-to shared library here; but this would be
      suboptimal:

      1. Because there is a dependency between the nodes themselves,
      the import library generation will always run before this link
      step is run. Therefore, the import lib will always exist and be
      updated before this link step runs.

      2. Because the import library is regenerated whenever the DLL is
      relinked we would have to custom-sign it (using a hash of the
      DLLs export list) to avoid relinking the executable all the
      time when only the DLL's internals change.

      3. The DLL's export list should be available in headers anyway,
      which is already covered in the compilation of the object files
      that actually uses those APIs.

      Therefore the best way right now is to not tell Tundra about the
      import lib at all and rely on header scanning to pick up API
      changes.

      An implicit input dependency would be needed however if someone
      is doing funky things with their import library (adding
      non-linker-generated code for example). These cases are so rare
      that we can safely put them off.

      ]]--
    end
  end

  -- Make sure sources are not specified more than once. This can happen when
  -- there are recursive dependencies on object groups.
  if data.Sources and #data.Sources > 0 then
    data.Sources = util.uniq(data.Sources)
  end

  local aux_outputs = env:get_list("AUX_FILES_" .. self.Keyword:upper(), {})

  if env:get('GENERATE_PDB', '0') ~= '0' then
    aux_outputs[#aux_outputs + 1] = "$(_PDB_LINK_FILE)"
    aux_outputs[#aux_outputs + 1] = "$(_PDB_CC_FILE)"
  end

  local extra_inputs = {}

  if env:has_key('MODDEF') then
    extra_inputs[#extra_inputs + 1] = "$(MODDEF)"
  end

  local targets = nil

  if self.Action then
    targets = { nodegen.get_target(data, self.Suffix, self.Prefix) }
  end

  local deps = util.merge_arrays(input_deps, my_extra_deps)

  local dag = depgraph.make_node {
    Env                 = env,
    Label               = self.Label,
    Pass                = data.Pass,
    Action              = self.Action,
    PreAction           = data.PreAction,
    InputFiles          = data.Sources,
    InputFilesUntracked = data.UntrackedSources,
    OutputFiles         = targets,
    AuxOutputFiles      = aux_outputs,
    ImplicitInputs      = extra_inputs,
    Dependencies        = deps,
    OverwriteOutputs    = self.OverwriteOutputs,
    PreciousOutputs     = self.PreciousOutputs,
    Expensive           = self.Expensive,
  }

  -- Remember this dag node for IDE file generation purposes
  data.__DagNode = dag

  return dag
end

local native_blueprint = {
  Name = {
    Required = true,
    Help = "Set output (base) filename",
    Type = "string",
  },
  Sources = {
    Required = true,
    Help = "List of source files",
    Type = "source_list",
    ExtensionKey = "NATIVE_SUFFIXES",
  },
  UntrackedSources = {
    Help = "List of input files that are not tracked",
    Type = "source_list",
    ExtensionKey = "NATIVE_SUFFIXES",
  },
  Target = {
    Help = "Override target location",
    Type = "string",
  },
  PreAction = {
    Help = "Optional action to run before main action.",
    Type = "string",
  },
  PrecompiledHeader = {
    Help = "Enable precompiled header (if supported)",
    Type = "table",
  },
  IdeGenerationHints = {
    Help = "Data to support control IDE file generation",
    Type = "table",
  },
}

local external_blueprint = {
  Name = {
    Required = true,
    Help = "Set name of the external library",
    Type = "string",
  },
}

local external_counter = 1

function _extlib_mt:create_dag(env, data, input_deps)
  local name = string.format("dummy node for %s (%d)", data.Name, external_counter)
  external_counter = external_counter + 1
  return depgraph.make_node {
    Env          = env,
    Label        = name,
    Pass         = data.Pass,
    Dependencies = input_deps,
  }
end

nodegen.add_evaluator("Object", _object_mt, native_blueprint)
nodegen.add_evaluator("Program", _program_mt, native_blueprint)
nodegen.add_evaluator("StaticLibrary", _staticlib_mt, native_blueprint)
nodegen.add_evaluator("SharedLibrary", _shlib_mt, native_blueprint)
nodegen.add_evaluator("ExternalLibrary", _extlib_mt, external_blueprint)
nodegen.add_evaluator("ObjGroup", _objgroup_mt, native_blueprint)
