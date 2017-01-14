-- rust-cargo.lua - Support for Rust and Cargo 

module(..., package.seeall)

local nodegen  = require "tundra.nodegen"
local files    = require "tundra.syntax.files"
local path     = require "tundra.path"
local util     = require "tundra.util"
local depgraph = require "tundra.depgraph"
local native   = require "tundra.native"

_rust_cargo_program_mt = nodegen.create_eval_subclass { }
_rust_cargo_shared_lib_mt = nodegen.create_eval_subclass { }
_rust_cargo_crate_mt = nodegen.create_eval_subclass { }

-- This function will gather up so extra dependencies. In the case when we depend on a Rust crate
-- We simply return the sources to allow the the unit being built to depend on it. The reason
-- for this is that Cargo will not actually link with this step but it's only used to make
-- sure it gets built when a Crate changes

function get_extra_deps(data, env) 
	local libsuffix = { env:get("LIBSUFFIX") }
  	local sources = data.Sources
  	local source_depts = {}
	local extra_deps = {} 

	for _, dep in util.nil_ipairs(data.Depends) do
    	if dep.Keyword == "StaticLibrary" then
			local node = dep:get_dag(env:get_parent())
			extra_deps[#extra_deps + 1] = node
			node:insert_output_files(sources, libsuffix)
		elseif dep.Keyword == "RustCrate" then
			local node = dep:get_dag(env:get_parent())
			source_depts[#source_depts + 1] = dep.Decl.Sources
		end
	end

	return extra_deps, source_depts 
end

local cmd_line_type_prog = 0
local cmd_line_type_shared_lib = 1
local cmd_line_type_crate = 2

function build_rust_action_cmd_line(env, data, program)
	local static_libs = ""

	-- build an string with all static libs this code depends on

	for _, dep in util.nil_ipairs(data.Depends) do
		if dep.Keyword == "StaticLibrary" then
			local node = dep:get_dag(env:get_parent())
			static_libs = static_libs .. dep.Decl.Name .. " "
		end
	end

	-- The way Cargo sets it's target directory is by using a env variable which is quite ugly but that is the way it works.
	-- So before running the command we set the target directory of  
	-- We also set the tundra cmd line as env so we can use that inside the build.rs
	-- to link with the libs in the correct path

	local target = path.join("$(OBJECTDIR)", "__" .. data.Name)

	local target_dir = "" 
	local tundra_dir = "$(OBJECTDIR)";
	local export = "export ";
	local merge = " ; ";

	if native.host_platform == "windows" then
		export = "set "
		merge = "&&"
	end

	target_dir = export .. "CARGO_TARGET_DIR=" .. target .. merge 
	tundra_dir = export .. "TUNDRA_OBJECTDIR=" .. tundra_dir .. merge 

	if static_libs ~= "" then
		-- Remove trailing " "
		local t = string.sub(static_libs, 1, string.len(static_libs) - 1)
		static_libs = export .. "TUNDRA_STATIC_LIBS=\"" .. t .. "\"" .. merge 
	end

	local variant = env:get('CURRENT_VARIANT')
	local release = ""
	local output_target = "" 
	local output_name = ""

	-- Make sure output_name gets prefixed/sufixed correctly

	if program == cmd_line_type_prog then
		output_name = data.Name .. "$(HOSTPROGSUFFIX)"
	elseif program == cmd_line_type_shared_lib then
		output_name = "$(SHLIBPREFIX)" .. data.Name .. "$(HOSTSHLIBSUFFIX)"
	else
		output_name = "$(SHLIBPREFIX)" .. data.Name .. ".rlib" 
	end

	-- If variant is debug (default) we assume that we should use debug and not release mode

	if variant == "debug" then
		output_target = path.join(target, "debug$(SEP)" .. output_name) 
	else
		output_target = path.join(target, "release$(SEP)" .. output_name) 
		release = " --release "
	end

	-- If user hasn't set any specific cargo opts we use build as default
	-- Setting RUST_CARGO_OPTS = "build" by default doesn't seem to work as if user set
	-- RUST_CARGO_OPTS = "test" the actual string is "build test" which doesn't work
	local cargo_opts = env:interpolate("$(RUST_CARGO_OPTS)")
	if cargo_opts == "" then
		cargo_opts = "build"
	end

	local action_cmd_line = tundra_dir .. target_dir .. static_libs .. "$(RUST_CARGO) " .. cargo_opts .. " --manifest-path=" .. data.CargoConfig .. release

	return action_cmd_line, output_target
end

function _rust_cargo_program_mt:create_dag(env, data, deps)

	local action_cmd_line, output_target = build_rust_action_cmd_line(env, data, cmd_line_type_prog)
	local extra_deps, dep_sources = get_extra_deps(data, env)

	local build_node = depgraph.make_node {
		Env          = env,
		Pass         = data.Pass,
		InputFiles   = util.merge_arrays({ data.CargoConfig }, data.Sources, util.flatten(dep_sources)),
		Annotation 	 = path.join("$(OBJECTDIR)", data.Name), 
		Label        = "Cargo Program $(@)",
		Action       = action_cmd_line,
		OutputFiles  = { output_target }, 
		Dependencies = util.merge_arrays(deps, extra_deps),
	}

	local dst ="$(OBJECTDIR)" .. "$(SEP)" .. path.get_filename(env:interpolate(output_target))
	local src = output_target

	-- Copy the output file to the regular $(OBJECTDIR) 
	return files.copy_file(env, src, dst, data.Pass, { build_node })
end

function _rust_cargo_shared_lib_mt:create_dag(env, data, deps)

	local action_cmd_line, output_target = build_rust_action_cmd_line(env, data, cmd_line_type_shared_lib)
	local extra_deps, dep_sources = get_extra_deps(data, env)

	local build_node = depgraph.make_node {
		Env          = env,
		Pass         = data.Pass,
		InputFiles   = util.merge_arrays({ data.CargoConfig }, data.Sources, util.flatten(dep_sources)),
		Annotation 	 = path.join("$(OBJECTDIR)", data.Name), 
		Label        = "Cargo SharedLibrary $(@)",
		Action       = action_cmd_line,
		OutputFiles  = { output_target }, 
		Dependencies = util.merge_arrays(deps, extra_deps),
	}

	local dst ="$(OBJECTDIR)" .. "$(SEP)" .. path.get_filename(env:interpolate(output_target))
	local src = output_target

	-- Copy the output file to the regular $(OBJECTDIR) 
	return files.copy_file(env, src, dst, data.Pass, { build_node })
end

function _rust_cargo_crate_mt:create_dag(env, data, deps)

	local action_cmd_line, output_target = build_rust_action_cmd_line(env, data, cmd_line_type_crate)
	local extra_deps, dep_sources = get_extra_deps(data, env)

	local build_node = depgraph.make_node {
		Env          = env,
		Pass         = data.Pass,
		InputFiles   = util.merge_arrays({ data.CargoConfig }, data.Sources, util.flatten(dep_sources)),
		Annotation 	 = path.join("$(OBJECTDIR)", data.Name), 
		Label        = "Cargo Crate $(@)",
		Action       = action_cmd_line,
		OutputFiles  = { output_target }, 
		Dependencies = util.merge_arrays(deps, extra_deps),
	}

	return build_node 
end


local rust_blueprint = {
  Name = { 
  	  Type = "string", 
  	  Required = true, 
  	  Help = "Name of the project. Must match the name in Cargo.toml" 
  },
  CargoConfig = { 
  	  Type = "string", 
  	  Required = true, 
  	  Help = "Path to Cargo.toml" 
  },
  Sources = {
    Required = true,
    Help = "List of source files",
    Type = "source_list",
    ExtensionKey = "RUST_SUFFIXES", 
  },
}

nodegen.add_evaluator("RustProgram", _rust_cargo_program_mt, rust_blueprint)
nodegen.add_evaluator("RustSharedLibrary", _rust_cargo_shared_lib_mt, rust_blueprint)
nodegen.add_evaluator("RustCrate", _rust_cargo_crate_mt, rust_blueprint)

