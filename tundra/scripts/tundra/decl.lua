module(..., package.seeall)

local nodegen = require "tundra.nodegen"

local functions = {}
local _decl_meta = {}
_decl_meta.__index = _decl_meta

local current = nil

local function new_parser()
  local obj = {
    Functions = {},
    Results = {},
    DefaultTargets = {},
    AlwaysTargets = {},
  }

  local outer_env = _G
  local iseval = nodegen.is_evaluator
  local function indexfunc(tab, var)
    if iseval(var) then
      -- Return an anonymous function such that
      -- the code "Foo { ... }" will result in a call to
      -- "nodegen.evaluate('Foo', { ... })"
      return function (data)
        local result = nodegen.evaluate(var, data)
        obj.Results[#obj.Results + 1] = result
        return result
      end
    end
    local p = obj.Functions[var]
    if p then return p end
    return outer_env[var]
  end

  obj.FunctionMeta = { __index = indexfunc, __newindex = error }
  obj.FunctionEnv = setmetatable({}, obj.FunctionMeta)

  for name, fn in pairs(functions) do
    obj.Functions[name] = setfenv(fn, obj.FunctionEnv)
  end

  obj.Functions["Default"] = function(default_obj)
    obj.DefaultTargets[#obj.DefaultTargets + 1] = default_obj
  end

  obj.Functions["Always"] = function(always_obj)
    obj.AlwaysTargets[#obj.AlwaysTargets + 1] = always_obj
  end

  current = setmetatable(obj, _decl_meta)
  return current
end

function add_function(name, fn)
  assert(name and fn)
  functions[name] = fn

  if current then
    -- require called from within unit script
    current.Functions[name] = setfenv(fn, current.FunctionEnv)
  end
end

function _decl_meta:parse_rec(data)
  local chunk
  if type(data) == "table" then
    for _, gen in ipairs(data) do
       self:parse_rec(gen)
    end
    return
  elseif type(data) == "function" then
    chunk = data
  elseif type(data) == "string" then
    chunk = assert(loadfile(data))
  else
    croak("unknown type %s for unit_generator %q", type(data), tostring(data))
  end

  setfenv(chunk, self.FunctionEnv)
  chunk()
end

function parse(data)
  p = new_parser()
  current = p
  p:parse_rec(data)
  current = nil
  return p.Results, p.DefaultTargets, p.AlwaysTargets
end
