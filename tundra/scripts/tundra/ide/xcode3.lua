-- Xcode 3 (works in 4 as well) Workspace/Project file generation

module(..., package.seeall)

local path = require "tundra.path"
local nodegen = require "tundra.nodegen"
local util = require "tundra.util"
local native = require "tundra.native"

local xcode_generator = {}
local xcode_generator = {}
xcode_generator.__index = xcode_generator

function xcode_generator:generate_workspace(fn, projects)
  local sln = io.open(fn, 'wb')

  sln:write('<?xml version="1.0" encoding="UTF-8"?>\n')
  sln:write('<Workspace\n')
  sln:write('\tversion = "1.0">\n')

  for _, proj in ipairs(projects) do
    local name = proj.Decl.Name
    local fname = proj.RelativeFilename
    if fname == '.' then fname = '' 
    else fname = fname ..'/' 
    end
    sln:write('\t<FileRef\n')
    sln:write('\t\tlocation = "group:', name .. '.xcodeproj">\n')
    sln:write('\t</FileRef>\n')
  end

  sln:write('</Workspace>\n')
end

local project_types = util.make_lookup_table {
  "Program", "SharedLibrary", "StaticLibrary", 
}

local function get_absolute_output_path(env)
  local base_dir = env:interpolate('$(OBJECTROOT)$(SEP)') 
  local cwd = native.getcwd()
  return cwd .. "/" .. base_dir
end

local function newid(data)
  local string = native.digest_guid(data) 
  -- a bit ugly but is to match the xcode style of UIds
  return string.sub(string.gsub(string, '-', ''), 1, 24)
end

local function getfiletype(name)
  local types = {
    [".c"]         = "sourcecode.c.c",
    [".cc"]        = "sourcecode.cpp.cpp",
    [".cpp"]       = "sourcecode.cpp.cpp",
    [".css"]       = "text.css",
    [".cxx"]       = "sourcecode.cpp.cpp",
    [".framework"] = "wrapper.framework",
    [".gif"]       = "image.gif",
    [".h"]         = "sourcecode.c.h",
    [".html"]      = "text.html",
    [".lua"]       = "sourcecode.lua",
    [".m"]         = "sourcecode.c.objc",
    [".mm"]        = "sourcecode.cpp.objc",
    [".nib"]       = "wrapper.nib",
    [".pch"]       = "sourcecode.c.h",
    [".plist"]     = "text.plist.xml",
    [".strings"]   = "text.plist.strings",
    [".xib"]       = "file.xib",
    [".icns"]      = "image.icns",
    [""]           = "compiled.mach-o.executable",
  }
  return types[path.get_extension(name)] or "text"
end


local function get_project_data(unit, env)
  local decl = unit.Decl

  if decl.Name and project_types[unit.Keyword] then

    local relative_fn = decl.Name
    local sources = util.flatten(decl.Sources) or {}
    sources = util.filter(sources, function (x) return type(x) == "string" end)

    if decl.SourceDir then
      sources = util.map(sources, function (x) return decl.SourceDir .. x end)
    end

    local source_list = {}

    -- Rebuild source list with ids that is needed by the xcode project layout
    for _, fn in ipairs(sources) do
      source_list[newid(fn)] = fn
    end

    return {
      Type = unit.Keyword,
      Decl = decl,
      Sources = source_list,
      RelativeFilename = relative_fn,
      Guid = newid(decl.Name .. "ProjectId"),
    }

  elseif unit.Keyword == "OsxBundle" then

    decl.Name = "OsxBundle"

    local source_list = {}

    source_list[newid(decl.InfoPList)] = decl.InfoPList

    for _, resource in ipairs(decl.Resources) do
      if resource.Decl then
        source_list[newid(resource.Decl.Source)] = resource.Decl.Source
      end
    end

    return {
      Type = unit.Keyword,
      Decl = decl,
      Sources = source_list,
      RelativeFilename = "$(OBJECTDIR)/MyApp.app",
      Guid = newid("OsxBundle"),
    }
  else
    return nil
  end
end

local function sort_filelist(source_list)
  local dest = {}
  for k, v in pairs(source_list) do table.insert(dest, { Key = k, Value = v }) end
  table.sort(dest, function(a, b) return a.Value < b.Value end)
  return dest
end

local function write_file_refs(p, projects)
  p:write('/* Begin FBXFileReference section */\n')
  local cwd = native.getcwd();

  -- build the source list

  local full_source_list = {}

  for _, project in pairs(projects) do
    local sources = project.Sources
    for key, fn in pairs(sources) do
      full_source_list[key] = fn
    end

    -- include executable names in the source list as well

    if project.Type == "Program" then 
      full_source_list[newid(project.Decl.Name .. "Program")] = project.Decl.Name
    end
  end

  local source_list = {}

  -- As we can't sort hashtables we need to move this over to a regular table

  source_list = sort_filelist(full_source_list)

  for _, entry in pairs(source_list) do
    local key = entry.Key
    local fn = entry.Value 
    local name = path.get_filename(fn) 
    local file_type = getfiletype(fn)
    local str = ""
    if file_type == "compiled.mach-o.executable" then
      str = string.format('\t\t%s /* %s */ = {isa = PBXFileReference; fileEncoding = 4; lastKnownFileType = %s; name = "%s"; includeInIndex = 0; path = "%s"; sourceTree = BUILT_PRODUCTS_DIR; };',
                key, fn, file_type, name, fn)
    else
      str = string.format('\t\t%s /* %s */ = {isa = PBXFileReference; fileEncoding = 4; lastKnownFileType = %s; name = "%s"; path = "%s"; sourceTree = "<group>"; };',
                key, fn, file_type, name, path.join(cwd, fn))
    end
    p:write(str, '\n')
  end

  p:write('/* End FBXFileReference section */\n\n')
end

local function write_legacy_targets(p, projects, env)
  p:write('/* Begin PBXLegacyTarget section */\n')

  local script_path = get_absolute_output_path(env)

  for _, project in pairs(projects) do
    local decl = project.Decl

    if project.IsMeta then
      --[[
      isa = PBXLegacyTarget;
      buildArgumentsString = "";
      buildConfigurationList = D7D12762170E4CF98A79B5EF /* Build configuration list for PBXLegacyTarget "!UpdateWorkspace" */;
      buildPhases = (
      );
      buildToolPath = /Users/danielcollin/unity_ps3/ps3/Projects/JamGenerated/_workspace.xcode_/updateworkspace;
      dependencies = (
      );
      name = "!UpdateWorkspace";
      passBuildSettingsInEnvironment = 1;
      productName = "!UpdateWorkspace";
      --]]

      p:write('\t\t', newid(decl.Name .. "Target"), ' /* ', decl.Name, ' */ = {\n')
      p:write('\t\t\tisa = PBXLegacyTarget;\n')
      p:write('\t\t\tbuildArgumentsString = "', project.MetaData.BuildArgs, '";\n') 
      p:write('\t\t\tbuildConfigurationList = ', newid(decl.Name .. 'Config'), ' /* Build configuration list for PBXLegacyTarget "',decl.Name, '" */;\n')
      p:write('\t\t\tbuildPhases = (\n')
      p:write('\t\t\t);\n');
      p:write('\t\t\tbuildToolPath = ', script_path .. project.MetaData.BuildTool, ';\n')
      p:write('\t\t\tdependencies = (\n\t\t\t);\n')
      p:write('\t\t\tname = "', decl.Name, '";\n') 
      p:write('\t\t\tpassBuildSettingsInEnvironment = 1;\n')
      p:write('\t\t\tproductName = "', decl.Name or "", '";\n')
      p:write('\t\t};\n')
    end
  end

  p:write('/* End PBXLegacyTarget section */\n')
end

local function write_native_targes(p, projects)
  p:write('/* Begin PBXNativeTarget section */\n')

  local categories = {
    ["Program"] = "com.apple.product-type.tool",
    ["StaticLibrary"] = "com.apple.product-type.library.static",
    ["SharedLibrary"] = "com.apple.product-type.library.dynamic",
  }

  for _, project in pairs(projects) do
    local decl = project.Decl

    if not project.IsMeta then
      p:write('\t\t', newid(decl.Name .. "Target"), ' /* ', decl.Name, ' */ = {\n')
      p:write('\t\t\tisa = PBXNativeTarget;\n')
      p:write('\t\t\tbuildConfigurationList = ', newid(decl.Name .. 'Config'), ' /* Build configuration list for PBXNativeTarget "',decl.Name, '" */;\n')
      p:write('\t\t\tbuildPhases = (\n')
      p:write('\t\t\t\t', newid(decl.Name .. "ShellScript"), ' /* ShellScript */,\n') 
      p:write('\t\t\t);\n');
      p:write('\t\t\tbuildRules = (\n\t\t\t);\n')
      p:write('\t\t\tdependencies = (\n\t\t\t);\n')
      p:write('\t\t\tname = "', decl.Name, '";\n') 
      p:write('\t\t\tProductName = "', decl.Name, '";\n') 
      p:write('\t\t\tproductReference = ', newid(decl.Name .. "Program"), ' /* ', decl.Name, ' */;\n ')
      p:write('\t\t\tproductType = "', categories[project.Type] or "", '";\n')
      p:write('\t\t};\n')
    end
  end

  p:write('/* End PBXNativeTarget section */\n')
end


local function write_header(p)
  p:write('// !$*UTF8*$!\n')
  p:write('{\n')
  p:write('\tarchiveVersion = 1;\n')
  p:write('\tclasses = {\n')
  p:write('\t};\n')
  p:write('\tobjectVersion = 45;\n')
  p:write('\tobjects = {\n')
  p:write('\n')
end

local function get_projects(raw_nodes, env)
  local projects = {}

  local source_list = {}
  source_list[newid("tundra.lua")] = "tundra.lua"
  local units = io.open("units.lua")
  if units then
    source_list[newid("units.lua")] = "units.lua"
    io.close(units)
  end

  local meta_name = "!BuildWorkspace"

   projects[#projects + 1] = {
     Decl = { Name = meta_name, },
     Type = "LegacyTarget",
     RelativeFilename = "",
     Sources = source_list,
     Guid = newid(meta_name .. 'ProjectId'),
     IsMeta = true,
     MetaData = { BuildArgs = "'' $(CONFIG) $(VARIANT) $(SUBVARIANT) $(ACTION)",
            BuildTool = "xcodetundra" },
   }

  local meta_name = "!UpdateWorkspace"

   projects[#projects + 1] = {
     Decl = { Name = "!UpdateWorkspace", },
     Type = "LegacyTarget", 
     RelativeFilename = "",
     Sources = source_list, 
     Guid = newid(meta_name .. 'ProjectId'),
     IsMeta = true,
     MetaData = { BuildArgs = "",
            BuildTool = "xcodeupdateproj" },
   }

  for _, unit in ipairs(raw_nodes) do
    local data = get_project_data(unit, env)
    if data then projects[#projects + 1] = data; end
  end

  return projects
end

local function split(fn)
  local dir, file = fn:match("^(.*)[/\\]([^\\/]*)$")
  if not dir then
    return ".", fn
  else
    return dir, file
  end
end

local function split_str(str, pat, name)
   local t = {}  -- NOTE: use {n = 0} in Lua-5.0
   local fpat = "(.-)" .. pat
   local last_end = 1
   local s, e, cap = str:find(fpat, 1)
   table.insert(t,name)
   while s do
      if s ~= 1 or cap ~= "" then
   table.insert(t,cap)
      end
      last_end = e+1
      s, e, cap = str:find(fpat, last_end)
   end
   if last_end <= #str then
      cap = str:sub(last_end)
      table.insert(t, cap)
   end
   return t
end

local function build_name_id(entry, offset, end_count)
  local entryname = ""
  for p = offset, end_count, 1 do
    if entry[p] ~= nil then
      entryname = entryname .. entry[p]
    end
  end
  return newid(entryname) 
end


local function make_indent(level)
  local indent = '\t';
  for i=1, level, 1 do
    indent = indent .. '\t'
  end
  return indent
end

local function make_full_path( grp )
  
  local full_path_string = grp.Name
  local gparent = grp.Parent
  while gparent ~= nil do
    full_path_string = gparent.Name ..'/'..full_path_string 
    gparent = gparent.Parent
  end
  return full_path_string .. ' : ' .. grp.Key
end

local function write_group_ref(p, g, full_path)

  p:write('\t\t', g.Key, ' /* ', full_path .. '/' .. g.Name, ' */ = {\n')
  p:write('\t\t\tisa = PBXGroup;\n')
  p:write('\t\t\tchildren = (\n')

  local dirs = {}
  local files = {}

  for _, ref in pairs(g.Children) do
    if ref.IsDir then 
      local key = ref.Key
      dirs[#dirs + 1] = { Key = key, Name = ref.Name }
    else
      local key = ref.Key
      files[#files + 1] = { Key = key, Name = ref.Name }
    end
  end
  
  table.sort(dirs, function(a, b) return a.Name < b.Name end)
  table.sort(files, function(a, b) return a.Name < b.Name end)

  for _, ref in pairs(dirs) do
    p:write(string.format('\t\t\t\t%s /* %s */,\n', ref.Key, full_path .. '/' .. ref.Name))
  end

  for _, ref in pairs(files) do
    p:write(string.format('\t\t\t\t%s /* %s */,\n', ref.Key, full_path .. '/' .. ref.Name))
  end

  p:write('\t\t\t);\n')
  p:write('\t\t\tname = "', g.Name, '"; \n');
  p:write('\t\t\tsourceTree = "<group>";\n');
  p:write('\t\t};\n')
end

local function print_children_2(p, children, path, level)
  if children == nil then
    return path
  end

  local c
  local local_path = ''--path
  for _, c in pairs(children) do
    local indent = make_indent(level)
    local_path = print_children_2( p, c.Children, path .. '/' .. c.Name, level + 1 )
    if #c.Children ~= 0 then 
      write_group_ref(p, c, path)
    end
    
  end

  return path
end

local function find_group(groups, group, parent)
  if groups == nil then return nil end
  for _, g in pairs(groups) do
    if g.Name == group and g.Parent == parent then
      return g
    end
    local r = find_group( g.Children, group, parent )
    if r ~= nil then return r end
  end
  return nil
end

local function write_sources(p, children, name, parent)

  local filelist = sort_filelist(children)
  local groups = {};

  table.insert(groups, {Name = name, Parent = nil, Key = parent, Children = {} })
  
  for _, entry in pairs(filelist) do
    local parent_group = nil
    local path, filename = split(entry.Value)
    local split_path = split_str(path, "/", name)
    for i=1  , #split_path, 1 do
      if split_path[i] ~= '.' then 
        local grp = find_group(groups, split_path[i], parent_group) 
        if grp == nil then
          grp = { IsDir = true, Name=split_path[i], Parent=parent_group, Key=newid(util.tostring(parent_group)..split_path[i]), Children={} }
          if parent_group == nil then
            table.insert(groups, grp)
          else
            parent_group = grp.Parent
            table.insert(parent_group.Children, grp)
          end
        end
        parent_group = grp
      end
    end
    if parent_group ~= nil then
      table.insert(parent_group.Children, { IsDir = false, Name=filename, Parent=parent_group, Key = entry.Key, Children = {}} )
    end
  end

  print_children_2(p, groups, '.', 0);
end


local function write_groups(p, projects)
  p:write('/* Begin PBXGroup section */\n')

  local all_targets_name = "AllTargets.workspace"
  local all_targets_id = newid(all_targets_name)

  for _, project in pairs(projects) do
    write_sources(p, project.Sources, project.Decl.Name, project.Guid)
  end

  -- write last group that links the projects names above

  p:write('\t\t', all_targets_id, ' /* ', all_targets_name, ' */ = {\n')
  p:write('\t\t\tisa = PBXGroup;\n')
  p:write('\t\t\tchildren = (\n')

  for _, project in pairs(projects) do
    p:write(string.format('\t\t\t\t%s /* %s */,\n', project.Guid, project.Decl.Name))
  end
  p:write('\t\t\t);\n')
  p:write('\t\t\tname = "', all_targets_name, '"; \n');
  p:write('\t\t\tsourceTree = "<group>";\n');
  p:write('\t\t};\n')

  p:write('/* End PBXGroup section */\n\n')
end

local function write_project(p, projects)

  local all_targets_name = "AllTargets.workspace"
  local all_targets_id = newid(all_targets_name)

  local project_id = newid("ProjectObject")
  local project_config_list_id = newid("ProjectObjectConfigList")

  p:write('/* Begin PBXProject section */\n')
  p:write('\t\t', project_id, ' /* Project object */ = {\n')
  p:write('\t\t\tisa = PBXProject;\n')
  p:write('\t\t\tbuildConfigurationList = ', project_config_list_id, ' /* Build configuration list for PBXProject "', "Project Object", '" */;\n')
  p:write('\t\t\tcompatibilityVersion = "Xcode 3.1";\n')
  p:write('\t\t\thasScannedForEncodings = 1;\n')
  p:write('\t\t\tmainGroup = ', all_targets_id, ' /* ', all_targets_name, ' */;\n')
  p:write('\t\t\tprojectDirPath = "";\n')
  p:write('\t\t\tprojectRoot = "";\n')
  p:write('\t\t\ttargets = (\n')

  for _, project in pairs(projects) do
    p:write(string.format('\t\t\t\t%s /* %s */,\n', newid(project.Decl.Name .. "Target"), project.Decl.Name))
  end

  p:write('\t\t\t);\n')
  p:write('\t\t};\n')
  p:write('/* End PBXProject section */\n')
end

local function write_shellscripts(p, projects, env)
  p:write('/* Begin PBXShellScriptBuildPhase section */\n')

  -- TODO: Do we really need to repead this for all projects? seems a bit wasteful

  local xcodetundra_filename = get_absolute_output_path(env) .. "xcodetundra"

  for _, project in pairs(projects) do
    local name = project.Decl.Name
    if not project.IsMeta then
      p:write('\t\t', newid(name .. "ShellScript"), ' /* ShellScript */ = {\n')
      p:write('\t\t\tisa = PBXShellScriptBuildPhase;\n')
      p:write('\t\t\tbuildActionMask = 2147483647;\n')
      p:write('\t\t\tfiles = (\n')
      p:write('\t\t\t);\n')
      p:write('\t\t\tinputPaths = (\n')
      p:write('\t\t\t);\n')
      p:write('\t\t\toutputPaths = (\n')
      p:write('\t\t\t);\n')
      p:write('\t\t\trunOnlyForDeploymentPostprocessing = 0;\n')
      p:write('\t\t\tshellPath = /bin/sh;\n')
      p:write('\t\t\tshellScript = "', xcodetundra_filename, ' $TARGET_NAME $CONFIG $VARIANT $SUBVARIANT $ACTION -v";\n')
      p:write('\t\t};\n')
    end
  end

  p:write('/* Begin PBXShellScriptBuildPhase section */\n')
end

local function get_full_config_name(config)
  return config.Config.Name .. '-' .. config.Variant.Name .. '-' .. config.SubVariant
end

local function write_configs(p, projects, config_tuples, env)

  p:write('/* Begin XCConfigurationList section */\n')

  -- I wonder if we really need to do it this way for all configs?

  for __, project in ipairs(projects) do
    for _, tuple in ipairs(config_tuples) do
      local full_config_name = get_full_config_name(tuple)

      local is_macosx_native = false

      for _, host in util.nil_ipairs(tuple.Config.SupportedHosts) do
        if host == "macosx" then
          is_macosx_native = true
        end
      end

      if "macosx" == tuple.Config.DefaultOnHost then
        is_macosx_native = true
      end

      local config_id = newid(project.Decl.Name .. full_config_name)

      p:write('\t\t', config_id, ' = {\n')
      p:write('\t\t\tisa = XCBuildConfiguration;\n')

      -- Don't add any think extra if subvariant is default

      p:write('\t\t\tbuildSettings = {\n')

      if is_macosx_native then 
        p:write('\t\t\t\tARCHS = "$(NATIVE_ARCH_ACTUAL)";\n') 
      end

      p:write('\t\t\t\tVARIANT = "', tuple.Variant.Name, '";\n') 
      p:write('\t\t\t\tCONFIG = "', tuple.Config.Name, '";\n')
      p:write('\t\t\t\tSUBVARIANT = "', tuple.SubVariant, '";\n')

      if is_macosx_native and not project.IsMeta then 
        p:write('\t\t\t\tCONFIGURATION_BUILD_DIR = "', full_config_name, '";\n') 
      end

      -- this is a little hack to get xcode to clean the whole output folder when using "FullBuild"
      p:write('\t\t\t\tPRODUCT_NAME = "',project.Decl.Name , '";\n')
      p:write('\t\t\t\tTARGET_NAME = "',project.Decl.Name , '";\n')

      p:write('\t\t\t};\n')
      p:write('\t\t\tname = "',full_config_name , '";\n')
      p:write('\t\t};\n')
    end
  end

  p:write('/* End XCConfigurationList section */\n')

end

local function write_config_list(p, projects, config_tuples)

  p:write('/* Begin XCConfigurationList section */\n')

  local default_config = "";

  -- find the default config

  for _, tuple in ipairs(config_tuples) do
    local is_macosx_native = tuple.Config.Name:match('^(%macosx)%-')

    if is_macosx_native and tuple.Variant.Name == "debug" then
      default_config = get_full_config_name(tuple)
      break
    end
  end

  -- if we did't find a default config just grab the first one

  if default_config == "" then
    default_config = get_full_config_name(config_tuples[0])
  end

  for __, project in ipairs(projects) do

    local config_id = newid(project.Decl.Name .. 'Config')

    p:write('\t\t', config_id, ' /* Build config list for "', project.Decl.Name, '" */ = {\n')
    p:write('\t\t\tisa = XCConfigurationList;\n')

    -- Don't add any think extra if subvariant is default

    p:write('\t\t\tbuildConfigurations = (\n')
  
    for _, tuple in ipairs(config_tuples) do
      local full_config_name = get_full_config_name(tuple)
      p:write(string.format('\t\t\t\t%s /* %s */,\n', newid(project.Decl.Name .. full_config_name), full_config_name))
    end

    p:write('\t\t\t);\n')
    p:write('\t\t\tdefaultConfigurationIsVisible = 1;\n')
    p:write('\t\t\tdefaultConfigurationName = "', default_config, '";\n')

    p:write('\t\t};\n')
  end

  p:write('/* End XCConfigurationList section */\n')

end

local function write_footer(p)
  p:write('\t};\n')
  p:write('\trootObject = ', newid("ProjectObject"), ' /* Project object */;\n')
  p:write('}\n')
end

local function generate_shellscript(env)
  local filename = path.join(get_absolute_output_path(env), "xcodetundra")
  local p = assert(io.open(filename, 'wb'))
  p:write("#/bin/sh\n")
  p:write("TARGET_NAME=$1\n")
  p:write("CONFIG=$2\n")
  p:write("VARIANT=$3\n")
  p:write("SUBVARIANT=$4\n")
  p:write("ACTION=$5\n")
  p:write('if [ "$5" = "clean" ]; then\n')
  p:write('    ACTION="-c"\n')
  p:write("fi\n\n")
  p:write('if [ "$5" = "build" ]; then\n')
  p:write('    ACTION=""\n')
  p:write("fi\n\n")
  p:write(TundraExePath .. " --full-paths $TARGET_NAME $CONFIG-$VARIANT-$SUBVARIANT $ACTION -v\n")
  p:close()
  os.execute("chmod +x " .. filename)
  local filename = path.join(get_absolute_output_path(env), "xcodeupdateproj")
  local p = io.open(filename, 'wb')
  p:write("#/bin/sh\n")
  p:write(TundraExePath .. " --ide-gen xcode3 -a\n")
  p:close()
  os.execute("chmod +x " .. filename)
end

function xcode_generator:generate_files(ngen, config_tuples, raw_nodes, env, default_names)
  assert(config_tuples and #config_tuples > 0)

  -- TODO: Set the first default config as default

  local base_dir = env:interpolate('$(OBJECTROOT)$(SEP)') 
  local xcodeproj_dir = base_dir .. "tundra-generated.xcodeproj/"

  native.mkdir(base_dir)
  native.mkdir(xcodeproj_dir) 

  generate_shellscript(env)

  local p = io.open(path.join(xcodeproj_dir, "project.pbxproj"), 'wb')

  local projects = get_projects(raw_nodes, env) 

  write_header(p)
  write_file_refs(p, projects)
  write_groups(p, projects)
  write_legacy_targets(p, projects, env)
  write_native_targes(p, projects)
  write_project(p, projects)
  write_shellscripts(p, projects, env)
  write_configs(p, projects, config_tuples, env)
  write_config_list(p, projects, config_tuples)
  write_footer(p)
end

nodegen.set_ide_backend(function(...)
  local state = setmetatable({}, xcode_generator)
  state:generate_files(...)
end)

