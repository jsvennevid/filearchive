/*

Copyright (c) 2010 Jesper Svennevid

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#include <filearchive/internal/api.h>

#include <fastlz/fastlz.h>

#if defined(FA_ZLIB_ENABLE)
#include <zlib.h>
#endif

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
#if defined(FA_ZLIB_ENABLE)
		case FA_COMPRESSION_DEFLATE:
		{
			uLongf destLen = outSize;

			if (outSize < compressBound(inSize))
			{
				return inSize;
			}

			if (compress2(out, &destLen, in, inSize, Z_DEFAULT_COMPRESSION) != Z_OK)
			{
				return inSize;
			}

			return destLen;
		}
		break;
#endif
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
#if defined(FA_ZLIB_ENABLE)
		case FA_COMPRESSION_DEFLATE:
		{
			uLongf destLen = outSize;

			if (uncompress(out, &destLen, in, inSize) != Z_OK)
			{
				return 0;
			}

			return destLen;
		}
		break;
#endif
	}
}

