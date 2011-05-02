local common = {}

Build {
	Units = "units.lua",
	SyntaxExtensions = { "tundra.syntax.glob" },

	Configs = {
		Config { Name = "win32-msvc", Inherit = common, Tools = { { "msvc-winsdk"; TargetArch = "x86" } } },
		Config { Name = "win64-msvc", Inherit = common, Tools = { { "msvc-winsdk"; TargetArch = "x64" } } },
		Config { Name = "macosx-clang", Inherit = common, Tools = { "clang-osx" } },
		Config { Name = "linux-gcc", Inherit = common, Tools = { "gcc" } },
		Config { Name = "openbsd-gcc", Inherit = common, Tools = { "gcc" } },
	},
}
