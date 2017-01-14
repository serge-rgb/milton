-- glob.lua - Glob syntax elements for declarative tundra.lua usage

module(..., package.seeall)

local util = require "tundra.util"
local path = require "tundra.path"
local decl = require "tundra.decl"
local dirwalk = require "tundra.dirwalk"

local ignored_dirs = util.make_lookup_table { ".git", ".svn", "CVS" }

local function glob(directory, recursive, filter_fn)
  local result = {}

  local function dir_filter(dir_name)
    if not recursive or ignored_dirs[dir_name] then
      return false
    end
    return true
  end

  for _, path in ipairs(dirwalk.walk(directory, dir_filter)) do
    if filter_fn(path) then
      result[#result + 1] = string.gsub(path, "\\", "/")
    end
  end
  return result
end

-- Glob syntax - Search for source files matching extension list
--
-- Synopsis:
--   Glob {
--      Dir = "...",
--      Extensions = { ".ext", ... },
--      [Recursive = false,]
--   }
--
-- Options:
--    Dir = "directory" (required)
--    - Base directory to search in
--
--    Extensions = { ".ext1", ".ext2" } (required)
--    - List of file extensions to include
--
--    Recursive = boolean (optional, default: true)
--    - Specified whether to recurse into subdirectories
function Glob(args)
  local recursive = args.Recursive
  if type(recursive) == "nil" then
    recursive = true
  end
  if not args.Extensions then
    croak("no 'Extensions' specified in Glob (Dir is '%s')", args.Dir)
  end
  local extensions = assert(args.Extensions)
  local ext_lookup = util.make_lookup_table(extensions)
  return glob(args.Dir, recursive, function (fn)
    local ext = path.get_extension(fn)
    return ext_lookup[ext]
  end)
end

-- FGlob syntax - Search for source files matching extension list with
-- configuration filtering
--
-- Usage:
--   FGlob {
--       Dir = "...",
--       Extensions = { ".ext", .... },
--       Filters = {
--         { Pattern = "/[Ww]in32/", Config = "win32-*-*" },
--         { Pattern = "/[Dd]ebug/", Config = "*-*-debug" },
--         ...
--       },
--       [Recursive = false],
--   }
local function FGlob(args)
  -- Use the regular glob to fetch the file list.
  local files = Glob(args)
  local pats = {}
  local result = {}

  -- Construct a mapping from { Pattern = ..., Config = ... }
  -- to { Pattern = { Config = ... } } with new arrays per config that can be
  -- embedded in the source result.
  for _, fitem in ipairs(args.Filters) do
    if not fitem.Config then
      croak("no 'Config' specified in FGlob (Pattern is '%s')", fitem.Pattern)
    end
    local tab = { Config = assert(fitem.Config) }
    pats[assert(fitem.Pattern)] = tab
    result[#result + 1] = tab
  end

  -- Traverse all files and see if they match any configuration filters. If
  -- they do, stick them in matching list. Otherwise, just keep them in the
  -- main list. This has the effect of returning an array such as this:
  -- {
  --   { "foo.c"; Config = "abc-*-*" },
  --   { "bar.c"; Config = "*-*-def" },
  --   "baz.c", "qux.m"
  -- }
  for _, f in ipairs(files) do
    local filtered = false
    for filter, list in pairs(pats) do
      if f:match(filter) then
        filtered = true
        list[#list + 1] = f
        break
      end
    end
    if not filtered then
      result[#result + 1] = f
    end
  end
  return result
end

decl.add_function("Glob", Glob)
decl.add_function("FGlob", FGlob)
