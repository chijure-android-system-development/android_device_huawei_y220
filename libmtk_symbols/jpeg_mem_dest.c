/*
 * jpeg_mem_dest() from libjpeg 8 (jdatadst.c), which MTK's libjpeg has and
 * CM's libjpeg 6b does not. The stock libcam.camshot.so encodes snapshots
 * into memory with it.
 *
 * Copyright (C) 1994-1996, Thomas G. Lane.
 * Modified 2009 by Guido Vollbeding.
 * This file is part of the Independent JPEG Group's software.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jpeglib.h"
#include "jerror.h"

#define OUTPUT_BUF_SIZE 4096

typedef struct {
    struct jpeg_destination_mgr pub;

    unsigned char **outbuffer;
    unsigned long *outsize;
    unsigned char *newbuffer;
    JOCTET *buffer;
    size_t bufsize;
} mem_destination_mgr;

typedef mem_destination_mgr *mem_dest_ptr;

static void init_mem_destination(j_compress_ptr cinfo)
{
    (void) cinfo;
}

/* Out of room: double the buffer, like libjpeg 8. */
static boolean empty_mem_output_buffer(j_compress_ptr cinfo)
{
    mem_dest_ptr dest = (mem_dest_ptr) cinfo->dest;
    size_t nextsize = dest->bufsize * 2;
    JOCTET *nextbuffer = (JOCTET *) malloc(nextsize);

    if (nextbuffer == NULL)
        ERREXIT1(cinfo, JERR_OUT_OF_MEMORY, 10);

    memcpy(nextbuffer, dest->buffer, dest->bufsize);
    if (dest->newbuffer != NULL)
        free(dest->newbuffer);
    dest->newbuffer = nextbuffer;

    dest->pub.next_output_byte = nextbuffer + dest->bufsize;
    dest->pub.free_in_buffer = dest->bufsize;

    dest->buffer = nextbuffer;
    dest->bufsize = nextsize;
    return TRUE;
}

static void term_mem_destination(j_compress_ptr cinfo)
{
    mem_dest_ptr dest = (mem_dest_ptr) cinfo->dest;

    *dest->outbuffer = dest->buffer;
    *dest->outsize = dest->bufsize - dest->pub.free_in_buffer;
}

void jpeg_mem_dest(j_compress_ptr cinfo, unsigned char **outbuffer,
                   unsigned long *outsize)
{
    mem_dest_ptr dest;

    if (outbuffer == NULL || outsize == NULL)
        ERREXIT(cinfo, JERR_BUFFER_SIZE);

    if (cinfo->dest == NULL) {
        cinfo->dest = (struct jpeg_destination_mgr *)
            (*cinfo->mem->alloc_small)((j_common_ptr) cinfo, JPOOL_PERMANENT,
                                       sizeof(mem_destination_mgr));
    }

    dest = (mem_dest_ptr) cinfo->dest;
    dest->pub.init_destination = init_mem_destination;
    dest->pub.empty_output_buffer = empty_mem_output_buffer;
    dest->pub.term_destination = term_mem_destination;
    dest->outbuffer = outbuffer;
    dest->outsize = outsize;
    dest->newbuffer = NULL;

    if (*outbuffer == NULL || *outsize == 0) {
        /* Allocate the initial buffer. */
        dest->newbuffer = *outbuffer = (unsigned char *) malloc(OUTPUT_BUF_SIZE);
        if (dest->newbuffer == NULL)
            ERREXIT1(cinfo, JERR_OUT_OF_MEMORY, 10);
        *outsize = OUTPUT_BUF_SIZE;
    }

    dest->pub.next_output_byte = dest->buffer = *outbuffer;
    dest->pub.free_in_buffer = dest->bufsize = *outsize;
}
