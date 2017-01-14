
module(..., package.seeall)

local vscommon = require "tundra.tools.msvc-vscommon"

function apply(env, options)
  vscommon.apply_msvc_visual_studio("12.0", env, options)
end
