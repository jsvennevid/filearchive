StaticLibrary
{
	Name = "filearchive",

	Sources = {
		Glob {
			Dir = "src", Extensions = { ".c" }
		},
		Glob {
			Dir = "contrib/fastlz", Extensions = { ".c" }
		},
		Glob {
			Dir = "contrib/sha1", Extensions = { ".c" }
		} 
	},

	Propagate = {
		Env = {
			CPPPATH = { "include" },
		},

		Libs = {
			{ "z"; Config = "macosx-*-*-*" },
			{ "z", "lzma"; Config = "linux-*-*-*" },
		},

		Defines = {
			{ "FA_ZLIB_ENABLE"; Config = "macosx-*-*-*" },
			{ "FA_ZLIB_ENABLE", "FA_LZMA_ENABLE"; Config = "linux-*-*-*" },
		}
	},

	Defines = {
		{ "FA_ZLIB_ENABLE"; Config = "macosx-*-*-*" },
		{ "FA_ZLIB_ENABLE", "FA_LZMA_ENABLE"; Config = "linux-*-*-*" },
	},

	Env = {
		CPPPATH = { "include", "contrib" }
	},
}

-------------
-- Contrib --
-------------

Program
{
	Name = "farc",

	Sources = {
		Glob { Dir = "tools/farc", Extensions = { ".c" } }
	},

	Depends = {
		"filearchive"
	}
}

Default "filearchive"
Default "farc"
