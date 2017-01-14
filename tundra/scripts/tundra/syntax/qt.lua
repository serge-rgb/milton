-- qt.lua -- support for Qt-specific compilers and tools

module(..., package.seeall)

local scanner = require "tundra.scanner"
local path = require "tundra.path"

DefRule {
  Name = "Moc",
  Command = "$(QTMOCCMD)",
  Blueprint = {
    Source = { Required = true, Type = "string" },
  },

  Setup = function (env, data)
    local src = data.Source
    -- We follow these conventions:
    --   If the source file is a header, we do a traditional header moc:
    --     - input: foo.h, output: moc_foo.cpp
    --     - moc_foo.cpp is then compiled separately together with (presumably) foo.cpp
    --   If the source file is a cpp file, we do things a little differently:
    --     - input: foo.cpp, output foo.moc
    --     - foo.moc is then manually included at the end of foo.cpp
    local base_name = path.get_filename_base(src) 
    local pfx = 'moc_'
    local ext = '.cpp'
    if path.get_extension(src) == ".cpp" then
      pfx = ''
      ext = '.moc'
    end
    return {
      InputFiles = { src },
      OutputFiles = { "$(OBJECTDIR)$(SEP)" .. pfx .. base_name .. ext },
      Scanner = scanner.make_cpp_scanner(env:get_list('CPPPATH'))
    }
  end,
}

DefRule {
  Name = "Rcc",
  Command = "$(QTRCCCMD)",
  Blueprint = {
    Source = { Required = true, Type = "string" },
  },

  Setup = function (env, data)
    local src = data.Source
    local base_name = path.get_filename_base(src) 
    local pfx = 'qrc_'
    local ext = '.cpp'
    return {
      InputFiles = { src },
      OutputFiles = { "$(OBJECTDIR)$(SEP)" .. pfx .. base_name .. ext },
      Scanner = scanner.make_generic_scanner {
        Paths = { "." },
        KeywordsNoFollow = { "<file" },
        UseSeparators = true
      },
    }
  end,
}

DefRule {
  Name = "Uic",
  Command = "$(QTUICCMD)",
  Blueprint = {
    Source = { Required = true, Type = "string" },
  },

  Setup = function (env, data)
    local src = data.Source
    local base_name = path.get_filename_base(src) 
    local pfx = 'ui_'
    local ext = '.h'
    return {
      InputFiles = { src },
      OutputFiles = { "$(OBJECTDIR)$(SEP)" .. pfx .. base_name .. ext },
    }
  end,
}
