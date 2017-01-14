module(..., package.seeall)

function apply_host(env)
  env:set_many {
    ["DOTNETRUN"] = "",
    ["HOSTPROGSUFFIX"] = ".exe",
    ["HOSTSHLIBSUFFIX"] = ".dll",
    ["_COPY_FILE"] = "copy $(<) $(@)",
    ["_HARDLINK_FILE"] = "copy /f $(<) $(@)",
  }
end
