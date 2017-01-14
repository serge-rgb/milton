

Build {
    Env = {
        CPPPATH = { "third_party", "third_party/imgui", "third_party/SDL2-2.0.3/include" },
        CXXOPTS = { }
    },

    Units = function()
        require "tundra.syntax.files"

        local shadergen = Program {
            Name = "shadergen",
            Sources = { "src/shadergen.cc" },
            Pass = "CodeGeneration",
        },
        DefRule {
            Name = "ShaderGen",
            Command = "pushd $(OBJECTROOT)$(SEP)$(BUILD_ID) && shadergen.exe",
            ImplicitInputs = { "$(OBJECTROOT)$(SEP)$(BUILD_ID)$(SEP)shadergen.exe" },
            Pass = "CodeGeneration",
            Blueprint = {
                Input = { Type = "string", Required = true },
            },
            ConfigInvariant = true,
            Setup = function(env, data)
                return {
                    InputFiles = { data.Input },
                    OutputFiles = { "src/shaders.gen.h" }
                }
            end,
        },
        DefRule {
            Name = "ResourceFile",
            Command = "$(RC) $(OBJECTROOT)$(SEP)$(BUILD_ID)$(SEP)Milton.rc ",
            ImplicitInputs = "$(OBJECTROOT)$(SEP)$(BUILD_ID)$(SEP)Milton.rc",
            Blueprint = {
                Input = { Type = "string", Required = true },
            },
            Setup = function(env, data)
                return {
                    InputFiles = { data.Input },
                    OutputFiles = { "$(OBJECTROOT)$(SEP)$(BUILD_ID)$(SEP)rc_dummy" }
                }
            end,
        }

        local milton = Program {
            Name = "Milton",
            Sources = {
                "src/milton.cc",
                "src/memory.cc",
                "src/gui.cc",
                "src/persist.cc",
                "src/color.cc",
                "src/canvas.cc",
                "src/profiler.cc",
                "src/gl_helpers.cc",
                "src/localization.cc",
                "src/hardware_renderer.cc",
                "src/utils.cc",
                "src/hash.cc",
                "src/vector.cc",
                "src/sdl_milton.cc",
                "src/StrokeList.cc",
                "src/platform_windows.cc",
                "src/third_party_libs.cc",

                -- ResourceFile { Input="Milton.rc" },
                ShaderGen { Input = "src/shadergen.cc" },
            },
            Libs = {
                { "kernel32.lib"; Config = { "win*-*-*" } },
                { "OpenGL32.lib"; Config = { "win*" } },
                { "User32.lib"; Config = { "win*" } },
                { "gdi32.lib"; Config = { "win*" } },
                { "shell32.lib"; Config = { "win*" } },
                { "comdlg32.lib"; Config = { "win*" } },
                { "ole32.lib"; Config = { "win*" } },
                { "oleAut32.lib"; Config = { "win*" } },
                { "third_party/bin/SDL2.lib"; Config = { "win*" } },
            },
        }

        local copyRC = CopyFile { Source="Milton.rc", Target="$(OBJECTROOT)$(SEP)$(BUILD_ID)$(SEP)/Milton.rc" }
        local copyIcon = CopyFile { Source="milton_icon.ico", Target="$(OBJECTROOT)$(SEP)$(BUILD_ID)$(SEP)/milton_icon.ico" }
        local copyFont = CopyFile { Source="third_party/Carlito.ttf", Target="$(OBJECTROOT)$(SEP)$(BUILD_ID)$(SEP)/Carlito.ttf" }
        local copyFontLicense= CopyFile { Source="third_party/Carlito.LICENSE", Target="$(OBJECTROOT)$(SEP)$(BUILD_ID)$(SEP)/Carlito.LICENSE" }

        Default(copyRC)
        Default(copyIcon)
        Default(copyFont)
        Default(copyFontLicense)
        Default(milton)
    end,
    Passes = {
        CodeGeneration = { Name="Generate Code", BuildOrder=1 }
    },
    Configs = {
        Config {
            Name = "win64-msvc",
            DefaultOnHost = "windows",
            Tools = { {"msvc-vs2015"; TargetArch = "x64"} },
            Config = { "Debug" },
            Env = {
                CPPDEFS = "_CRT_SECURE_NO_WARNINGS",
                CXXOPTS = { "/Wall", "/EHsc", "/Zi", "/Oi", "/Od", "/WX",
                    -- Define
                    -- Comment for cleanup
                    "/wd4100", "/wd4189", "/wd4800", "/wd4127", "/wd4239", "/wd4987",
                    -- Disabled warnings
                    "/wd4305", "/wd4820", "/wd4255", "/wd4710", "/wd4711", "/wd4201", "/wd4204", "/wd4191", "/wd5027", "/wd4514", "/wd4242", "/wd4244", "/wd4738", "/wd4619", "/wd4505",
                }
            },
            ReplaceEnv = {
                OBJECTROOT = "build",
            },
        }
    },
}