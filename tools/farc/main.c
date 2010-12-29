#include <stdio.h>
#include <string.h>

int commandCreate(int argc, char* argv[]);
int commandList(int argc, char* argv[]);
int commandCat(int argc, char* argv[]);
int commandHelp(const char* command);

int main(int argc, char* argv[])
{
	if (argc < 2)
	{
		commandHelp(NULL);
		return 0;
	}
	else if (!strcmp("help", argv[1]))
	{
		commandHelp(argc >= 3 ? argv[2] : argv[1]);
		return 0;
	}
	else if (!strcmp("create", argv[1]) || !strcmp("c", argv[1]))
	{
		if (commandCreate(argc, argv) < 0)
		{
			commandHelp("create");
			return 1;
		}
	}
	else if (!strcmp("list", argv[1]) || !strcmp("l", argv[1]))
	{
		if (commandList(argc, argv) < 0)
		{
			commandHelp("list");
			return 1;
		}
	}
	else if (!strcmp("cat", argv[1]))
	{
		if (commandCat(argc, argv) < 0)
		{
			commandHelp("cat");
			return 1;
		}
	}
	else
	{
		fprintf(stderr, "Unknown filearchive command.\n");
		commandHelp(NULL);
		return 1;
	}

	return 0;
}
