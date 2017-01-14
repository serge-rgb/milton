module(..., package.seeall)

local error_count = 0

function _G.unit_test(label, fn)
  local t_mt = {
    check_equal = function (obj, a, b) 
      if a ~= b then
        error { Message = "Equality test failed: " .. tostring(a) .. " != " .. tostring(b) }
      end
    end
  }
  t_mt.__index = t_mt

  local t = setmetatable({}, t_mt)
  local function stack_dumper(err_obj)
    if type(err_obj) == "table" then
      return err_obj.Message
    end
    local debug = require 'debug'
    return debug.traceback(err_obj, 2)
  end

  io.stdout:write("Testing ", label, ": ")
  io.stdout:flush()
  local ok, err = xpcall(function () fn(t) end, stack_dumper)
  if not ok then
    io.stdout:write("failed\n")
    io.stdout:write(tostring(err), "\n")
    error_count = error_count + 1
  else
    io.stderr:write("OK\n")
  end
end

require "tundra.test.t_env"
require "tundra.test.t_path"
