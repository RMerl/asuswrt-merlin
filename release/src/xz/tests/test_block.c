///////////////////////////////////////////////////////////////////////////////
//
/// \file       test_block.c
/// \brief      Tests Block coders
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#include "tests.h"


static uint8_t text[] = "Hello world!";
static uint8_t buffer[4096];
static lzma_options_block block_options;
static lzma_stream strm = LZMA_STREAM_INIT;


static void
test1(void)
{

}


int
main()
{
	lzma_init();

	block_options = (lzma_options_block){
		.check_type = LZMA_CHECK_NONE,
		.has_eopm = true,
		.has_uncompressed_size_in_footer = false,
		.has_backward_size = false,
		.handle_padding = false,
		.total_size = LZMA_VLI_UNKNOWN,
		.compressed_size = LZMA_VLI_UNKNOWN,
		.uncompressed_size = LZMA_VLI_UNKNOWN,
		.header_size = 5,
	};
	block_options.filters[0].id = LZMA_VLI_UNKNOWN;
	block_options.filters[0].options = NULL;


	lzma_end(&strm);

	return 0;
}
