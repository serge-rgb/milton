-- msvc-winsdk.lua - Use Microsoft Windows SDK 7.1 or later to build.

module(..., package.seeall)

local native = require "tundra.native"
local os = require "os"

if native.host_platform ~= "windows" then
  error("the msvc toolset only works on windows hosts")
end

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

local compiler_dirs = {
  ["x86"] = {
    ["x86"] = "bin\\",
    ["x64"] = "bin\\x86_amd64\\",
    ["itanium"] = "bin\\x86_ia64\\",
  },
  ["x64"] = {
    ["x86"] = "bin\\",
    ["x64"] = {
      ["11.0"] = "bin\\x86_amd64\\",
      "bin\\amd64\\"
    },
    ["itanium"] = "bin\\x86_ia64\\",
  },
  ["itanium"] = {
    ["x86"] = "bin\\x86_ia64\\",
    ["itanium"] = "bin\\ia64\\",
  },
}

local function setup(env, options)
  options = options or {}
  local target_arch = options.TargetArch or "x86"
  local host_arch = options.HostArch or get_host_arch()
  local vcversion = options.VcVersion or "10.0"

  local binDir =
    compiler_dirs[host_arch][target_arch][vcversion]
    or compiler_dirs[host_arch][target_arch][1]
    or compiler_dirs[host_arch][target_arch]

  if not binDir then
    errorf("can't build target arch %s on host arch %s", target_arch, host_arch)
  end

  local sdkDir;
  local sdkDirIncludes;
  local sdkLibDir;
  local vcLibDir;

  if vcversion == "11.0" then
    local sdk_key = "SOFTWARE\\Microsoft\\Microsoft SDKs\\Windows\\v8.0"
    sdkDir = assert(native.reg_query("HKLM", sdk_key, "InstallationFolder"))
    sdkDirIncludes = { sdkDir .. "\\INCLUDE\\UM", sdkDir .. "\\INCLUDE\\SHARED" }

    sdkLibDir = "LIB\\win8\\um\\"
    vcLibDir = "LIB"

    if "x86" == target_arch then
      sdkLibDir = sdkLibDir .. "x86"
    elseif "x64" == target_arch then
      sdkLibDir = sdkLibDir .. "x64"
      vcLibDir = "LIB\\amd64"
    elseif "arm" == target_arch then
      sdkLibDir = sdkLibDir .. "arm"
    end
  else
    local sdk_key = "SOFTWARE\\Microsoft\\Microsoft SDKs\\Windows"
    sdkDir = assert(native.reg_query("HKLM", sdk_key, "CurrentInstallFolder"))
    sdkDirIncludes = { sdkDir .. "\\INCLUDE" };

    sdkLibDir = "LIB"
    vcLibDir = "LIB"

    if "x64" == target_arch then
      sdkLibDir = "LIB\\x64"
      vcLibDir = "LIB\\amd64"
    elseif "itanium" == target_arch then
      sdkLibDir = "LIB\\IA64"
      vcLibDir = "LIB\\IA64"
    end
  end


  local vc_key = "SOFTWARE\\Microsoft\\VisualStudio\\SxS\\VC7"
  local vc_dir = assert(native.reg_query("HKLM", vc_key, vcversion))
  if vc_dir:sub(-1) ~= '\\' then
    vc_dir = vc_dir .. '\\'
  end

  local cl_exe = '"' .. vc_dir .. binDir .. "cl.exe" ..'"'
  local lib_exe = '"' .. vc_dir .. binDir .. "lib.exe" ..'"'
  local link_exe = '"' .. vc_dir .. binDir .. "link.exe" ..'"'

  env:set('CC', cl_exe)
  env:set('CXX', cl_exe)
  env:set('LIB', lib_exe)
  env:set('LD', link_exe)

  -- Set up the MS SDK associated with visual studio

  env:set_external_env_var("WindowsSdkDir", sdkDir)
  env:set_external_env_var("INCLUDE", table.concat(sdkDirIncludes, ";") .. ";" .. vc_dir .. "\\INCLUDE")

  local rc_exe
  print("vcversion",  vcversion)
  if vcversion == "11.0" then
    rc_exe = '"' .. sdkDir .. "\\bin\\x86\\rc.exe" ..'"'
  else
    rc_exe = '"' .. sdkDir .. "\\bin\\rc.exe" ..'"'
  end
  env:set('RC', rc_exe)

  local libString = sdkDir .. "\\" .. sdkLibDir .. ";" .. vc_dir .. "\\" .. vcLibDir
  env:set_external_env_var("LIB", libString)
  env:set_external_env_var("LIBPATH", libString)

  local path = { }
  local vc_root = vc_dir:sub(1, -4)
  if binDir ~= "\\bin\\" then
    path[#path + 1] = vc_dir .. "\\bin"
  end
  path[#path + 1] = vc_root .. "Common7\\Tools" -- drop vc\ at end
  path[#path + 1] = vc_root .. "Common7\\IDE" -- drop vc\ at end
  path[#path + 1] = sdkDir
  path[#path + 1] = vc_dir .. binDir
  path[#path + 1] = env:get_external_env_var('PATH')

  env:set_external_env_var("PATH", table.concat(path, ';'))
end

function apply(env, options)
  -- Load basic MSVC environment setup first. We're going to replace the paths to
  -- some tools.
  tundra.unitgen.load_toolset('msvc', env)
  setup(env, options)
end
