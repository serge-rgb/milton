module(..., package.seeall)

function ConfigureRaw(cmdline, name, constructor)
  local fh = assert(io.popen(cmdline))
  local data = fh:read("*all")
  fh:close()

  local cpppath = {}
  local libpath = {}
  local libs = {}
  local defines = {}
  local frameworks = {}

  for kind, value in data:gmatch("-([ILlD])([^ \n\r]+)") do
    if kind == "I" then
      cpppath[#cpppath + 1] = value
    elseif kind == "D" then
      defines[#defines + 1] = value
    elseif kind == "L" then
      libpath[#libpath + 1] = value
    elseif kind == "l" then
      libs[#libs + 1] = value
    end
  end

  for value in data:gmatch("-framework ([^ \n\r]+)") do
    frameworks[#frameworks + 1] = value
  end

  -- We don't have access to ExternalLibrary here - user has to pass it in.
  return constructor({
    Name = name,
    Propagate = {
      Env = {
        FRAMEWORKS = frameworks,
        CPPDEFS = defines,
        CPPPATH = cpppath,
        LIBS    = libs,
        LIBPATH = libpath
      }
    }
  })
end

function Configure(name, ctor)
  return ConfigureRaw("pkg-config " .. name .. " --cflags --libs", name, ctor)
end

function ConfigureWithTool(tool, name, ctor)
  return ConfigureRaw(tool .. " --cflags --libs", name, ctor)
end
