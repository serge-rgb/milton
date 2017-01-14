module(..., package.seeall)

local function generic_dotnet_setup(env)

end

function apply(env, options)
  env:add_setup_function(generic_dotnet_setup)

  env:set_many {
    ["CSLIBS"] = "", -- assembly references
    ["CSLIBPATH"] = {}, -- assembly directories
    ["CSCOPTS"] = "-optimize",
    ["CSRESOURCES"] = "",
    ["CSC_WARNING_LEVEL"] = "4",
  }
end
