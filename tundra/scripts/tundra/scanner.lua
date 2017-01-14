module(..., package.seeall)

local util   = require "tundra.util"
local native = require "tundra.native"

local _scanner_mt = {}
setmetatable(_scanner_mt, { __index = _scanner_mt })

local cpp_scanner_cache = {}
local generic_scanner_cache = {}

function make_cpp_scanner(paths)
  local key = table.concat(paths, '\0')

  if not cpp_scanner_cache[key] then
    local data = { Kind = 'cpp', Paths = paths, Index = #cpp_scanner_cache }
    cpp_scanner_cache[key] = setmetatable(data, _scanner_mt)
  end

  return cpp_scanner_cache[key]
end

function make_generic_scanner(data)
  data.Kind = 'generic'
  local mashup = { }
  local function add_all(l)
    for _, value in util.nil_ipairs(l) do
      mashup[#mashup + 1] = value
    end
  end
  add_all(data.Paths)
  add_all(data.Keywords)
  add_all(data.KeywordsNoFollow)
  mashup[#mashup + 1] = '!!'
  mashup[#mashup + 1] = data.RequireWhitespace and 'y' or 'n'
  mashup[#mashup + 1] = data.UseSeparators and 'y' or 'n'
  mashup[#mashup + 1] = data.BareMeansSystem and 'y' or 'n'
  local key_str = table.concat(mashup, '\001')
  local key = native.digest_guid(key_str)
  local value = generic_scanner_cache[key]
  if not value then
    value = data
    generic_scanner_cache[key] = data
  end
  return value
end

function all_scanners()
  local scanners = {}
  for k, v in pairs(cpp_scanner_cache) do
    scanners[v.Index + 1] = v
  end
  for k, v in pairs(generic_scanner_cache) do
    scanners[v.Index + 1] = v
  end
  return scanners
end
