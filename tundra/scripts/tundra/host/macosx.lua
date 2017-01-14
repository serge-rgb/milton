module(..., package.seeall)

function apply_host(env)
  env:set_many {
    ["DOTNETRUN"] = "mono ",
    ["HOSTPROGSUFFIX"] = "",
    ["HOSTSHLIBSUFFIX"] = ".dylib",
    ["_COPY_FILE"] = "cp -f $(<) $(@)",
    ["_HARDLINK_FILE"] = "ln -f $(<) $(@)",
  }
end
