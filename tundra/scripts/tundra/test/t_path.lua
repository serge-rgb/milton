module(..., package.seeall)

local path = require "tundra.path"
local native = require "tundra.native"

local function check_path(t, p, expected)
  p = p:gsub('\\', '/')
  t:check_equal(p, expected)
end

unit_test('path.normalize', function (t)
  check_path(t, path.normalize("foo"), "foo")
  check_path(t, path.normalize("foo/bar"), "foo/bar")
  check_path(t, path.normalize("foo//bar"), "foo/bar")
  check_path(t, path.normalize("foo/./bar"), "foo/bar")
  check_path(t, path.normalize("foo/../bar"), "bar")
  check_path(t, path.normalize("../bar"), "../bar")
  check_path(t, path.normalize("foo/../../bar"), "../bar")
end)

unit_test('path.join', function (t)
  check_path(t, path.join("foo", "bar"), "foo/bar")
  check_path(t, path.join("foo", "../bar"), "bar")
  check_path(t, path.join("/foo", "bar"), "/foo/bar")
end)

unit_test('path.split', function (t)
  local function check_split(p, expected_dir, expected_fn)
    local dir, fn = path.split(p)
    dir = dir:gsub('\\', '/')
    fn = fn:gsub('\\', '/')
    t:check_equal(dir, expected_dir)
    t:check_equal(fn, expected_fn)
  end

  check_split("", ".", "")
  check_split("foo", ".", "foo")
  check_split("foo/bar", "foo", "bar")
  check_split("/foo/bar", "/foo", "bar")
  check_split("x:\\foo\\bar", "x:/foo", "bar")
end)


unit_test('path.get_filename_dir', function (t)
  t:check_equal(path.get_filename_dir("foo/bar"), "foo")
  t:check_equal(path.get_filename_dir("foo"), "")
end)

unit_test('path.get_filename', function (t)
  t:check_equal(path.get_filename("foo/bar"), "bar")
  t:check_equal(path.get_filename("foo"), "foo")
end)

unit_test('path.get_extension', function (t)
  t:check_equal(path.get_extension("foo"), "")
  t:check_equal(path.get_extension("foo."), ".")
  t:check_equal(path.get_extension("foo.c"), ".c")
  t:check_equal(path.get_extension("foo/bar/.c"), ".c")
  t:check_equal(path.get_extension("foo/bar/baz.cpp"), ".cpp")
end)

unit_test('path.drop_suffix', function (t)
  t:check_equal(path.drop_suffix("foo.c"), "foo")
  t:check_equal(path.drop_suffix("foo/bar.c"), "foo/bar")
  t:check_equal(path.drop_suffix("/foo/bar.c"), "/foo/bar")
end)

unit_test('path.get_filename_base', function (t)
  t:check_equal(path.get_filename_base("foo1"), "foo1")
  t:check_equal(path.get_filename_base("foo2.c"), "foo2")
  t:check_equal(path.get_filename_base("/path/to/foo3"), "foo3")
  t:check_equal(path.get_filename_base("/path/to/foo4.c"), "foo4")
end)

unit_test('path.is_absolute', function (t)
  t:check_equal(path.is_absolute("/foo")    and "true" or "false", "true")
  t:check_equal(path.is_absolute("foo")     and "true" or "false", "false")
  if native.host_platform == "windows" then
    t:check_equal(path.is_absolute("x:\\foo") and "true" or "false", "true")
  end
end)
