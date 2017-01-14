
module(..., package.seeall)

unit_test('scalar interpolation', function (t)
  local e = require 'tundra.environment'
  local e1, e2, e3
  e1 = e.create(nil, { Foo="Foo", Baz="Strut" })
  e2 = e1:clone({ Foo="Bar" })
  e3 = e1:clone({ Baz="c++" })

  t:check_equal(e1:get("Foo"), "Foo")
  t:check_equal(e1:get("Baz"), "Strut")
  t:check_equal(e2:get("Foo"), "Bar")
  t:check_equal(e2:get("Baz"), "Strut")
  t:check_equal(e3:get("Fransos", "Ost"), "Ost")

  e1:set("Foo", "Foo")
  t:check_equal(e1:interpolate("$(Foo)"), "Foo")
  t:check_equal(e1:interpolate("$(Foo:u)"), "FOO")
  t:check_equal(e1:interpolate("$(Foo:l)"), "foo")

  t:check_equal(e1:interpolate("$(Foo) $(Baz)"), "Foo Strut")
  t:check_equal(e2:interpolate("$(Foo) $(Baz)"), "Bar Strut")
  t:check_equal(e3:interpolate("$(Foo) $(Baz)"), "Foo c++")
  t:check_equal(e1:interpolate("a $(<)", { ['<'] = "foo" }), "a foo")

  e1:set("FILE", "foo/bar.txt")
  t:check_equal(e1:interpolate("$(FILE:B)"), "foo/bar")
  t:check_equal(e1:interpolate("$(FILE:F)"), "bar.txt")
  t:check_equal(e1:interpolate("$(FILE:D)"), "foo")

  e1:set("FILEQ", '"foo/bar.txt"')
  t:check_equal(e1:interpolate("$(FILEQ:q)"), '"foo/bar.txt"')
end)

unit_test('list interpolation', function (t)
  local e = require 'tundra.environment'
  local e1 = e.create()

  e1:set("Foo", { "Foo" })
  t:check_equal(e1:interpolate("$(Foo)"), "Foo")

  e1:set("Foo", { "Foo", "Bar" } )
  t:check_equal(e1:interpolate("$(Foo)") , "Foo Bar")
  t:check_equal(e1:interpolate("$(Foo:j,)"), "Foo,Bar")
  t:check_equal(e1:interpolate("$(Foo:p!)") , "!Foo !Bar")
  t:check_equal(e1:interpolate("$(Foo:a!)") , "Foo! Bar!")
  t:check_equal(e1:interpolate("$(Foo:p-I:j__)") , "-IFoo__-IBar")
  t:check_equal(e1:interpolate("$(Foo:j\\:)"), "Foo:Bar")
  t:check_equal(e1:interpolate("$(Foo:u)"), "FOO BAR")
  t:check_equal(e1:interpolate("$(Foo:[2])"), "Bar")
  t:check_equal(e1:interpolate("$(Foo:Aoo)"), "Foo Baroo")
  t:check_equal(e1:interpolate("$(Foo:PF)"), "Foo FBar")

  local lookaside = {
    ['@'] = 'output',
    ['<'] = { 'a', 'b' },
  }

  t:check_equal(e1:interpolate("$(Foo) $(<)=$(@)", lookaside), "Foo Bar a b=output")

  -- Verify interpolation caching is cleared when keys change.
  e1:set("Foo", { "Baz" })
  t:check_equal(e1:interpolate("$(Foo) $(<)=$(@)", lookaside), "Baz a b=output")
end)
