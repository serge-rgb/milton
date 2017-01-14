-- msvc6.lua - Visual Studio 6

module(..., package.seeall)

local native = require "tundra.native"
local os = require "os"

function path_combine(path, path_to_append)
  if path == nil then
    return path_to_append
  end
  if path:find("\\$") then
    return path .. path_to_append
  end
  return path .. "\\" .. path_to_append
end

function path_it(maybe_list)
  if type(maybe_list) == "table" then
    return ipairs(maybe_list)
  end
  return ipairs({maybe_list})
end

function apply(env, options)

  if native.host_platform ~= "windows" then
    error("the msvc6 toolset only works on windows hosts")
  end
  
  -- Load basic MSVC environment setup first.
  -- We're going to replace the paths to some tools.
  tundra.unitgen.load_toolset('msvc', env)

  -- Override PCH handling for poor MSVC 6. While it supports writing PDB files
  -- from the compiler, it doesn't handling multiple invocations of the
  -- compiler wanting to write to the same PDB. So we switch to /Z7 format
  -- instead and have just the linker write a PDB.
  env:replace("_USE_PDB_CC_OPT", "/Z7")

  options = options or {}

  -- We'll find any edition of VS (including Express) here
  local vs_root = native.reg_query("HKLM", "SOFTWARE\\Microsoft\\VisualStudio\\6.0\\Setup\\Microsoft Visual C++", "ProductDir")
  assert(vs_root, "The requested version of Visual Studio isn't installed")
  vs_root = string.gsub(vs_root, "\\+$", "\\")

  local common_root = native.reg_query("HKLM", "SOFTWARE\\Microsoft\\VisualStudio\\6.0\\Setup", "VsCommonDir")
  assert(common_root, "The requested version of Visual Studio isn't installed")
  common_root = string.gsub(common_root, "\\+$", "\\")

  local vc_lib
  local vc_bin
  
  vc_bin =  vs_root .. "\\bin"
  vc_lib =  vs_root .. "\\lib"

  -- Tools

  local cl_exe = '"' .. path_combine(vc_bin, "cl.exe") .. '"'
  local lib_exe = '"' .. path_combine(vc_bin, "lib.exe") .. '"'
  local link_exe = '"' .. path_combine(vc_bin, "link.exe") .. '"'
  local rc_exe = '"' .. path_combine(common_root, "MSDev98\\Bin\\rc.exe") .. '"'

  env:set('CC', cl_exe)
  env:set('CXX', cl_exe)
  env:set('LIB', lib_exe)
  env:set('LD', link_exe)
  env:set('RC', rc_exe)

  env:set("RCOPTS", "") -- clear the "/nologo" option (it was first added in VS2010)
 
  -- Wire-up the external environment
  env:set_external_env_var('VSINSTALLDIR', vs_root)
  env:set_external_env_var('VCINSTALLDIR', vs_root .. "\\vc")
  --env:set_external_env_var('DevEnvDir', vs_root .. "Common7\\IDE")

  do
    local include = {
      path_combine(vs_root, "ATL\\INCLUDE"),
      path_combine(vs_root, "INCLUDE"),
      path_combine(vs_root, "MFC\\INCLUDE"),
    }
    env:set_external_env_var("INCLUDE", table.concat(include, ';'))
  end

  do
    local lib = {
      path_combine(vs_root, "LIB"),
      path_combine(vs_root, "MFC\\LIB"),
    }
    local lib_str = table.concat(lib, ';')
    env:set_external_env_var("LIB", lib_str)
    env:set_external_env_var("LIBPATH", lib_str)
  end

  -- Modify %PATH%

  do
    local path = {
      path_combine(vs_root, "BIN"),
      path_combine(common_root, "MSDev98\\BIN"),
      env:get_external_env_var('PATH'),
    }
    env:set_external_env_var("PATH", table.concat(path, ';'))
  end
end
