module(..., package.seeall)

local npath = require "tundra.native.path"
local native = require "tundra.native"

split             = npath.split
normalize         = npath.normalize
join              = npath.join
get_filename_dir  = npath.get_filename_dir
get_filename      = npath.get_filename
get_extension     = npath.get_extension
drop_suffix       = npath.drop_suffix
get_filename_base = npath.get_filename_base
is_absolute       = npath.is_absolute
digest_guid       = native.digest_guid

function remove_prefix(prefix, fn)
  if fn:find(prefix, 1, true) == 1 then
    return fn:sub(#prefix + 1)
  else
    return fn
  end
end

function make_object_filename(env, src_fn, suffix)
  local object_fn

  local src_suffix = get_extension(src_fn):sub(2)

  -- Drop leading $(OBJECTDIR)[/\\] in the input filename.
  do
    local pname = src_fn:match("^%$%(OBJECTDIR%)[/\\](.*)$")
    if pname then
      object_fn = pname
    else
      object_fn = src_fn
    end
  end

  -- Compute path under OBJECTDIR we want for the resulting object file.
  -- Replace ".." with "dotdot" to avoid creating files outside the
  -- object directory. Also salt the generated object name with the source
  -- suffix, so that multiple source files with the same base name don't end
  -- up clobbering each other (Tundra emits an error for this when checking
  -- the DAG)
  do
    local hashed_name = digest_guid(drop_suffix(object_fn:gsub("%.%.", "dotdot")))
    local fn = drop_suffix(get_filename(object_fn):gsub("%.%.", "dotdot"))
    object_fn = "$(OBJECTDIR)/$(UNIT_PREFIX)/" .. fn .. "-" .. src_suffix .. "-" .. hashed_name .. suffix
  end

  return object_fn
end
