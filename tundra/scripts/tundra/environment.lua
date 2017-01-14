module(..., package.seeall)

local util     = require 'tundra.util'
local path     = require 'tundra.path'
local depgraph = require 'tundra.depgraph'
local nenv     = require 'tundra.environment.native'
local os       = require 'os'

local global_setup = {}

--[==[

The environment is a holder for variables and their associated values. Values
are always kept as tables, even if there is only a single value.

FOO = { a b c }

e:interpolate("$(FOO)") -> "a b c"
e:interpolate("$(FOO:j, )") -> "a, b, c"
e:interpolate("$(FOO:p-I)") -> "-Ia -Ib -Ic"

Missing keys trigger errors unless a default value is specified.

]==]--

local envclass = {}

function envclass:create(parent, assignments, obj)
  obj = obj or {}
  setmetatable(obj, self)
  self.__index = self

  obj.cached_interpolation = {}
  obj.vars = {}
  obj.parent = parent
  obj.lookup = { obj.vars }
  obj.memos = {}
  obj.memo_keys = {}
  obj.external_vars = parent and util.clone_table(parent.external_vars) or nil

  -- assign initial bindings
  if assignments then
    obj:set_many(assignments)
  end

  return obj
end

function envclass:clone(assignments)
  return envclass:create(self, assignments)
end

function envclass:register_implicit_make_fn(ext, fn, docstring)
  if type(ext) ~= "string" then
    errorf("extension must be a string")
  end
  if type(fn) ~= "function" then
    errorf("fn must be a function")
  end

  if not ext:match("^%.") then
    ext = "." .. ext -- we want the dot in the extension
  end

  if not self._implicit_exts then
    self._implicit_exts = {}
  end

  self._implicit_exts[ext] = {
    Function = fn,
    Doc = docstring or "",
  }
end

function envclass:get_implicit_make_fn(filename)
  local ext = path.get_extension(filename)
  local chain = self
  while chain do
    local t = chain._implicit_exts
    if t then
      local v = t[ext]
      if v then return v.Function end
    end
    chain = chain.parent
  end
  return nil
end

function envclass:has_key(key)
  local chain = self
  while chain do
    if chain.vars[key] then
      return true
    end
    chain = chain.parent
  end

  return false
end

function envclass:get_vars()
  return self.vars
end

function envclass:set_many(table)
  for k, v in pairs(table) do
    self:set(k, v)
  end
end

function envclass:append(key, value)
  if type(value) ~= "string" then
    error("environment append: " .. util.tostring(value) .. " is not a string", 2)
  end
  self:invalidate_memos(key)
  local t = self:get_list(key, 1)
  local result
  if type(t) == "table" then
    result = util.clone_array(t)
    table.insert(result, value)
  else
    result = { value }
  end
  self.vars[key] = result
end

function envclass:append_many(data)
  for k, v in pairs(data) do
    self:append(k, v)
  end
end

function envclass:replace(key, value)
  if type(value) == "string" then
    value = { value }
  end
  assert(type(value) == "table")

  self:invalidate_memos(key)
  self.vars[key] = value
end

function envclass:invalidate_memos(key)
  self.cached_interpolation = {}
  local name_tab = self.memo_keys[key]
  if name_tab then
    for name, _ in pairs(name_tab) do
      self.memos[name] = nil
    end
  end
end

function envclass:set_default(key, value)
  if not self:has_key(key) then
    self:set(key, value)
  end
end

function envclass:set_default_many(table)
  for key, value in pairs(table) do
    self:set_default(key, value)
  end
end

function envclass:set(key, value)
  self:invalidate_memos(key)
  assert(key:len() > 0, "key must not be empty")
  assert(type(key) == "string", "key must be a string")
  if type(value) == "string" then
    if value:len() > 0 then
      self.vars[key] = { value }
    else
      -- let empty strings make empty tables
      self.vars[key] = {}
    end
  elseif type(value) == "table" then
    -- FIXME: should filter out empty values
    for _, v in ipairs(value) do
      if not type(v) == "string" then
        error("key " .. key .. "'s table value contains non-string value " .. tostring(v))
      end
    end
    self.vars[key] = util.clone_array(value)
  else
    error("key " .. key .. "'s value is neither table nor string: " .. tostring(value))
  end
end

function envclass:get_id()
  return self.id
end

function envclass:get(key, default)
  local v = self.vars[key]
  if v then
    return table.concat(v, " ")
  elseif self.parent then
    return self.parent:get(key, default)
  elseif default then
    return default
  else
    error(string.format("key '%s' not present in environment", key))
  end
end

function envclass:get_list(key, default)
  local v = self.vars[key]
  if v then
    return v -- FIXME: this should be immutable from the outside
  elseif self.parent then
    return self.parent:get_list(key, default)
  elseif default then
    return default
  elseif not key then
    error("nil key is not allowed")
  else
    error(string.format("key '%s' not present in environment", key))
  end
end

function envclass:get_parent()
  return self.parent
end

function envclass:interpolate(str, vars)
  local cached = self.cached_interpolation[str]

  if not cached then
    cached = nenv.interpolate(str, self)
    self.cached_interpolation[str] = cached
  end

  if vars then
    return nenv.interpolate(cached, self, vars)
  else
    return cached
  end
end

function create(parent, assignments, obj)
  return envclass:create(parent, assignments, obj)
end

function envclass:record_memo_var(key, name)
  local tab = self.memo_keys[key]
  if not tab then
    tab = {}
    self.memo_keys[key] = tab
  end
  tab[name] = true
end

function envclass:memoize(key, name, fn)
  local memo = self.memos[name]
  if not memo then 
    self:record_memo_var(key, name)
    memo = fn()
    self.memos[name] = memo
  end
  return memo
end

function envclass:get_external_env_var(key)
  local chain = self
  while chain do
    local t = self.external_vars
    if t then
      local v = t[key]
      if v then return v end
    end

    chain = chain.parent
  end

  return os.getenv(key)
end

function envclass:set_external_env_var(key, value)
  local t = self.external_vars
  if not t then
    t = {}
    self.external_vars = t
  end
  t[key] = value
end

function envclass:add_setup_function(fn)
  local t = self.setup_funcs
  if not t then
    t = {}
    self.setup_funcs = t
  end
  t[#t + 1] = fn
end

function envclass:run_setup_functions()
    for _, func in ipairs(global_setup) do
        func(self)
    end
  t = self.setup_funcs
  local chain = self
  while chain do
    for _, func in util.nil_ipairs(chain.setup_funcs) do
      func(self)
    end
    chain = chain.parent
  end
end

function add_global_setup(fn)
    global_setup[#global_setup + 1] = fn
end

function is_environment(datum)
  return getmetatable(datum) == envclass
end
