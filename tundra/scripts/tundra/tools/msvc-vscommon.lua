-- msvc-vscommon.lua - utility code for all versions of Visual Studio

module(..., package.seeall)

local native = require "tundra.native"
local os = require "os"

-- Visual Studio tooling layout

local vc_bin_map = {
  ["x86"] = {
    ["x86"] = "",
    ["x64"] = "x86_amd64",
    ["arm"] = "x86_arm",
  },
  ["x64"] = {
    ["x86"] = "",
    ["x64"] = "amd64",
    ["arm"] = "x86_arm", -- is this really legal?
  },
}

local vc_lib_map = {
  ["x86"] = {
    ["x86"] = "",
    ["x64"] = "amd64",
    ["arm"] = "arm",
  },
  ["x64"] = {
    ["x86"] = "",
    ["x64"] = "amd64",
    ["arm"] = "arm",
  },
}

-- Windows SDK layout

local pre_win8_sdk_dir = {
  ["bin"] = "bin",
  ["include"] = "include",
  ["lib"] = "lib",
}

local win8_sdk_dir = {
  ["bin"] = "bin",
  ["include"] = "include",
  ["lib"] = "lib\\win8\\um",
}

local win81_sdk_dir = {
  ["bin"] = "bin",
  ["include"] = "include",
  ["lib"] = "lib\\winv6.3\\um",
}

local pre_win8_sdk = {
  ["x86"] = {
    ["bin"] = "",
    ["include"] = "",
    ["lib"] = "",
  },
  ["x64"] = {
    ["bin"] = "x64",
    ["include"] = "",
    ["lib"] = "x64",
  },
}

local post_win8_sdk = {
  ["x86"] = {
    ["bin"] = "x86",
    ["include"] = { "shared", "um" },
    ["lib"] = "x86",
  },
  ["x64"] = {
    ["bin"] = "x64",
    ["include"] = { "shared", "um" },
    ["lib"] = "x64",
  },
  ["arm"] = {
    ["bin"] = "arm",
    ["include"] = { "shared", "um" },
    ["lib"] = "arm",
  },
}


-- Maps from VS version to default SDK version. Also used to imitate behaviour
-- before this patch, where SDK version was identified by VS version.

local vs_sdk_map = {
  ["9.0"] = "v6.0A",
  ["10.0"] = "v7.0A",
  ["10.1"] = "v7.1A",
  ["11.0"] = "v8.0",
  ["12.0"] = "v8.1",
  -- The current visual studio 2015 download does not include the full windows
  -- 10 SDK, and new Win32 apps created in VS2015 default to using the 8.1 SDK
  ["14.0"] = "v8.1" 
}

-- Each quadruplet specifies a registry key value that gets us the SDK location, 
-- followed by a folder structure (for each supported target architecture) 
-- and finally the corresponding bin, include and lib folder's relative location

local pre_win10_sdk_map = {
  ["v6.0A"] = { "SOFTWARE\\Microsoft\\Microsoft SDKs\\Windows\\v6.0A", "InstallationFolder", pre_win8_sdk_dir, pre_win8_sdk },
  ["v7.0A"] = { "SOFTWARE\\Microsoft\\Microsoft SDKs\\Windows\\v7.0A", "InstallationFolder", pre_win8_sdk_dir, pre_win8_sdk },
  ["V7.1A"] = { "SOFTWARE\\Microsoft\\Microsoft SDKs\\Windows\\v7.1A", "InstallationFolder", pre_win8_sdk_dir, pre_win8_sdk },  
  ["v8.0"] = { "SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots", "KitsRoot", win8_sdk_dir, post_win8_sdk },
  ["v8.1"] = { "SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots", "KitsRoot81", win81_sdk_dir, post_win8_sdk }
}

local win10_sdk = {
  "SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots", "KitsRoot10"
}


local function get_host_arch()
  local snative = native.getenv("PROCESSOR_ARCHITECTURE")
  local swow = native.getenv("PROCESSOR_ARCHITEW6432", "")
  if snative == "AMD64" or swow == "AMD64" then
    return "x64"
  elseif snative == "IA64" or swow == "IA64" then
    return "itanium";
  else
    return "x86"
  end
end

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

function get_pre_win10_sdk(sdk_version, vs_version, target_arch)
  local result = {
    ["include"] = {},
  }

  local sdk = pre_win10_sdk_map[sdk_version]
  assert(sdk, "The requested version of Visual Studio isn't supported")

  local sdk_root = native.reg_query("HKLM", sdk[1], sdk[2])
  assert(sdk_root, "The requested version of the SDK isn't installed")
  sdk_root = string.gsub(sdk_root, "\\+$", "\\")
  
  local sdk_dir_base = sdk[3]

  local sdk_dir = sdk[4][target_arch]
  assert(sdk_dir, "The target platform architecture isn't supported by the SDK")

  result.bin = sdk_root .. sdk_dir_base["bin"] .. "\\" .. sdk_dir["bin"]

  local sdk_dir_base_include = sdk_dir_base["include"]
  for _, v in path_it(sdk_dir["include"]) do
    result.include[#result.include + 1] = sdk_root .. sdk_dir_base_include .. "\\" .. v
  end

  result.lib_str = sdk_root .. sdk_dir_base["lib"] .. "\\" .. sdk_dir["lib"]
  result.root = sdk_root

  -- Windows 10 changed CRT to be split between Windows SDK and VC. It
  -- appears that when targeting pre-win10 with VS2015 you should always use
  -- use 10.0.10150.0, according to Microsoft.Cpp.Common.props in MSBuild.
  if vs_version == "14.0" then
    local win10_sdk_root = native.reg_query("HKLM", win10_sdk[1], win10_sdk[2])
    assert(win10_sdk_root, "The windows 10 UCRT is required when building using Visual studio 2015")
    result.include[#result.include + 1] = win10_sdk_root .. "Include\\10.0.10150.0\\ucrt"
    result.lib_str = result.lib_str .. ";" .. win10_sdk_root .. "Lib\\10.0.10150.0\\ucrt\\" .. post_win8_sdk[target_arch].lib
  end

  return result
end

function get_win10_sdk(sdk_version, vs_version, target_arch)
  -- Remove v prefix
  sdk_version = string.sub(sdk_version, 2, -1)

  -- This only checks if the windows 10 SDK specifically is installed. A
  -- 'dir exists' method would be needed here to check if a specific SDK
  -- target folder exists.
  local sdk_root = native.reg_query("HKLM", win10_sdk[1], win10_sdk[2])
  assert(sdk_root, "The requested version of the SDK isn't installed")

  local result = {
    ["include"] = {}
  }

  result.bin = sdk_root .. "bin\\" .. post_win8_sdk[target_arch].bin

  local sdk_dir_base_include = sdk_root .. "include\\" .. sdk_version .. "\\"
  result.include[#result.include + 1] = sdk_dir_base_include .. "shared"
  result.include[#result.include + 1] = sdk_dir_base_include .. "ucrt"
  result.include[#result.include + 1] = sdk_dir_base_include .. "um"

  local sdk_dir_base_lib = sdk_root .. "Lib\\" .. sdk_version .. "\\"
  result.lib_str = sdk_dir_base_lib .. "ucrt\\" .. post_win8_sdk[target_arch].lib
  result.lib_str = result.lib_str .. ";" .. sdk_dir_base_lib .. "um\\" .. post_win8_sdk[target_arch].lib

  result.root = sdk_root

  return result
end

function get_sdk(sdk_version, vs_version, target_arch)
  -- All versions using v10.0.xxxxx.x use specific releases of the
  -- Win10 SDK. Other versions are assumed to be pre-win10
  if string.sub(sdk_version, 1, 6) == "v10.0." then
    return get_win10_sdk(sdk_version, vs_version, target_arch)
  else
    return get_pre_win10_sdk(sdk_version, vs_version, target_arch)
  end
end


function apply_msvc_visual_studio(version, env, options)

  -- NOTE:  don't make changes to  `env` until you've asserted
  --        that the requested version is in fact installed, 
  --        the `vs-wild` toolset will call this function
  --        repeatedly with a the next version but the same `env`, 
  --        if a version fails (assert/error)

  if native.host_platform ~= "windows" then
    error("the msvc toolset only works on windows hosts")
  end
  
  -- Load basic MSVC environment setup first.
  -- We're going to replace the paths to some tools.
  tundra.unitgen.load_toolset('msvc', env)

  options = options or {}

  local target_arch = options.TargetArch or "x86"
  local host_arch = options.HostArch or get_host_arch()

  -- SDKs are identified by SdkVersion or vs version
  -- each VS version defines a default SDK to use.
  local sdk_version = options.SdkVersion or version 
  sdk_version = vs_sdk_map[sdk_version] or sdk_version

  -- We'll find any edition of VS (including Express) here
  local vs_root = native.reg_query("HKLM", "SOFTWARE\\Microsoft\\VisualStudio\\SxS\\VS7", version)
  if vs_root == nil then
    -- This is necessary for supporting the "Visual C++ Build Tools", which includes only the Compiler & SDK (not Visual Studio)
    local vc_root = native.reg_query("HKLM", "SOFTWARE\\Microsoft\\VisualStudio\\SxS\\VC7", version)
    if vc_root ~= nil then
      vs_root = string.gsub(vc_root, "\\VC\\$", "\\")
    end
  end
  assert(vs_root, "Visual Studio [Version " .. version .. "] isn't installed. To use a different Visual Studio version, edit tundra.lua accordingly")
  vs_root = string.gsub(vs_root, "\\+$", "\\")

  local vc_lib
  local vc_bin
  
  vc_bin =  vc_bin_map[host_arch][target_arch]
  if not vc_bin then
    errorf("can't build target arch %s on host arch %s", target_arch, host_arch)
  end
  vc_bin =  vs_root .. "vc\\bin\\" .. vc_bin
  
  vc_lib =  vs_root .. "vc\\lib\\" .. vc_lib_map[host_arch][target_arch]

  --
  -- Now fix up the SDK
  --
  local sdk = get_sdk(sdk_version, version, target_arch)  

  
  --
  -- Tools
  --
  local cl_exe = '"' .. path_combine(vc_bin, "cl.exe") .. '"'
  local lib_exe = '"' .. path_combine(vc_bin, "lib.exe") .. '"'
  local link_exe = '"' .. path_combine(vc_bin, "link.exe") .. '"'
  local rc_exe = '"' .. path_combine(sdk.bin, "rc.exe") .. '"' -- pickup the Resource Compiler from the SDK

  env:set('CC', cl_exe)
  env:set('CXX', cl_exe)
  env:set('LIB', lib_exe)
  env:set('LD', link_exe)
  env:set('RC', rc_exe)

  if sdk_version == "9.0" then
    env:set("RCOPTS", "") -- clear the "/nologo" option (it was first added in VS2010)
  end
 
  if version == "12.0" or version == "14.0" then
    -- Force MSPDBSRV.EXE
    env:set("CCOPTS", "/FS")
    env:set("CXXOPTS", "/FS")
  end
 
  -- Wire-up the external environment

  env:set_external_env_var('VSINSTALLDIR', vs_root)
  env:set_external_env_var('VCINSTALLDIR', vs_root .. "\\vc")
  env:set_external_env_var('DevEnvDir', vs_root .. "Common7\\IDE")

  local include = {}

  for _, v in ipairs(sdk.include) do
    include[#include + 1] = v
  end

  include[#include + 1] = vs_root .. "VC\\ATLMFC\\INCLUDE"
  include[#include + 1] = vs_root .. "VC\\INCLUDE"


  -- if MFC isn't installed with VS
  -- the linker will throw an error when looking for libs
  -- Lua does not have a "does directory exist function"
  -- we could use one here
  local lib_str = sdk.lib_str .. ";" .. vs_root .. "\\VC\\ATLMFC\\lib\\" .. vc_lib_map[host_arch][target_arch] .. ";" .. vc_lib

  env:set_external_env_var("WindowsSdkDir", sdk.root)
  env:set_external_env_var("INCLUDE", table.concat(include, ';'))


  env:set_external_env_var("LIB", lib_str)
  env:set_external_env_var("LIBPATH", lib_str)

  -- Modify %PATH%

  local path = {}

  path[#path + 1] = sdk.root
  path[#path + 1] = vs_root .. "Common7\\IDE"

  if "x86" == host_arch then
    path[#path + 1] = vs_root .. "\\VC\\Bin"
  elseif "x64" == host_arch then
    path[#path + 1] = vs_root .. "\\VC\\Bin\\amd64"
  elseif "arm" == host_arch then
    path[#path + 1] = vs_root .. "\\VC\\Bin\\arm"
  end
  
  path[#path + 1] = vs_root .. "\\Common7\\IDE"

  path[#path + 1] = env:get_external_env_var('PATH') 

  env:set_external_env_var("PATH", table.concat(path, ';'))
   
end
