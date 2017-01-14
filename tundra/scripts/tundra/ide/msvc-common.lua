module(..., package.seeall)

local native  = require "tundra.native"
local nodegen = require "tundra.nodegen"
local path    = require "tundra.path"
local util    = require "tundra.util"

LF = '\r\n'
local UTF_HEADER = '\239\187\191' -- byte mark EF BB BF 

local VERSION_NUMBER = "12.00"
local VERSION_YEAR   = "2012"
local HOOKS          = {}

local msvc_generator = {}
msvc_generator.__index = msvc_generator

local project_types = util.make_lookup_table {
  "Program", "SharedLibrary", "StaticLibrary", "CSharpExe", "CSharpLib", "ObjGroup",
}

local toplevel_stuff = util.make_lookup_table {
  ".exe", ".lib", ".dll",
}

local binary_extension = util.make_lookup_table {
  ".exe", ".lib", ".dll", ".pdb", ".res", ".obj", ".o", ".a",
}

local header_exts = util.make_lookup_table {
  ".h", ".hpp", ".hh", ".inl",
}

-- Scan for sources, following dependencies until those dependencies seem to be
-- a different top-level unit
local function get_sources(dag, sources, generated, level, dag_lut)
  for _, output in util.nil_ipairs(dag.outputs) do
    local ext = path.get_extension(output)
    if not binary_extension[ext] then
      generated[output] = true
      sources[output] = true -- pick up generated headers
    end
  end

  for _, input in util.nil_ipairs(dag.inputs) do
    local ext = path.get_extension(input)
    if not binary_extension[ext] then
      sources[input] = true
    end
  end

  for _, dep in util.nil_ipairs(dag.deps) do
    if not dag_lut[dep] then -- don't go into other top-level DAGs
      get_sources(dep, sources, generated, level + 1, dag_lut)
    end
  end
end

function get_guid_string(data)
  local sha1 = native.digest_guid(data)
  local guid = sha1:sub(1, 8) .. '-' .. sha1:sub(9,12) .. '-' .. sha1:sub(13,16) .. '-' .. sha1:sub(17,20) .. '-' .. sha1:sub(21, 32)
  assert(#guid == 36) 
  return guid:upper()
end

local function get_headers(unit, source_lut, dag_lut, name_to_dags)
  local src_dir = ''

  if not unit.Decl then
    -- Ignore ExternalLibrary and similar that have no data.
    return
  end

  if unit.Decl.SourceDir then
    src_dir = unit.Decl.SourceDir .. '/'
  end
  for _, src in util.nil_ipairs(nodegen.flatten_list('*-*-*-*', unit.Decl.Sources)) do
    if type(src) == "string" then
      local ext = path.get_extension(src)
      if header_exts[ext] then
        local full_path = path.normalize(src_dir .. src)
        source_lut[full_path] = true
      end
    end
  end

  local function toplevel(u)
    if type(u) == "string" then
      return type(name_to_dags[u]) ~= "nil"
    end

    for _, dag in pairs(u.Decl.__DagNodes) do
      if dag_lut[dag] then
        return true
      end
    end
    return false
  end

  -- Repeat for dependencies ObjGroups
  for _, dep in util.nil_ipairs(nodegen.flatten_list('*-*-*-*', unit.Decl.Depends)) do

    if not toplevel(dep) then
      get_headers(dep, source_lut, dag_lut)
    end
  end
end

local function make_meta_project(base_dir, data)
  data.Guid               = get_guid_string(data.Name)
  data.IdeGenerationHints = { Msvc = { SolutionFolder = "Build System Meta" } }
  data.IsMeta             = true
  data.RelativeFilename   = data.Name .. ".vcxproj"
  data.Filename           = base_dir .. data.RelativeFilename
  data.Type               = "meta"
  if not data.Sources then
    data.Sources            = {}
  end
  return data
end

local function tundra_cmdline(args)
  local root_dir    = native.getcwd()
  return "\"" .. TundraExePath .. "\" -C \"" .. root_dir .. "\" " .. args
end

local function project_regen_commandline(ide_script)
  return tundra_cmdline("-g " .. ide_script)
end

local function make_project_data(units_raw, env, proj_extension, hints, ide_script)

  -- Filter out stuff we don't care about.
  local units = util.filter(units_raw, function (u)
    return u.Decl.Name and project_types[u.Keyword]
  end)

  local base_dir = hints.MsvcSolutionDir and (hints.MsvcSolutionDir .. '\\') or env:interpolate('$(OBJECTROOT)$(SEP)')
  native.mkdir(base_dir)

  local project_by_name = {}
  local all_sources = {}
  local dag_node_lut = {} -- lookup table of all named, top-level DAG nodes 
  local name_to_dags = {} -- table mapping unit name to array of dag nodes (for configs)

  -- Map out all top-level DAG nodes
  for _, unit in ipairs(units) do
    local decl = unit.Decl

    local dag_nodes = assert(decl.__DagNodes, "no dag nodes for " .. decl.Name)
    for build_id, dag_node in pairs(dag_nodes) do
      dag_node_lut[dag_node] = unit
      local array = name_to_dags[decl.Name]
      if not array then
        array = {}
        name_to_dags[decl.Name] = array
      end
      array[#array + 1] = dag_node
    end
  end

  local function get_output_project(name)
    if not project_by_name[name] then
      local relative_fn = name .. proj_extension
      project_by_name[name] = {
        Name             = name,
        Sources          = {},
        RelativeFilename = relative_fn,
        Filename         = base_dir .. relative_fn,
        Guid             = get_guid_string(name),
        BuildByDefault   = hints.BuildAllByDefault,
      }
    end
    return project_by_name[name]
  end

  -- Sort units based on dependency complexity. We want to visit the leaf nodes
  -- first so that any source file references are picked up as close to the
  -- bottom of the dependency chain as possible.
  local unit_weights = {}
  for _, unit in ipairs(units) do
    local decl = unit.Decl
    local stack = { }
    for _, dag in pairs(decl.__DagNodes) do
      stack[#stack + 1] = dag
    end
    local weight = 0
    while #stack > 0 do
      local node = table.remove(stack)
      if dag_node_lut[node] then
        weight = weight + 1
      end
      for _, dep in util.nil_ipairs(node.deps) do
        stack[#stack + 1] = dep
      end
    end
    unit_weights[unit] = weight
  end

  table.sort(units, function (a, b)
    return unit_weights[a] < unit_weights[b]
  end)

  -- Keep track of what source files have already been grabbed by other projects.
  local grabbed_sources = {}

  for _, unit in ipairs(units) do
    local decl = unit.Decl
    local name = decl.Name

    local source_lut = {}
    local generated_lut = {}
    for build_id, dag_node in pairs(decl.__DagNodes) do
      get_sources(dag_node, source_lut, generated_lut, 0, dag_node_lut)
    end

    -- Explicitly add all header files too as they are not picked up from the DAG
    -- Also pick up headers from non-toplevel DAGs we're depending on
    get_headers(unit, source_lut, dag_node_lut, name_to_dags)

    -- Figure out which project should get this data.
    local output_name = name
    local ide_hints = unit.Decl.IdeGenerationHints
    if ide_hints then
      if ide_hints.OutputProject then
        output_name = ide_hints.OutputProject
      end
    end

    local proj = get_output_project(output_name)

    if output_name == name then
      -- This unit is the real thing for this project, not something that's
      -- just being merged into it (like an ObjGroup). Set some more attributes.
      proj.IdeGenerationHints = ide_hints
      proj.DagNodes           = decl.__DagNodes
      proj.Unit               = unit
    end

    for src, _ in pairs(source_lut) do
      local norm_src = path.normalize(src)
      if not grabbed_sources[norm_src] then
        grabbed_sources[norm_src] = unit
        local is_generated = generated_lut[src]
        proj.Sources[#proj.Sources+1] = {
          Path      = norm_src,
          Generated = is_generated,
        }
      end
    end
  end

  -- Get all accessed Lua files
  local accessed_lua_files = util.table_keys(get_accessed_files())

  -- Filter out the ones that belong to this build (exclude ones coming from Tundra) 
  local function is_non_tundra_lua_file(p)
    return not path.is_absolute(p)
  end
  local function make_src_node(p)
    return { Path = path.normalize(p) }
  end
  local source_list = util.map(util.filter(accessed_lua_files, is_non_tundra_lua_file), make_src_node)

  local solution_hints = hints.MsvcSolutions
  if not solution_hints then
    print("No IdeGenerationHints.MsvcSolutions specified - using defaults")
    solution_hints = {
      ['tundra-generated.sln'] = {}
    }
  end

  local projects = util.table_values(project_by_name)
  local vanilla_projects = util.clone_array(projects)

  local solutions = {}

  -- Create meta project to regenerate solutions/projects. Added to every solution.
  local regen_meta_proj = make_meta_project(base_dir, {
    Name               = "00-Regenerate-Projects",
    FriendlyName       = "Regenerate Solutions and Projects",
    BuildCommand       = project_regen_commandline(ide_script),
  })

  projects[#projects + 1] = regen_meta_proj

  for name, data in pairs(solution_hints) do
    local sln_projects
    local ext_projects = {}
    if data.Projects then
      sln_projects = {}
      for _, pname in ipairs(data.Projects) do
        local pp = project_by_name[pname]
        if not pp then
          errorf("can't find project %s for inclusion in %s -- check your MsvcSolutions data", pname, name)
        end
        sln_projects[#sln_projects + 1] = pp
      end
    else
      -- All the projects (that are not meta)
      sln_projects = util.clone_array(vanilla_projects)
    end

    for _, ext in util.nil_ipairs(data.ExternalProjects) do
      ext_projects[#ext_projects + 1] = ext
    end

    local meta_proj = make_meta_project(base_dir, {
      Name               = "00-tundra-" .. path.drop_suffix(name),
      FriendlyName       = "Build This Solution",
      BuildByDefault     = true,
      Sources            = source_list,
      BuildProjects      = util.clone_array(sln_projects),
    })

    sln_projects[#sln_projects + 1] = regen_meta_proj
    sln_projects[#sln_projects + 1] = meta_proj
    projects[#projects + 1] = meta_proj

    solutions[#solutions + 1] = {
      Filename             = base_dir .. name,
      Projects             = sln_projects,
      ExternalProjects     = ext_projects,
      BuildSolutionProject = meta_proj,
    }
  end

  return solutions, projects
end

local cl_tags = {
  ['.h']   = 'ClInclude',
  ['.hh']  = 'ClInclude',
  ['.hpp'] = 'ClInclude',
  ['.inl'] = 'ClInclude',
}

local function slurp_file(fn)
  local fh, err = io.open(fn, 'rb')
  if fh then
    local data = fh:read("*all")
    fh:close()
    return data
  end
  return ''
end

local function replace_if_changed(new_fn, old_fn)
  local old_data = slurp_file(old_fn)
  local new_data = slurp_file(new_fn)
  if old_data == new_data then
    os.remove(new_fn)
    return
  end
  printf("Updating %s", old_fn)
  os.remove(old_fn)
  os.rename(new_fn, old_fn)
end

function msvc_generator:generate_solution(fn, projects, ext_projects, solution)
  local sln = io.open(fn .. '.tmp', 'wb')
  sln:write(UTF_HEADER, LF, "Microsoft Visual Studio Solution File, Format Version ", VERSION_NUMBER, LF, "# Visual Studio ", VERSION_YEAR, LF)

  -- Map folder names to array of projects under that folder
  local sln_folders = {}
  for _, proj in ipairs(projects) do
    local hints = proj.IdeGenerationHints
    local msvc_hints = hints and hints.Msvc or nil
    local folder = msvc_hints and msvc_hints.SolutionFolder or nil
    if folder then
      local projects = sln_folders[folder] or {}
      projects[#projects + 1] = proj
      sln_folders[folder] = projects
    end
  end

  for _, proj in ipairs(projects) do
    local name = proj.Name
    local fname = proj.RelativeFilename
    local guid = proj.Guid
    sln:write(string.format('Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "%s", "%s", "{%s}"', name, fname, guid), LF)
    sln:write('EndProject', LF)
  end

  -- Dump external projects. Make them depend on everything in this solution being built by Tundra.
  for _, data in util.nil_ipairs(ext_projects) do
    local guid = data.Guid
    local fname = path.normalize(path.join(native.getcwd(), data.Filename))
    local name = path.get_filename_base(fname)
    sln:write(string.format('Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "%s", "%s", "{%s}"', name, fname, guid), LF)
    local build_sln_proj = solution.BuildSolutionProject
    if build_sln_proj then
      local meta_guid = build_sln_proj.Guid
      sln:write('\tProjectSection(ProjectDependencies) = postProject', LF)
      sln:write('\t\t{', meta_guid,'} = {', meta_guid,'}', LF)
      sln:write('\tEndProjectSection', LF)
    end
    sln:write('EndProject', LF)
  end

  for folder_name, _ in pairs(sln_folders) do
    local folder_guid = get_guid_string("folder/" .. folder_name)
    sln:write(string.format('Project("{2150E333-8FDC-42A3-9474-1A3956D46DE8}") = "%s", "%s", "{%s}"', folder_name, folder_name, folder_guid), LF)
    sln:write('EndProject', LF)
  end

  sln:write("Global", LF)
  sln:write("\tGlobalSection(SolutionConfigurationPlatforms) = preSolution", LF)
  for _, tuple in ipairs(self.config_tuples) do
    sln:write(string.format('\t\t%s = %s', tuple.MsvcName, tuple.MsvcName), LF)
  end
  sln:write("\tEndGlobalSection", LF)

  sln:write("\tGlobalSection(ProjectConfigurationPlatforms) = postSolution", LF)
  for _, proj in ipairs(projects) do
    for _, tuple in ipairs(self.config_tuples) do
      local leader = string.format('\t\t{%s}.%s.', proj.Guid, tuple.MsvcName)
      sln:write(leader, "ActiveCfg = ", tuple.MsvcName, LF)
      if proj.BuildByDefault then
        sln:write(leader, "Build.0 = ", tuple.MsvcName, LF)
      end
    end
  end

  -- External projects build by default, and after Tundra is done (depends on "Build this solution").
  for _, proj in util.nil_ipairs(ext_projects) do
    for _, tuple in ipairs(self.config_tuples) do
      local leader = string.format('\t\t{%s}.%s.', proj.Guid, tuple.MsvcName)
      sln:write(leader, "ActiveCfg = ", tuple.MsvcName, LF)
      if not proj.Platform or proj.Platform == tuple.MsvcPlatform then
        sln:write(leader, "Build.0 = ", tuple.MsvcName, LF)
      end
    end
  end
  sln:write("\tEndGlobalSection", LF)

  sln:write("\tGlobalSection(SolutionProperties) = preSolution", LF)
  sln:write("\t\tHideSolutionNode = FALSE", LF)
  sln:write("\tEndGlobalSection", LF)

  sln:write("\tGlobalSection(NestedProjects) = preSolution", LF)
  for folder_name, projects in pairs(sln_folders) do
    local folder_guid = get_guid_string("folder/" .. folder_name)
    for _, project in ipairs(projects) do
      sln:write(string.format('\t\t{%s} = {%s}', project.Guid, folder_guid), LF)
    end
  end
  sln:write("\tEndGlobalSection", LF)

  sln:write("EndGlobal", LF)
  sln:close()

  replace_if_changed(fn .. ".tmp", fn)
end

local function find_dag_node_for_config(project, tuple)
  local build_id = string.format("%s-%s-%s", tuple.Config.Name, tuple.Variant.Name, tuple.SubVariant)
  local nodes = project.DagNodes
  if not nodes then
    return nil
  end

  if nodes[build_id] then
    return nodes[build_id]
  end
  errorf("couldn't find config %s for project %s (%d dag nodes) - available: %s",
    build_id, project.Name, #nodes, table.concat(util.table_keys(nodes), ", "))
end

function msvc_generator:generate_project(project, all_projects)
  local fn = project.Filename
  local p = assert(io.open(fn .. ".tmp", 'wb'))
  p:write('<?xml version="1.0" encoding="utf-8"?>', LF)
  p:write('<Project')
  p:write(' DefaultTargets="Build"')

  -- This doesn't seem to change any behaviour, but this is the default
  -- value when creating a makefile project from VS2013 and VS2015
  -- wizards.
  if VERSION_YEAR == '2015' then
    p:write(' ToolsVersion="14.0"')
  elseif VERSION_YEAR == '2013' then
    p:write(' ToolsVersion="12.0"')
  else
    p:write(' ToolsVersion="4.0"')
  end

  p:write(' xmlns="http://schemas.microsoft.com/developer/msbuild/2003"')
  p:write('>', LF)

  -- List all project configurations
  p:write('\t<ItemGroup Label="ProjectConfigurations">', LF)
  for _, tuple in ipairs(self.config_tuples) do
    p:write('\t\t<ProjectConfiguration Include="', tuple.MsvcName, '">', LF)
    p:write('\t\t\t<Configuration>', tuple.MsvcConfiguration, '</Configuration>', LF)
    p:write('\t\t\t<Platform>', tuple.MsvcPlatform, '</Platform>', LF)
    p:write('\t\t</ProjectConfiguration>', LF)
  end
  p:write('\t</ItemGroup>', LF)

  p:write('\t<PropertyGroup Label="Globals">', LF)
  p:write('\t\t<ProjectGuid>{', project.Guid, '}</ProjectGuid>', LF)
  p:write('\t\t<Keyword>MakeFileProj</Keyword>', LF)
  if project.FriendlyName then
    p:write('\t\t<ProjectName>', project.FriendlyName, '</ProjectName>', LF)
  end

  if HOOKS.global_properties then
    HOOKS.global_properties(p, project)
  end

  p:write('\t</PropertyGroup>', LF)
  p:write('\t<PropertyGroup>', LF)
  if VERSION_YEAR == '2012' then
    p:write('\t\t<_ProjectFileVersion>10.0.30319.1</_ProjectFileVersion>', LF)
  end
  p:write('\t</PropertyGroup>', LF)

  p:write('\t<Import Project="$(VCTargetsPath)\Microsoft.Cpp.Default.props" />', LF)

  -- Mark all project configurations as makefile-type projects
  for _, tuple in ipairs(self.config_tuples) do
    p:write('\t<PropertyGroup Condition="\'$(Configuration)|$(Platform)\'==\'', tuple.MsvcName, '\'" Label="Configuration">', LF)
    p:write('\t\t<ConfigurationType>Makefile</ConfigurationType>', LF)
    p:write('\t\t<UseDebugLibraries>true</UseDebugLibraries>', LF) -- I have no idea what this setting affects
    if VERSION_YEAR == '2012' then
      p:write('\t\t<PlatformToolset>v110</PlatformToolset>', LF) -- I have no idea what this setting affects
    elseif VERSION_YEAR == '2013' then
      p:write('\t\t<PlatformToolset>v120</PlatformToolset>', LF) -- I have no idea what this setting affects
    elseif VERSION_YEAR == '2015' then
      p:write('\t\t<PlatformToolset>v140</PlatformToolset>', LF) -- I have no idea what this setting affects
    end
    p:write('\t</PropertyGroup>', LF)
  end

  p:write('\t<Import Project="$(VCTargetsPath)\Microsoft.Cpp.props" />', LF)

  for _, tuple in ipairs(self.config_tuples) do
    p:write('\t<PropertyGroup Condition="\'$(Configuration)|$(Platform)\'==\'', tuple.MsvcName, '\'">', LF)

    local dag_node = find_dag_node_for_config(project, tuple)
    local include_paths, defines
    if dag_node then
      local env = dag_node.src_env
      local paths = util.map(env:get_list("CPPPATH"), function (p)
        local ip = path.normalize(env:interpolate(p))
        if not path.is_absolute(ip) then
          ip = native.getcwd() .. '\\' .. ip
        end
        return ip
      end)
      include_paths = table.concat(paths, ';')
      local ext_paths = env:get_external_env_var('INCLUDE')
      if ext_paths then
        include_paths = include_paths .. ';' .. ext_paths
      end
      defines = env:interpolate("$(CPPDEFS:j;)")
    else
      include_paths = ''
      defines = ''
    end

    local root_dir    = native.getcwd()
    local build_id    = string.format("%s-%s-%s", tuple.Config.Name, tuple.Variant.Name, tuple.SubVariant)
    local base        = "\"" .. TundraExePath .. "\" -C \"" .. root_dir .. "\" "
    local build_cmd   = base .. build_id
    local clean_cmd   = base .. "--clean " .. build_id
    local rebuild_cmd = base .. "--rebuild " .. build_id

    if project.BuildCommand then
      build_cmd = project.BuildCommand
      clean_cmd = ""
      rebuild_cmd = ""
    elseif not project.IsMeta then
      build_cmd   = build_cmd .. " " .. project.Name
      clean_cmd   = clean_cmd .. " " .. project.Name
      rebuild_cmd = rebuild_cmd .. " " .. project.Name
    else
      local all_projs_str = table.concat(
        util.map(assert(project.BuildProjects), function (p) return p.Name end), ' ')
      build_cmd   = build_cmd .. " " .. all_projs_str
      clean_cmd   = clean_cmd .. " " .. all_projs_str
      rebuild_cmd = rebuild_cmd .. " " .. all_projs_str
    end

    p:write('\t\t<NMakeBuildCommandLine>', build_cmd, '</NMakeBuildCommandLine>', LF)
    p:write('\t\t<NMakeOutput></NMakeOutput>', LF)
    p:write('\t\t<NMakeCleanCommandLine>', clean_cmd, '</NMakeCleanCommandLine>', LF)
    p:write('\t\t<NMakeReBuildCommandLine>', rebuild_cmd, '</NMakeReBuildCommandLine>', LF)
    p:write('\t\t<NMakePreprocessorDefinitions>', defines, ';$(NMakePreprocessorDefinitions)</NMakePreprocessorDefinitions>', LF)
    p:write('\t\t<NMakeIncludeSearchPath>', include_paths, ';$(NMakeIncludeSearchPath)</NMakeIncludeSearchPath>', LF)
    p:write('\t\t<NMakeForcedIncludes>$(NMakeForcedIncludes)</NMakeForcedIncludes>', LF)
    p:write('\t</PropertyGroup>', LF)
  end

  if HOOKS.pre_sources then
    HOOKS.pre_sources(p, project)
  end

  -- Emit list of source files
  p:write('\t<ItemGroup>', LF)
  for _, record in ipairs(project.Sources) do
    local path_str = assert(record.Path)
    if not path.is_absolute(path_str) then
      path_str = native.getcwd() .. '\\' .. path_str
    end
    local ext = path.get_extension(path_str)
    local cl_tag = cl_tags[ext] or 'ClCompile'
    p:write('\t\t<', cl_tag,' Include="', path_str, '" />', LF)
  end
  p:write('\t</ItemGroup>', LF)

  local post_src_hook = HOOKS.post_sources
  if post_src_hook then
    post_src_hook(p, project)
  end

  p:write('\t<Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" />', LF)

  if VERSION_YEAR == "2012" then
    -- Import helper msbuild stuff to make build aborting work propertly in VS2012
    local xml = path.normalize(TundraScriptDir .. '/tundra/ide/msvc-rules.xml')
    p:write('\t<Import Project="', xml, '" />', LF)
  end

  p:write('</Project>', LF)
  p:close()

  replace_if_changed(fn .. ".tmp", fn)
end

local function get_common_dir(sources)
  local dir_tokens = {}
  for _, src in ipairs(sources) do
    local path = assert(src.Path)
    if not tundra.path.is_absolute(path) then
      local subdirs = {}
      for subdir in path:gmatch("([^\\\]+)\\") do
        subdirs[#subdirs + 1] = subdir
      end

      if #dir_tokens == 0 then
        dir_tokens = subdirs
      else
        for i = 1, #dir_tokens do
          if dir_tokens[i] ~= subdirs[i] then
            while #dir_tokens >= i do
              table.remove(dir_tokens)
            end
            break
          end
        end
      end
    end
  end

  local result = table.concat(dir_tokens, '\\')
  if #result > 0 then
    result = result .. '\\'
  end
  return result
end

function msvc_generator:generate_project_filters(project)
  local fn = project.Filename .. ".filters"
  local p = assert(io.open(fn .. ".tmp", 'wb'))
  p:write('<?xml version="1.0" encoding="Windows-1252"?>', LF)
  p:write('<Project')
  p:write(' ToolsVersion="4.0"')
  p:write(' xmlns="http://schemas.microsoft.com/developer/msbuild/2003"')
  p:write('>', LF)

  local common_dir = get_common_dir(util.filter(project.Sources, function (s) return not s.Generated end))
  local common_dir_gen = get_common_dir(util.filter(project.Sources, function (s) return s.Generated end))

  local filters = {}
  local sources = {}

  -- Mangle source filenames, and find which filters need to be created
  for _, record in ipairs(project.Sources) do
    local fn = record.Path
    local common_start = record.Generated and common_dir_gen or common_dir
    if fn:find(common_start, 1, true) then
      fn = fn:sub(#common_start+1)
    end

    local dir, filename = path.split(fn)

    if dir == '.' then
      dir = nil
    end

    local abs_path = record.Path
    if not path.is_absolute(abs_path) then
      abs_path = native.getcwd() .. '\\' .. abs_path
    end

    if record.Generated then
      dir = 'Generated Files'
    end

    sources[#sources + 1] = {
      FullPath  = abs_path,
      Directory = dir,
    }

    -- Register filter and all its parents
    while dir and dir ~= '.' do
      filters[dir] = true
      dir, _ = path.split(dir)
    end
  end

  -- Emit list of filters
  p:write('\t<ItemGroup>', LF)
  for filter_name, _ in pairs(filters) do
    if filter_name ~= "" then
      filter_guid = get_guid_string(filter_name)
      p:write('\t\t<Filter Include="', filter_name, '">', LF)
      p:write('\t\t\t<UniqueIdentifier>{', filter_guid, '}</UniqueIdentifier>', LF)
      p:write('\t\t</Filter>', LF)
    end
  end
  p:write('\t</ItemGroup>', LF)

  -- Emit list of source files
  p:write('\t<ItemGroup>', LF)
  for _, source in ipairs(sources) do
    local ext = path.get_extension(source.FullPath)
    local cl_tag = cl_tags[ext] or 'ClCompile'
    if not source.Directory then
      p:write('\t\t<', cl_tag, ' Include="', source.FullPath, '" />', LF)
    else
      p:write('\t\t<', cl_tag, ' Include="', source.FullPath, '">', LF)
      p:write('\t\t\t<Filter>', source.Directory, '</Filter>', LF)
      p:write('\t\t</', cl_tag, '>', LF)
    end
  end
  p:write('\t</ItemGroup>', LF)

  p:write('</Project>', LF)

  p:close()

  replace_if_changed(fn .. ".tmp", fn)
end

function msvc_generator:generate_project_user(project)
  local fn = project.Filename .. ".user"
  -- Don't overwrite user settings
  do
    local p, err = io.open(fn, 'rb')
    if p then
      p:close()
      return
    end
  end

  local p = assert(io.open(fn, 'wb'))
  p:write('<?xml version="1.0" encoding="utf-8"?>', LF)
  p:write('<Project')
  p:write(' ToolsVersion="4.0"')
  p:write(' xmlns="http://schemas.microsoft.com/developer/msbuild/2003"')
  p:write('>', LF)

  for _, tuple in ipairs(self.config_tuples) do
    local dag_node = find_dag_node_for_config(project, tuple)
    if dag_node then
      local exe = nil
      for _, output in util.nil_ipairs(dag_node.outputs) do
        if output:match("%.exe") then
          exe = output
          break
        end
      end
      if exe then
        p:write('\t<PropertyGroup Condition="\'$(Configuration)|$(Platform)\'==\'', tuple.MsvcName, '\'">', LF)
        p:write('\t\t<LocalDebuggerCommand>', native.getcwd() .. '\\' .. exe, '</LocalDebuggerCommand>', LF)
        p:write('\t\t<DebuggerFlavor>WindowsLocalDebugger</DebuggerFlavor>', LF)
        p:write('\t\t<LocalDebuggerWorkingDirectory>', native.getcwd(), '</LocalDebuggerWorkingDirectory>', LF)
        p:write('\t</PropertyGroup>', LF)
      end
    end
  end

  p:write('</Project>', LF)

  p:close()
end
  
function msvc_generator:generate_files(ngen, config_tuples, raw_nodes, env, default_names, hints, ide_script)
  assert(config_tuples and #config_tuples > 0)

  if not hints then
    hints = {}
  end

  local complained_mappings = {}

  self.msvc_platforms = {}
  local msvc_hints = hints.Msvc or {}
  local variant_mappings = msvc_hints.VariantMappings or {}
  local platform_mappings = msvc_hints.PlatformMappings or {}
  local full_mappings = msvc_hints.FullMappings or {}

  for _, tuple in ipairs(config_tuples) do
    local build_id    = string.format("%s-%s-%s", tuple.Config.Name, tuple.Variant.Name, tuple.SubVariant)
    if full_mappings[build_id] then
      local m = full_mappings[build_id]
      tuple.MsvcConfiguration = assert(m.Config)
      tuple.MsvcPlatform = assert(m.Platform)
    elseif variant_mappings[tuple.Variant.Name] then
      tuple.MsvcConfiguration = variant_mappings[tuple.Variant.Name]
    elseif variant_mappings[tuple.Variant.Name .. "-" .. tuple.SubVariant] then
      tuple.MsvcConfiguration = variant_mappings[tuple.Variant.Name .. "-" .. tuple.SubVariant]
    else
      tuple.MsvcConfiguration = tuple.Variant.Name
    end

    -- Use IdeGenerationHints.Msvc.PlatformMappings table to map tundra
    -- configurations to MSVC platform names. Note that this isn't a huge deal
    -- for building stuff as Tundra doesn't care about this setting. But it
    -- might influence the choice of debugger and affect include paths for
    -- things like Intellisense that certain users may care about.
    if not tuple.MsvcPlatform then
      tuple.MsvcPlatform = platform_mappings[tuple.Config.Name]
    end

    -- If we didn't find anything, warn and then default to Win32, which VS
    -- will always accept (or so one would assume)
    if not tuple.MsvcPlatform then
      tuple.MsvcPlatform = "Win32"
      if not complained_mappings[tuple.Config.Name] then
        printf("warning: No VS platform mapping for %s, mapping to Win32", tuple.Config.Name)
        print("(Add one to IdeGenerationHints.Msvc.PlatformMappings to override)")
        complained_mappings[tuple.Config.Name] = true
      end
    end
    tuple.MsvcName = tuple.MsvcConfiguration .. "|" .. tuple.MsvcPlatform
    self.msvc_platforms[tuple.MsvcPlatform] = true
  end

  self.config_tuples = config_tuples

  printf("Generating Visual Studio projects for %d configurations/variants", #config_tuples)

  -- Figure out where we're going to store the projects
  local solutions, projects = make_project_data(raw_nodes, env, ".vcxproj", hints, ide_script)

  local proj_lut = {}
  for _, p in ipairs(projects) do
    proj_lut[p.Name] = p
  end

  for _, sln in pairs(solutions) do
    self:generate_solution(sln.Filename, sln.Projects, sln.ExternalProjects, sln)
  end

  for _, proj in ipairs(projects) do
    self:generate_project(proj, projects)
    self:generate_project_filters(proj)
    self:generate_project_user(proj)
  end
end

function setup(version_short, version_year, hooks)
  VERSION_NUMBER = version_short
  VERSION_YEAR   = version_year
  if hooks then
    HOOKS          = hooks
  end
  nodegen.set_ide_backend(function(...)
    local state = setmetatable({}, msvc_generator)
    state:generate_files(...)
  end)
end

