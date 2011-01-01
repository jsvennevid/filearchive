#include <stdio.h>
#include <string.h>

void commandHelp(char* command)
{
	fprintf(stderr, "\n");

	if (command == NULL)
	{
		fprintf(stderr, "farc <command> ...\n");
		fprintf(stderr, "command = create, help, list, cat\n\n");
		return;
	}
	else if (!strcmp("help", command))
	{
		fprintf(stderr, "Help is available for the following commands:\n\n");
		fprintf(stderr, "\tcreate (short: c)\n");
		fprintf(stderr, "\tlist (short: l)\n");
		fprintf(stderr, "\n");
		return;
	}
	else if (!strcmp("create", command) || !strcmp("c", command))
	{
		fprintf(stderr, "farc crecte|c [<options>] <archive> [@<spec>] [<path> ...]\n\n");
		fprintf(stderr, "Create a new file archive.\n\n");
		fprintf(stderr, "Options are:\n");
		fprintf(stderr, "\t-z <compression>   Select compression method: fastlz lz77 none (default: none) (global/spec)\n");
		fprintf(stderr, "\t-s                 Optimize layout for optical media (align access to block boundaries) (global)\n");
		fprintf(stderr, "\t-v                 Enabled verbose output (global)\n");
		fprintf(stderr, "\n<archive> = Archive file to create\n");
		fprintf(stderr, "<spec> = File spec to read files description from (files are gathered relative to spec path)\n");
		fprintf(stderr, "<path> ... = Paths to load files from (files are gathered relative to path root)\n");
		fprintf(stderr, "\nSpec file format:\n");
		fprintf(stderr, "\"<file path>\" <options>\n");
		fprintf(stderr, "\n");
		return;
	}
	else if (!strcmp("list", command) || !strcmp("l", command))
	{
		fprintf(stderr, "farc list|l <archive> ...\n\n");
		fprintf(stderr, "List contents of one or more archives.\n\n");
		return;
	}
	else if (!strcmp("cat", command))
	{
		fprintf(stderr, "farc cat <archive> <file> ...\n\n");
		fprintf(stderr, "Pipe contents of one or more files to standard output\n\n");
		return;
	}

	fprintf(stderr, "Unknown help topic.\n\n");
}
