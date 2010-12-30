#include <filearchive/api.h>

int commandCat(int argc, char* argv[])
{
	fa_archive_t* archive = NULL;
	int result = -1;

	do
	{
		int i;
		fa_file_t* file;

		if (argc < 3)
		{
			fprintf(stderr, "cat: Not enough arguments to command\n");
			break;
		}

		if ((archive = fa_open_archive(argv[2], FA_MODE_READ, 0, NULL)) == NULL)
		{
			fprintf(stderr, "cat: Could not open archive \"%s\"\n", argv[2]);
			break;
		}

		for (i = 3; i < argc; ++i)
		{
			char buf[1024];
			size_t len;

			if ((file = fa_open(archive, argv[i], FA_COMPRESSION_NONE, NULL)) == NULL)
			{
				fprintf(stderr, "cat: Could not open archive file \"%s\"\n", argv[i]);
				break;
			}

			while ((len = fa_read(file, buf, sizeof(buf))) > 0)
			{
				if (fwrite(buf, 1, len, stdout) != len)
				{
					fprintf(stderr, "cat: Short write while writing to stdout\n");
					break;
				}
			}

			if (len > 0)
			{
				break;
			}

			if (fa_close(file, NULL) < 0)
			{
				fprintf(stderr, "cat: An error occured while closing archive file \"%s\"\n", argv[i]);
				break;
			}
			file = NULL;
		}

		if (file != NULL)
		{
			fa_close(file, NULL);
		}

		if (i == argc)
		{
			result = 0;
		}
	}
	while (0);

	if (archive != NULL)
	{
		fa_close_archive(archive, FA_COMPRESSION_NONE, NULL);
	}

	return result;
}

