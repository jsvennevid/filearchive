#include "../internal/api.h"

#include <fastlz/fastlz.h>

size_t fa_compress_block(fa_compression_t compression, void* out, size_t outSize, const void* in, size_t inSize)
{
	switch (compression)
	{
		default: return inSize;

		case FA_COMPRESSION_FASTLZ:
		{
			if (inSize < 16)
			{
				return inSize;
			}

			return fastlz_compress_level(2, in, inSize, out);
		}
		break;
	}
}

size_t fa_decompress_block(fa_compression_t compression, void* out, size_t outSize, const void* in, size_t inSize)
{
	switch (compression)
	{
		default: return 0;

		case FA_COMPRESSION_FASTLZ:
		{
			return fastlz_decompress(in, inSize, out, outSize);
		}
		break;
	}
}
