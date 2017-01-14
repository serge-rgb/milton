local _tostring = tostring
module(..., package.seeall)

function tostring(value, stack)
  local str = ''
  stack = stack or {}

  if type(value) ~= 'table' then
    if type(value) == 'string' then
      str = string.format("%q", value)
    else
      str = _tostring(value)
    end
  elseif stack[value] then
    return '<recursion>'
  else
    stack[value] = true
    local auxTable = {}
    for k, v in pairs(value) do
      auxTable[#auxTable + 1] = k
    end
    table.sort(auxTable, function (a, b) return _tostring(a) < _tostring(b) end)

    str = str..'{'
    local separator = ""
    local entry = ""
    for index, fieldName in ipairs(auxTable) do
      if ((tonumber(fieldName)) and (tonumber(fieldName) > 0)) then
        entry = tostring(value[tonumber(fieldName)], stack)
      else
        entry = tostring(fieldName) .. " = " .. tostring(rawget(value, fieldName), stack)
      end
      str = str..separator..entry
      separator = ", "
    end
    str = str..'}'

    local mt = getmetatable(value)
    if mt then
      str = str .. ' @meta = ' .. tostring(mt, stack)
    end
  end
  return str
end

function map_in_place(t, fn)
  for x = 1, #t do
    t[x] = fn(t[x])
  end
  return t
end

function map(t, fn)
  local result = {}
  for idx = 1, #t do
    result[idx] = fn(t[idx])
  end
  return result
end

function mapnil(table, fn)
  if not table then
    return nil
  else
    return map(table, fn)
  end
end

function get_named_arg(tab, name, context)
  local v = tab[name]
  if v then
    return v
  else
    if context then
      error(context .. ": argument " .. name .. " must be specified", 3)
    else
      error("argument " .. name .. " must be specified", 3)
    end
  end
end

function parse_cmdline(args, blueprint)
  local index, max = 2, #args
  local options, targets = {}, {}
  local lookup = {}

  for _, opt in ipairs(blueprint) do
    if opt.Short then
      lookup[opt.Short] = opt
    end
    if opt.Long then
      lookup[opt.Long] = opt
    end
  end

  while index <= max do
    local s = args[index]
    local key, val

    if s:sub(1, 2) == '--' then
      key, val = s:match("^%-%-([-a-zA-Z0-9]+)=(.*)$")
      if not key then
        key = s:sub(3)
      end
    elseif s:sub(1, 1) == '-' then
      key = s:sub(2,2)
      if s:len() > 2 then
        val = s:sub(3)
      end
    else
      table.insert(targets, s)
    end

    if key then
      local opt = lookup[key]
      if not opt then
        return nil, nil, "Unknown option " .. s
      end
      if opt.HasValue then
        if not val then
          index = index + 1
          val = args[index]
        end
        if val then
          options[opt.Name] = val
        else
          return nil, nil, "Missing value for option "..s
        end
      else
        local v = options[opt.Name] or 0
        options[opt.Name] = v + 1
      end
    end

    index = index + 1
  end

  return options, targets
end

function clone_table(t)
  if t then
    local r = {}
    for k, v in pairs(t) do
      r[k] = v
    end
    for k, v in ipairs(t) do
      r[k] = v
    end
    return r
  else
    return nil
  end
end

function deep_clone_table(t)
  local function clone_value(v)
    if type(v) == "table" then
      return deep_clone_table(v)
    else
      return v
    end
  end
  if t then
    local r = {}
    for k, v in pairs(t) do
      r[clone_value(k)] = clone_value(v)
    end
    for k, v in ipairs(t) do
      r[k] = clone_value(v)
    end
    return r
  else
    return nil
  end
end

function clone_array(t)
  local r = {}
  for k, v in ipairs(t) do
    r[k] = v
  end
  return r
end

function merge_arrays(...)
  local result = {}
  local count = select('#', ...)
  for i = 1, count do
    local tab = select(i, ...)
    if tab then
      for _, v in ipairs(tab) do
        result[#result + 1] = v
      end
    end
  end
  return result
end

function merge_arrays_2(a, b)
  if a and b then
    return merge_arrays(a, b)
  elseif a then
    return a
  elseif b then
    return b
  else
    return {}
  end
end

function matches_any(str, patterns)
  for _, pattern in ipairs(patterns) do
    if str:match(pattern) then
      return true
    end
  end
  return false
end

function return_nil()
end

function nil_pairs(t)
  if t then
    return next, t
  else
    return return_nil
  end
end

function nil_ipairs(t)
  if t then
    return ipairs(t)
  else
    return return_nil
  end
end

function clear_table(tab)
  local key, val = next(tab)
  while key do
    tab[key] = nil
    key, val = next(tab, key)
  end
  return tab
end

function filter(tab, predicate)
  local result = {}
  for _, x in ipairs(tab) do
    if predicate(x) then
      result[#result + 1] = x
    end
  end
  return result
end

function filter_nil(tab, predicate)
  if not predicate then return nil end
  local result = {}
  for _, x in ipairs(tab) do
    if predicate(x) then
      result[#result + 1] = x
    end
  end
  return result
end

function filter_in_place(tab, predicate)
  local i, limit = 1, #tab
  while i <= limit do
    if not predicate(tab[i]) then
      table.remove(tab, i)
      limit = limit - 1
    else
      i = i + 1
    end
  end
  return tab
end

function append_table(result, items)
  local offset = #result
  for i = 1, #items do
    result[offset + i] = items[i]
  end
  return result
end

function flatten(array)
  local function iter(item, accum)
    if type(item) == 'table' and not getmetatable(item) then
      for _, sub_item in ipairs(item) do
        iter(sub_item, accum)
      end
    else
      accum[#accum + 1] = item
    end
  end
  local accum = {}
  iter(array, accum)
  return accum
end

function memoize(closure)
  local result = nil
  return function(...)
    if not result then
      result = assert(closure(...))
    end
    return result
  end
end

function uniq(array)
  local seen = {}
  local result = {}
  for _, val in nil_ipairs(array) do
    if not seen[val] then
      seen[val] = true
      result[#result + 1] = val
    end
  end
  return result
end

function make_lookup_table(array)
  local result = {}
  for _, item in nil_ipairs(array) do
    result[item] = true
  end
  return result
end

function table_keys(array)
  local result = {}
  for k, _ in nil_pairs(array) do
    result[#result + 1] = k
  end
  return result
end

function table_values(array)
  local result = {}
  for _, v in nil_pairs(array) do
    result[#result + 1] = v
  end
  return result
end

function array_contains(array, find)
  for _, val in ipairs(array) do
    if val == find then
      return true
    end
  end
  return false
end

