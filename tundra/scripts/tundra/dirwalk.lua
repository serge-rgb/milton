module(..., package.seeall)

local native = require "tundra.native"

-- Stash of all dir walks performed for signature generation.
local query_records = {}

function walk(path, filter_callback)

  local dir_stack = { path }
  local paths_out = {}

  while #dir_stack > 0 do
    local dir = dir_stack[#dir_stack]
    table.remove(dir_stack)

    local subdirs, files = native.list_directory(dir)
    query_records[dir] = { Files = files, SubDirs = subdirs }

    for _, subdir in ipairs(subdirs) do
      full_dir_path = dir .. SEP .. subdir
      if not filter_callback or filter_callback(subdir) then
        table.insert(dir_stack, full_dir_path)
      end
    end

    for _, file in ipairs(files) do
      table.insert(paths_out, dir .. SEP .. file)
    end
  end

  return paths_out
end

function all_queries()
  local result = {}
  for k, v in pairs(query_records) do
    result[#result + 1] = { Path = k, Files = v.Files, SubDirs = v.SubDirs }
  end
  return result
end
