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

