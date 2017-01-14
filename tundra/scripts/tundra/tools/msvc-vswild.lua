
module(..., package.seeall)

local vscommon = require "tundra.tools.msvc-vscommon"

function apply(env, options)

  local vsvs = options.VsVersions or { "14.0", "12.0", "11.0", "10.0", "9.0" }

  for _, v in ipairs(vsvs) do
  	local v1 = v
  	local success, result = xpcall(function() vscommon.apply_msvc_visual_studio(v1, env, options) end, function(err) return err end)
  	if success then
  		print("Visual Studio version " .. v1 .. " found ")
  		return
  	else
  		print("Visual Studio version " .. v1 .. " does not appear to be installed (" .. result .. ")")
  	end
  end

  error("Unable to find suitable version of Visual Studio (please install either version " .. table.concat(vsvs, ", ") .. " of Visual Studio to continue)")

end
