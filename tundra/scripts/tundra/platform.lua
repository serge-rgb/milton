module(..., package.seeall)

local native = require "tundra.native"

function host_platform()
  return native.host_platform
end
