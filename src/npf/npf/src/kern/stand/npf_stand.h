/*-
 * Copyright (c) 2015 The NetBSD Foundation, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _NPF_STAND_H_
#define _NPF_STAND_H_

/*
 * This file contains wrappers of the kernel interfaces for the
 * standalone version of NPF.  These wrappers use inteded to be portable,
 * using the standard C99 or POSIX interfaces.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>
#include <limits.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include <assert.h>

#define	__FAVOR_BSD
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#if defined(__linux__)
#include <net/ethernet.h>
#endif

#include <prop/proplib.h>
#include <qsbr/qsbr.h>
#include <cdbr.h>

#ifdef USE_JUDY
#include <Judy.h>
#endif

#include "cext.h"

#include "mempool.h"
#include "ptree.h"
#include "rbtree.h"
#include "bpf.h"

/*
 * Synchronisation primitives (mutex, condvar, etc).
 */

#define	kmutex_t		pthread_mutex_t
#define	mutex_init(l, t, i)	pthread_mutex_init(l, NULL)
#define	mutex_enter(l)		pthread_mutex_lock(l)
#define	mutex_exit(l)		pthread_mutex_unlock(l)
#define	mutex_owned(l)		true
#define	mutex_destroy(l)	pthread_mutex_destroy(l)

#define	RW_READER		0
#define	RW_WRITER		1

#define	krwlock_t		pthread_rwlock_t
#define	rw_init(l)		pthread_rwlock_init(l, NULL)
#define	rw_destroy(l)		pthread_rwlock_destroy(l)
#define	rw_enter(l, op)		\
    (op) == RW_READER ? pthread_rwlock_rdlock(l) : pthread_rwlock_wrlock(l)
#define	rw_exit(l)		pthread_rwlock_unlock(l)

static inline int
npfkern_pthread_cond_timedwait(pthread_cond_t *t, pthread_mutex_t *l,
    const unsigned msec)
{
	const unsigned sec = msec / 1000;
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	ts.tv_sec += sec;
	ts.tv_nsec = (msec - (sec * 1000)) * 1000000;
	return pthread_cond_timedwait(t, l, &ts);
}

#define	kcondvar_t		pthread_cond_t
#define	cv_init(c, w)		pthread_cond_init(c, NULL)
#define	cv_broadcast(c)		pthread_cond_broadcast(c)
#define	cv_wait(c, l)		pthread_cond_wait(c, l)
#define	cv_timedwait(c, l, t)	npfkern_pthread_cond_timedwait(c, l, t)
#define	cv_signal(c)		pthread_cond_signal(c)
#define	cv_destroy(c)		pthread_cond_destroy(c)

/*
 * Passive serialization based on QSBR.
 */
typedef qsbr_t *		pserialize_t;

static inline void
npfkern_qsbr_wait(qsbr_t *qsbr)
{
	const struct timespec dtime = { 0, 1 * 1000 * 1000 }; /* 1 ms */
	qsbr_epoch_t target = qsbr_barrier(qsbr);

	while (!qsbr_sync(qsbr, target)) {
		(void)nanosleep(&dtime, NULL);
	}
}

#define	pserialize_create()	qsbr_create()
#define	pserialize_destroy(p)	qsbr_destroy(p)
#define	pserialize_register(p)	qsbr_register(p)
#define	pserialize_checkpoint(p) qsbr_checkpoint(p)
#define	pserialize_perform(p)	npfkern_qsbr_wait(p)
#define	pserialize_read_enter()	0x50505050
#define	pserialize_read_exit(s)	assert((s) == 0x50505050)

/*
 * Atomic operations and memory barriers.
 */

static inline void *
npfkern_atomic_swap_ptr(volatile void *ptr, void *nval)
{
	/* Solaris/NetBSD API uses *ptr, but it represents **ptr. */
	void * volatile *ptrp = (void * volatile *)ptr;
	volatile void *ptrval;
	void *oldval;

	do {
		ptrval = *ptrp;
		oldval = __sync_val_compare_and_swap(ptrp, *ptrp, nval);
	} while (oldval != ptrval);

	return oldval;
}

#define	membar_sync()		__sync_synchronize()
#define	membar_producer()	__sync_synchronize()
#define	atomic_inc_uint(x)	__sync_fetch_and_add(x, 1)
#define	atomic_dec_uint(x)	__sync_sub_and_fetch(x, 1)
#define	atomic_dec_uint_nv(x)	__sync_sub_and_fetch(x, 1)
#define	atomic_or_uint(x, v)	__sync_fetch_and_or(x, v)
#define	atomic_cas_32(p, o, n)	__sync_val_compare_and_swap(p, o, n)
#define	atomic_cas_ptr(p, o, n)	__sync_val_compare_and_swap(p, o, n)
#define atomic_swap_ptr(x, y)	npfkern_atomic_swap_ptr(x, y)

/*
 * Threads.
 */

typedef struct { pthread_t thr; } lwp_t;

static inline int
npfkern_pthread_create(lwp_t **lret, void (*func)(void *), void *arg)
{
	lwp_t *l;

	if ((l = calloc(1, sizeof(lwp_t))) == NULL)
		return ENOMEM;
	*lret = l;
	return pthread_create(&l->thr, NULL, (void *(*)(void *))func, arg);
}

#define	kthread_create(pri, flags, ci, func, arg, thr, fmt) \
    npfkern_pthread_create(thr, func, arg)
#define	kthread_join(t)	{ void *__r; pthread_join((t)->thr, &__r); free(t); }
#define	kthread_exit(x)	pthread_exit(NULL);

/*
 * Memory allocators and management.
 */

#define	KM_SLEEP	0x00000001
#define	KM_NOSLEEP	0x00000002
#define	PR_WAITOK	KM_SLEEP
#define PR_NOWAIT	KM_NOSLEEP

#ifndef pool_cache_t
typedef mempool_t *	pool_cache_t;
#endif

#define	pool_cache_init(size, align, a, b, c, d, p, e, f, g) \
    mempool_create(NULL, size, 64);
#define	pool_cache_destroy(p)		mempool_destroy(p)
#define	pool_cache_get(p, flags)	mempool_alloc((p), MEMP_WAITOK)
#define	pool_cache_put(p, obj)		mempool_free((p), (obj))
#define	pool_cache_invalidate(p)	mempool_cancel((p))

#define	kmem_zalloc(len, flags)		calloc(1, len)
#define	kmem_alloc(len, flags)		malloc(len)
#define	kmem_free(ptr, len)		free(ptr)
#define	kmem_intr_zalloc(len, flags)	kmem_zalloc(len, flags)
#define	kmem_intr_free(ptr, len)	kmem_free(ptr, len)

#define	kmalloc(size, type, flags)	calloc(1, (size))
#define	kfree(ptr, type)		free(ptr)

/*
 * FIXME/TODO: To be implemented using TLS ..
 */
#define	percpu_t			void
#define	percpu_alloc(s)			calloc(1, s)
#define	percpu_free(p, s)		free(p)
#define	percpu_getref(p)		p
#define	percpu_putref(p)
#define	percpu_foreach(p, f, b)		f(p, b, NULL)

static inline int
npfkern_copy(void *dst, const void *src, size_t len)
{
	memcpy(dst, src, len);
	return 0;
}
#define	copyout(k, u, l)		npfkern_copy(u, k, l)
#define	copyin(u, k, l)			npfkern_copy(k, u, l)
#define	copyinstr(u, k, l, d)		(0 & (int)(uintptr_t)strncpy(k, u, l))

/*
 * Random number generator.
 */

#define	cprng_fast32()			((uint32_t)random())

/*
 * Hashing.
 */

enum hashtype { HASH_LIST, HASH_SLIST, HASH_TAILQ };

void *		hashinit(u_int, enum hashtype, bool, u_long *);
void		hashdone(void *, enum hashtype, u_long);
uint32_t	murmurhash2(const void *, size_t, uint32_t);

/*
 * Time operations.
 */

#define	getnanouptime(ts)	clock_gettime(CLOCK_MONOTONIC, (ts))
#undef	mstohz
#define	mstohz(ms)		(ms)

static inline int
npfkern_kpause(const char *wmesg, bool intr, int timo, kmutex_t *mtx)
{
	const struct timespec req = { .tv_sec = 0, .tv_nsec = timo * 1000000 };
	(void)wmesg; (void)intr; (void)mtx;
	return nanosleep(&req, NULL);
}

#define	kpause(w, s, t, l)	npfkern_kpause(w, s, t, l)

/*
 * Networking.
 */

#ifndef IPV6_VERSION
#define IPV6_VERSION	0x60
#endif
#ifndef IPV6_DEFHLIM
#define IPV6_DEFHLIM	64
#endif

#define PFIL_ALL	(PFIL_IN|PFIL_OUT)
#define PFIL_IFADDR	0x00000008
#define PFIL_IFNET	0x00000010

#define	pfil_head_t	void

#define	MAX_TCPOPTLEN	40

/*
 * FIXME/TODO: To be implemented ..
 */
struct ifnet;
typedef struct ifnet ifnet_t;

#ifndef	IFNAMSIZ
#define	IFNAMSIZ	16
#endif

#define	ip_reass_packet(p, h)		ENOTSUP
#define	ip_output(m, a, b, c, d, e)	ENOTSUP
#define	icmp_error(m, t, c, a, b)
#define	in_cksum(m, len)	0

#define	ip_defttl		64
#define	max_linkhdr		0

#define	M_UNWRITABLE(m, l)	false

/*
 * Misc.
 */

#ifndef COHERENCY_UNIT
#define	COHERENCY_UNIT		CACHE_LINE_SIZE
#endif

#define	__read_mostly
#define	__cacheline_aligned
#define	__diagused
#ifndef	__dead
#define	__dead
#endif

#define	KASSERT			assert
#define	KASSERTMSG(e, m, ...)	assert(e)
#define	panic(x)		abort()

#define	KERNEL_LOCK(a, b)
#define	KERNEL_UNLOCK_ONE(a)

#define	MODULE(c, m, d)
#define	module_autoload(n, c)	ENOTSUP

#define	MODULE_CMD_INIT		1
#define	MODULE_CMD_FINI		2
#define	MODULE_CMD_AUTOLOAD	3
#define	MODULE_CMD_AUTOUNLOAD	4

typedef int modcmd_t;

#ifndef EPROGMISMATCH
#define	EPROGMISMATCH		ENOTSUP
#endif

#ifndef HASH32_BUF_INIT
#define	HASH32_BUF_INIT		5381
#define	hash32_buf(b, l, s)	murmurhash2(b, l, s)
#endif

#define	ffs32(x)		ffs(x)
#define	fls32(x)		flsl((unsigned long)(x))

struct cpu_info { unsigned id; };

#ifdef __linux__
static inline size_t
strlcpy(char *dst, const char *src, size_t len)
{
	char *p = stpncpy(dst, src, len);
	dst[len - 1] = '\0';
	return strlen(src);
}
#endif

#endif
