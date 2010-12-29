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
			CPPPATH = { "include" }
		}
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
