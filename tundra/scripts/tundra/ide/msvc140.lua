-- Microsoft Visual Studio 2013 Solution/Project file generation

module(..., package.seeall)

local msvc_common = require "tundra.ide.msvc-common"

msvc_common.setup("14.00", "2015")
