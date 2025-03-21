// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2013-2014, ARM Limited and Contributors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * Neither the name of ARM nor the names of its contributors may be used
 * to endorse or promote products derived from this software without specific
 * prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <linux/kernel.h>
#include "config.h"
#include "hyp_debug.h"

#define HYP_PL011_BASE_PHYS	HYP_DEBUG_UART_ADDR
#define HYP_PL011_UARTFR	0x18

#define HYP_PL011_UARTFR_BUSY	3
#define HYP_PL011_UARTFR_FULL	5
/* Choose max of 128 chars for now. */
#define PRINT_BUFFER_SIZE 128
extern unsigned long arm64_kvm_hyp_debug_uart_addr;

static inline void *__hyp_pl011_base(void)
{
	return (void *)arm64_kvm_hyp_debug_uart_addr;
}

static inline unsigned int __hyp_readw(void *ioaddr)
{
	unsigned int val;

	asm volatile("ldr %w0, [%1]" : "=r" (val) : "r" (ioaddr));
	return val;
}

static inline void __hyp_writew(unsigned int val, void *ioaddr)
{
	asm volatile("str %w0, [%1]" : : "r" (val), "r" (ioaddr));
}

static inline void hyp_putc(char c)
{
	unsigned int val;
	void *base = __hyp_pl011_base();

	do {
		val = __hyp_readw(base + HYP_PL011_UARTFR);
	} while (val & (1U << HYP_PL011_UARTFR_FULL));

	__hyp_writew(c, base);

	do {
		val = __hyp_readw(base + HYP_PL011_UARTFR);
	} while (val & (1U << HYP_PL011_UARTFR_BUSY));
}

int hyp_print(const char *fmt, ...)
{
	va_list args;
	char buf[PRINT_BUFFER_SIZE];
	int count;

	va_start(args, fmt);
	hyp_vsnprintf(buf, sizeof(buf) - 1, fmt, args);
	va_end(args);

	/* Use putchar directly as 'puts()' adds a newline. */
	buf[PRINT_BUFFER_SIZE - 1] = '\0';
	count = 0;
	while (buf[count]) {
		hyp_putc(buf[count]);
		count++;
	}

	return count;
}

int hyp_snprint(char *s, size_t slen, const char *format, ...)
{
	int retval;
	va_list ap;

	va_start(ap, format);
	retval = hyp_vsnprintf(s, slen, format, ap);
	va_end(ap);
	return retval;
}

int update_rb(struct shared_buffer *rb, u8 *buf, int cnt)
{
	int tmp;
	int size;

	size = (rb->ri + rb->size - rb->wi - 1) % rb->size;
	if (size < cnt)
		return -1;

	if (rb->wi + cnt > rb->size) {
		tmp =  rb->size - rb->wi;
		ops->memcpy(&rb->data[rb->wi], buf, tmp);
		ops->memcpy(&rb->data[(rb->wi + tmp) % rb->size], &buf[tmp], cnt - tmp);
	} else
		ops->memcpy(&rb->data[rb->wi], buf, cnt);
	barrier();
	rb->wi = (rb->wi + cnt) % rb->size;
	return cnt;
}

int hyp_dbg_print(const char *fmt, ...)
{
	va_list args;
	char buf[PRINT_BUFFER_SIZE];
	int cnt;
	int count = 0;
	struct shared_buffer *p = dbg_buffer;

	va_start(args, fmt);
	cnt = hyp_vsnprintf(buf, sizeof(buf) - 1, fmt, args);
	va_end(args);
	if (p) {
		return update_rb(p, buf, cnt);
	} else
		while (cnt--)
			hyp_putc(buf[count++]);

	return cnt;
}
