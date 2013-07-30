/* Copyright (c) 2013 Xingxing Ke <yykxx@hotmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _PLM_BUFFER_H
#define _PLM_BUFFER_H

#ifdef __cplusplus
extern "C" {
#endif

enum {
	MEM_8K,
	MEM_4K,
	MEM_2K,
	MEM_1K,
	MEM_END,
};

enum {
	SIZE_8K = 8192,
	SIZE_4K = 4096,
	SIZE_2K = 2048,
	SIZE_1K = 1024,
};

/* init buffers */
void plm_buffer_init(int thrdsafe, int tagchk, int zeromem);

/* alloc memory buffer 8k, 4k, 2k, 1k
 * @type -- buffer type
 * return address of buf on success or NULL on error
 */
char *plm_buffer_alloc(int type);

/* free memory */
void plm_buffer_free(int type, char *buf);

/* destroy pool and free all memory */
void plm_buffer_destroy();

#ifdef __cplusplus
}
#endif

#endif
