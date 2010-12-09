#include "../internal/api.h"

size_t fa_deflate_stream(fa_compression_t compression, size_t blockSize, fa_compress_output_callback_t output, fa_compress_input_callback_t input, void* userData)
{
	size_t total = 0;

	uint8_t* inbuf = malloc(blockSize);
	uint8_t* outbuf = malloc(blockSize * 2);

	for (;;)
	{
		size_t insize = input(inbuf, blockSize, userData);
		size_t outsize = blockSize;

		if (size == 0)
		{
			break;
		}

		if (compression == FA_COMPRESSION_NONE)
		{
			if (output(inbuf, insize, userData) != size)
			{
				break;
			}

			total += insize;
			continue;
		}

		switch (compression)
		{
			default: break;
			case FA_COMPRESSION_FASTLZ:
			{
				if (insize < 16)
				{
					break;
				}

				outsize = fastlz_compress_level(2, inbuf, insize, outbuf);
			}
			break;
		}

		if (outsize >= blockSize)
		{
			fa_block_t block;

			block.original = insize;
			block.compressed = FA_COMPRESSION_SIZE_IGNORE | insize;

			if (output(&block, sizeof(block), userData) != sizeof(block))
			{
				break;
			}

			if (output(inbuf, insize, userData) != size)
			{
				break;
			}

			total += sizeof(block) + insize;
		}
		else
		{
			fa_block_t block;

			block.original = insize;
			block.compressed = outsize;

			if (output(&block, sizeof(block), userData) != sizeof(block))
			{
				break;
			}

			if (output(outbuf, outsize, userData) != outsize)
			{
				break;
			}

			total += sizeof(block) + outsize;
		}
	}

	free(inbuf);
	free(outbuf);

	return total;
}

size_t fa_inflate_stream(fa_compression_t compression, size_t blockSize, fa_compress_output_callback_t output, fa_compress_input_callback_t input, void* userData)
{
	size_t total = 0;

	uint8_t* inbuf = malloc(sizeof(fa_block_t) + blockSize);
	uint8_t* outbuf = malloc(blockSize);

	for (;;)
	{
		size_t insize = input(
	}

	free(inbuf);
	free(outbuf);

	return total;
}

