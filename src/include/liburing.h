/**
 * @mainpage liburing.h
 * @file liburing.h
 * @brief API simpler to use and understand for io_uring.
 */
/* SPDX-License-Identifier: MIT */
#ifndef LIB_URING_H
#define LIB_URING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/socket.h>
#include <sys/uio.h>
#include <signal.h>
#include <inttypes.h>
#include <time.h>
#include "liburing/compat.h"
#include "liburing/io_uring.h"
#include "liburing/barrier.h"

/*
 * Library interface to io_uring
 */
struct io_uring_sq {
	unsigned *khead;
	unsigned *ktail;
	unsigned *kring_mask;
	unsigned *kring_entries;
	unsigned *kflags;
	unsigned *kdropped;
	unsigned *array;
	struct io_uring_sqe *sqes;

	unsigned sqe_head;
	unsigned sqe_tail;

	size_t ring_sz;
	void *ring_ptr;
};

struct io_uring_cq {
	unsigned *khead;
	unsigned *ktail;
	unsigned *kring_mask;
	unsigned *kring_entries;
	unsigned *koverflow;
	struct io_uring_cqe *cqes;

	size_t ring_sz;
	void *ring_ptr;
};

struct io_uring {
	struct io_uring_sq sq;
	struct io_uring_cq cq;
	unsigned flags;
	int ring_fd;
};

/*
 * Library interface
 */

/**
 * Get probe for ring.
 * @param[in,out] ring    io_uring queue.
 * @return an allocated io_uring_probe structure, or NULL if probe fails (for
 * example, if it is not available). The caller is responsible for freeing it.
 */
extern struct io_uring_probe *io_uring_get_probe_ring(struct io_uring *ring);

/**
 * Get probe.
 * Same as io_uring_get_probe_ring, but takes care of ring init and teardown.
 * @param[in,out] ring    io_uring queue.
 * @return an allocated io_uring_probe structure, or NULL if probe fails (for
 * example, if it is not available). The caller is responsible for freeing it.
 */
extern struct io_uring_probe *io_uring_get_probe(void);

/**
 * Check whether opcode is supported or not.
 * @param[in] ring    io_uring queue.
 * @param[in] op      opcode.
 * @return 1 if supported, 0 otherwise.
 */
static inline int io_uring_opcode_supported(struct io_uring_probe *p, int op)
{
	if (op > p->last_op)
		return 0;
	return (p->ops[op].flags & IO_URING_OP_SUPPORTED) != 0;
}

/**
 * Initialize queue with params.
 * @param[in]     entries number of entries in queue.
 * @param[in,out] ring    io_uring queue.
 * @param[in]     p       params.
 * @return 0 on success, -errno on failure.
 */
extern int io_uring_queue_init_params(unsigned entries, struct io_uring *ring,
	struct io_uring_params *p);

/**
 * Initialize queue with flags.
 * @param[in]     entries number of entries in queue.
 * @param[in,out] ring    io_uring queue.
 * @param[in]     flags   flags.
 * @return 0 on success, -errno on failure.
 */
extern int io_uring_queue_init(unsigned entries, struct io_uring *ring,
	unsigned flags);

/**
 * Memory map queue.
 * @param[in]     fd      file descriptor.
 * @param[in]     p       params.
 * @param[in,out] ring    io_uring queue.
 * @return 0 on success, -errno on failure.
 */
extern int io_uring_queue_mmap(int fd, struct io_uring_params *p,
	struct io_uring *ring);

/**
 * Ensure that the mmap'ed rings aren't available to a child after a fork(2).
 * This uses madvise(..., MADV_DONTFORK) on the mmap'ed ranges.
 * @param[in,out] ring    io_uring queue.
 * @return 0 on success, -errno on failure.
 */
extern int io_uring_ring_dontfork(struct io_uring *ring);

/**
 * Teardown queue.
 * @param[in,out] ring    io_uring queue.
 */
extern void io_uring_queue_exit(struct io_uring *ring);

/**
 * Fill in an array of IO completions up to count, if any are available.
 * @param[in,out] ring    io_uring queue.
 * @param[out]    cqes    completion queue entries.
 * @param[in]     count   fill entires up to this count.
 * @return the amount of IO completions filled.
 */
unsigned io_uring_peek_batch_cqe(struct io_uring *ring,
	struct io_uring_cqe **cqes, unsigned count);

/**
 * Return an IO completion, waiting for it if necessary.
 * Like io_uring_wait_cqe(), except it accepts a timeout value as well. Note
 * that an sqe is used internally to handle the timeout. Applications using
 * this function must never set sqe->user_data to LIBURING_UDATA_TIMEOUT!
 *
 * If 'ts' is specified, the application need not call io_uring_submit() before
 * calling this function, as we will do that on its behalf. From this it also
 * follows that this function isn't safe to use for applications that split SQ
 * and CQ handling between two threads and expect that to work without
 * synchronization, as this function manipulates both the SQ and CQ side.
 *
 * @param[in,out] ring    an io_uring queue.
 * @param[out]    cqe_ptr completion queue entries.
 * @param[in]     wait_nr number of completions to wait.
 * @param[in]     ts      timeout.
 * @param[in]     sigmask mask for signal set.
 * @return 0 with cqe_ptr filled in on success, -errno on failure.
 */
extern int io_uring_wait_cqes(struct io_uring *ring,
	struct io_uring_cqe **cqe_ptr, unsigned wait_nr,
	struct __kernel_timespec *ts, sigset_t *sigmask);

/**
 * Return an IO completion, waiting for it if necessary.
 * See io_uring_wait_cqes() - this function is the same, it just always uses
 * '1' as the wait_nr.
 * @param[in,out] ring    an io_uring queue.
 * @param[out]    cqe_ptr completion queue entries.
 * @param[in]     ts      timeout.
 * @return 0 with cqe_ptr filled in on success, -errno on failure.
 */
extern int io_uring_wait_cqe_timeout(struct io_uring *ring,
	struct io_uring_cqe **cqe_ptr, struct __kernel_timespec *ts);

/**
 * Submit sqes acquired from io_uring_get_sqe() to the kernel.
 *
 * @param[in,out] ring    an io_uring queue.
 * @return number of sqes submitted on success, -errno on failure.
 */
extern int io_uring_submit(struct io_uring *ring);

/**
 * Submit sqes acquired from io_uring_get_sqe() to the kernel and wait for
 * events.
 * Like io_uring_submit(), but allows waiting for events as well.
 *
 * @param[in,out] ring    an io_uring queue.
 * @param[in]     wait_nr number of completions to wait.
 * @return number of sqes submitted on success, -errno on failure.
 */
extern int io_uring_submit_and_wait(struct io_uring *ring, unsigned wait_nr);

/**
 * Return an sqe to fill. Application must later call io_uring_submit()
 * when it's ready to tell the kernel about it. The caller may call this
 * function multiple times before calling io_uring_submit().
 *
 * @param[in,out] ring    an io_uring queue.
 * @return a vacant sqe, or NULL if we're full.
 */
extern struct io_uring_sqe *io_uring_get_sqe(struct io_uring *ring);

/**
 * Register a fixed set of buffers for IO.
 *
 * @param[in,out] ring       an io_uring queue.
 * @param[in]     iovecs     a fixed set of iovec buffers.
 * @param[in]     nr_iovecs  a number of iovec buffers.
 * @return 0 on success, -errno on failure.
 */
extern int io_uring_register_buffers(struct io_uring *ring,
					const struct iovec *iovecs,
					unsigned nr_iovecs);
/**
 * Unregister a fixed set of buffers for IO which were registered with
 * io_uring_register_buffers.
 *
 * @param[in,out] ring      an io_uring queue.
 * @return 0 on success, -errno on failure.
 */
extern int io_uring_unregister_buffers(struct io_uring *ring);

/**
 * Register a file-set.
 *
 * @param[in,out] ring      an io_uring queue.
 * @param[in]     files     an array of file descriptors that the application
 *                          has already open.
 * @param[in]     nr_files  the size of the array of file descriptors.
 * @return 0 on success, -errno on failure.
 */
extern int io_uring_register_files(struct io_uring *ring, const int *files,
					unsigned nr_files);
/**
 * Unregister a file-set which were registered with io_uring_register_files.
 *
 * @param[in,out] ring      an io_uring queue.
 * @return 0 on success, -errno on failure.
 */
extern int io_uring_unregister_files(struct io_uring *ring);

/**
 * Register an update for an existing file set. The updates will start at
 * 'off' in the original array, and 'nr_files' is the number of files we'll
 * update.
 *
 * @param[in,out] ring      an io_uring queue.
 * @param[in]     off       an offset in the original file descriptor array.
 * @param[in]     files     an array of file descriptors that the application
 *                          has already open.
 * @param[in]     nr_files  the size of the array of file descriptors.
 * @return number of files updated on success, -errno on failure.
 */
extern int io_uring_register_files_update(struct io_uring *ring, unsigned off,
					int *files, unsigned nr_files);
/**
 * Register an event file descriptor.
 *
 * @param[in,out] ring      an io_uring queue.
 * @param[in]     fd        an event file descriptor.
 * @return 0 on success, -errno on failure.
 */
extern int io_uring_register_eventfd(struct io_uring *ring, int fd);
/**
 * Register an event file descriptor asynchronously.
 *
 * @param[in,out] ring      an io_uring queue.
 * @param[in]     fd        an event file descriptor.
 * @return 0 on success, -errno on failure.
 */
extern int io_uring_register_eventfd_async(struct io_uring *ring, int fd);
/**
 * Unegister an event file descriptor.
 *
 * @param[in,out] ring      an io_uring queue.
 * @return 0 on success, -errno on failure.
 */
extern int io_uring_unregister_eventfd(struct io_uring *ring);
/**
 * Register probes.
 * 
 * @param[in,out] ring      an io_uring queue.
 * @param[in]     p         an array of probes. TODO: Confirm this.
 * @param[in]     nr        the size of the array of probes.
 * @return 0 on success, -errno on failure.
 */
extern int io_uring_register_probe(struct io_uring *ring,
					struct io_uring_probe *p, unsigned nr);
/**
 * Register personality. TODO: Figure out what is personality.
 *
 * @param[in,out] ring      an io_uring queue.
 * @return 0 on success, -errno on failure.
 */
extern int io_uring_register_personality(struct io_uring *ring);
/**
 * Unregister personality.
 *
 * @param[in,out] ring      an io_uring queue.
 * @param[in]     fd        a file descriptor. TODO: for what?
 * @return 0 on success, -errno on failure.
 */
extern int io_uring_unregister_personality(struct io_uring *ring, int id);

/**
 * Helper for the peek/wait single cqe functions. Exported because of that,
 * but probably shouldn't be used directly in an application.
 */
extern int __io_uring_get_cqe(struct io_uring *ring,
			      struct io_uring_cqe **cqe_ptr, unsigned submit,
			      unsigned wait_nr, sigset_t *sigmask);

#define LIBURING_UDATA_TIMEOUT	((__u64) -1)

/**
 * A macro for a for-loop for each cqe.
 *
 * @param[in,out] ring      an io_uring queue.
 * @param[out]     head     a variable for cqe list head.
 * @param[out]     cqe      a variable for cqe.
 */
#define io_uring_for_each_cqe(ring, head, cqe)				\
	/*								\
	 * io_uring_smp_load_acquire() enforces the order of tail	\
	 * and CQE reads.						\
	 */								\
	for (head = *(ring)->cq.khead;					\
	     (cqe = (head != io_uring_smp_load_acquire((ring)->cq.ktail) ? \
		&(ring)->cq.cqes[head & (*(ring)->cq.kring_mask)] : NULL)); \
	     head++)							\

/**
 * Tell the kernel a number of cq entries processed by the application.
 *
 * Must be called after io_uring_for_each_cqe()
 * @param[in,out] ring      an io_uring queue.
 * @param[in]     nr        a number of cq entries to advance.
 */
static inline void io_uring_cq_advance(struct io_uring *ring,
				       unsigned nr)
{
	if (nr) {
		struct io_uring_cq *cq = &ring->cq;

		/*
		 * Ensure that the kernel only sees the new value of the head
		 * index after the CQEs have been read.
		 */
		io_uring_smp_store_release(cq->khead, *cq->khead + nr);
	}
}

/**
 * Tell the kernel the cqe has been processed by the application.
 * Must be called after io_uring_{peek,wait}_cqe() after the cqe has
 * been processed by the application.
 *
 * @param[in,out] ring      an io_uring queue.
 * @param[in]     cqe       a completion queue entry.
 */
static inline void io_uring_cqe_seen(struct io_uring *ring,
				     struct io_uring_cqe *cqe)
{
	if (cqe)
		io_uring_cq_advance(ring, 1);
}

/*
 * Command prep helpers
 */
/**
 * Set a user data to a sq entry.
 * The user data can be retrieved with io_uring_cqe_get_data when completed.
 *
 * @param[in,out] sqe        a submission queue entry.
 * @param[in]     data       a user data.
 */
static inline void io_uring_sqe_set_data(struct io_uring_sqe *sqe, void *data)
{
	sqe->user_data = (unsigned long) data;
}

/** 
 * Get user data from a cq entry which was set wth io_uring_sqe_set_data.
 *
 * @param[in]     cqe        a completion queue entry.
 * @return the user data which was set wth io_uring_sqe_set_data.
 */
static inline void *io_uring_cqe_get_data(const struct io_uring_cqe *cqe)
{
	return (void *) (uintptr_t) cqe->user_data;
}

/**
 * Set flags to a sq entry.
 *
 * @param[in,out] sqe        a submission queue entry.
 * @param[in]     flags      flags to set.
 */
static inline void io_uring_sqe_set_flags(struct io_uring_sqe *sqe,
					  unsigned flags)
{
	sqe->flags = flags;
}

/**
 * A helper function to prepare a read or write for a sq entry.
 *
 * @param[in]     op         an opcode.
 * @param[in,out] sqe        a submission queue entry.
 * @param[in]     fd         a file descriptor. used for other value in some operations.
 * @param[in]     addr       an address. used for other value in some operations.
 * @param[in]     len        a length. used for other value in some operations.
 * @param[in]     offset     an offset. used for other value in some operations.
 */
static inline void io_uring_prep_rw(int op, struct io_uring_sqe *sqe, int fd,
				    const void *addr, unsigned len,
				    __u64 offset)
{
	sqe->opcode = op;
	sqe->flags = 0;
	sqe->ioprio = 0;
	sqe->fd = fd;
	sqe->off = offset;
	sqe->addr = (unsigned long) addr;
	sqe->len = len;
	sqe->rw_flags = 0;
	sqe->user_data = 0;
	sqe->__pad2[0] = sqe->__pad2[1] = sqe->__pad2[2] = 0;
}

/**
 * Prepare a splice operation for a sq entry.
 * See [IORING_OP_SPLICE], [io_splice_prep], and [io_splice] for processing this operation in the kernel.
 *
 * @param[in,out] sqe           a submission queue entry.
 * @param[in]     fd_in         a file descriptor for an input. will be used to set @b file_in in [struct io_splice].
 * @param[in]     off_in        an offset in the input. will be set to @b off_in in [struct io_splice].
 * @param[in]     fd_out        a file descriptor for an output. will be used to set @b file_out in [struct io_splice].
 * @param[in]     off_out       an offset in the output. will be set to @b off_out in [struct io_splice].
 * @param[in]     nbytes        a number of bytes. will be set to @b len in [struct io_splice].
 * @param[in]     splice_flags  flags for splice. will be set to @b flags in [struct io_splice].
 *
 * [IORING_OP_SPLICE]: https://elixir.bootlin.com/linux/v5.7-rc2/ident/IORING_OP_SPLICE
 * [io_splice_prep]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L2729
 * [io_splice]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L2768
 * [struct io_splice]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L456
 */
static inline void io_uring_prep_splice(struct io_uring_sqe *sqe,
					int fd_in, loff_t off_in,
					int fd_out, loff_t off_out,
					unsigned int nbytes,
					unsigned int splice_flags)
{
	io_uring_prep_rw(IORING_OP_SPLICE, sqe, fd_out, NULL, nbytes, off_out);
	sqe->splice_off_in = off_in;
	sqe->splice_fd_in = fd_in;
	sqe->splice_flags = splice_flags;
}

/**
 * Prepare a readv operation for a sq entry.
 *
 * See [IORING_OP_READV], [io_read_prep], [io_prep_rw], and [io_read] for processing this operation in the kernel.
 *
 * @param[in,out] sqe        a submission queue entry.
 * @param[in]     fd         a file descriptor. will be used to set @b file in [struct io_kiocb].
 * @param[in]     iovecs     a fixed set of iovec buffers. will be set to @b addr in [struct io_rw] @b rw in [struct io_kiocb].
 * @param[in]     nr_iovecs  a number of iovec buffers. will be set to @b len in [struct io_rw] @b rw in [struct io_kiocb].
 * @param[in]     offset     an offset in the input. will be set to @b ki_pos in [struct kiocb] @b kiocb in [struct io_rw] @b rw in [struct io_kiocb].
 *
 * [IORING_OP_READV]: https://elixir.bootlin.com/linux/v5.7-rc2/ident/IORING_OP_READV
 * [io_read_prep]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L2519
 * [io_prep_rw]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L2049
 * [io_read]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L2549
 * [struct io_kiocb]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L593
 * [struct io_rw]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L390
 * [struct kiocb]: https://elixir.bootlin.com/linux/v5.7-rc2/source/include/linux/fs.h#L318
 */
static inline void io_uring_prep_readv(struct io_uring_sqe *sqe, int fd,
				       const struct iovec *iovecs,
				       unsigned nr_vecs, off_t offset)
{
	io_uring_prep_rw(IORING_OP_READV, sqe, fd, iovecs, nr_vecs, offset);
}

/**
 * Prepare a read operation using a fixed buffer for a sq entry.
 *
 * See [IORING_OP_READ_FIXED], [io_read_prep], [io_prep_rw], and [io_read] for processing this operation in the kernel.
 *
 * @param[in,out] sqe        a submission queue entry.
 * @param[in]     fd         a file descriptor. will be used to set @b file in [struct io_kiocb].
 * @param[in]     buf        a buffer which the read result will be written to. will be set to @b addr in [struct io_rw] @b rw in [struct io_kiocb].
 * @param[in]     nbytes     the maximum number of bytes to read. will be set to @b len in [struct io_rw] @b rw in [struct io_kiocb].
 * @param[in]     offset     an offset in the input. will be set to @b ki_pos in [struct kiocb] @b kiocb in [struct io_rw] @b rw in [struct io_kiocb].
 * @param[in]     buf_index  the index of the fixed buffer to use. will be set to @b private in [struct kiocb] @b kiocb in [struct io_rw] @b rw in [struct io_kiocb].
 *
 * [IORING_OP_READ_FIXED]: https://elixir.bootlin.com/linux/v5.7-rc2/ident/IORING_OP_READ_FIXED
 * [io_read_prep]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L2519
 * [io_prep_rw]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L2049
 * [io_read]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L2549
 * [struct io_kiocb]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L593
 * [struct io_rw]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L390
 * [struct kiocb]: https://elixir.bootlin.com/linux/v5.7-rc2/source/include/linux/fs.h#L318
 */
static inline void io_uring_prep_read_fixed(struct io_uring_sqe *sqe, int fd,
					    void *buf, unsigned nbytes,
					    off_t offset, int buf_index)
{
	io_uring_prep_rw(IORING_OP_READ_FIXED, sqe, fd, buf, nbytes, offset);
	sqe->buf_index = buf_index;
}

/**
 * Prepare a writev operation for a sq entry.
 *
 * See [IORING_OP_WRITEV], [io_write_prep], [io_prep_rw], and [io_write] for processing this operation in the kernel.
 *
 * @param[in,out] sqe        a submission queue entry.
 * @param[in]     fd         a file descriptor. will be used to set @b file in [struct io_kiocb].
 * @param[in]     iovecs     a fixed set of iovec buffers. will be set to @b addr in [struct io_rw] @b rw in [struct io_kiocb].
 * @param[in]     nr_iovecs  a number of iovec buffers. will be set to @b len in [struct io_rw] @b rw in [struct io_kiocb].
 * @param[in]     offset     an offset in the input. will be set to @b ki_pos in [struct kiocb] @b kiocb in [struct io_rw] @b rw in [struct io_kiocb].
 *
 * [IORING_OP_WRITEV]: https://elixir.bootlin.com/linux/v5.7-rc2/ident/IORING_OP_WRITEV
 * [io_write_prep]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L2608
 * [io_write]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L2640
 * [struct io_kiocb]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L593
 * [struct io_rw]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L390
 * [struct kiocb]: https://elixir.bootlin.com/linux/v5.7-rc2/source/include/linux/fs.h#L318
 */
static inline void io_uring_prep_writev(struct io_uring_sqe *sqe, int fd,
					const struct iovec *iovecs,
					unsigned nr_vecs, off_t offset)
{
	io_uring_prep_rw(IORING_OP_WRITEV, sqe, fd, iovecs, nr_vecs, offset);
}

/**
 * Prepare a write operation using a fixed buffer for a sq entry.
 *
 * See [IORING_OP_WRITE_FIXED], [io_write_prep], [io_prep_rw], and [io_write] for processing this operation in the kernel.
 *
 * @param[in,out] sqe        a submission queue entry.
 * @param[in]     fd         a file descriptor. will be used to set @b file in [struct io_kiocb].
 * @param[in]     buf        a buffer which the read result will be written to. will be set to @b addr in [struct io_rw] @b rw in [struct io_kiocb].
 * @param[in]     nbytes     the maximum number of bytes to read. will be set to @b len in [struct io_rw] @b rw in [struct io_kiocb].
 * @param[in]     offset     an offset in the input. will be set to @b ki_pos in [struct kiocb] @b kiocb in [struct io_rw] @b rw in [struct io_kiocb].
 * @param[in]     buf_index  the index of the fixed buffer to use. will be set to @b private in [struct kiocb] @b kiocb in [struct io_rw] @b rw in [struct io_kiocb].
 *
 * [IORING_OP_WRITE_FIXED]: https://elixir.bootlin.com/linux/v5.7-rc2/ident/IORING_OP_WRITE_FIXED
 * [io_write_prep]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L2608
 * [io_prep_rw]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L2049
 * [io_write]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L2640
 * [struct io_kiocb]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L593
 * [struct io_rw]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L390
 * [struct kiocb]: https://elixir.bootlin.com/linux/v5.7-rc2/source/include/linux/fs.h#L318
 */
static inline void io_uring_prep_write_fixed(struct io_uring_sqe *sqe, int fd,
					     const void *buf, unsigned nbytes,
					     off_t offset, int buf_index)
{
	io_uring_prep_rw(IORING_OP_WRITE_FIXED, sqe, fd, buf, nbytes, offset);
	sqe->buf_index = buf_index;
}

/**
 * Prepare a recvmsg operation for a sq entry.
 * See [IORING_OP_RECVMSG], [io_recvmsg_prep], and [io_recvmsg] for processing this operation in the kernel.
 *
 * @param[in,out] sqe        a submission queue entry.
 * @param[in]     fd         a file descriptor for the socket to receive from. will be used to set @b file in [struct io_sr_msg].
 * @param[in,out] msg        a message. will be set to @b msg in [struct io_sr_msg].
 * @param[in]     flags      flags. will be set to @b msg_flags in [struct io_sr_msg].
 *
 * [IORING_OP_RECVMSG]: https://elixir.bootlin.com/linux/v5.7-rc2/ident/IORING_OP_RECVMSG
 * [io_recvmsg_prep]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L3775
 * [io_recvmsg]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L3804
 * [struct io_sr_msg]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L403
 */
static inline void io_uring_prep_recvmsg(struct io_uring_sqe *sqe, int fd,
					 struct msghdr *msg, unsigned flags)
{
	io_uring_prep_rw(IORING_OP_RECVMSG, sqe, fd, msg, 1, 0);
	sqe->msg_flags = flags;
}

/**
 * Prepare a sendmsg operation for a sq entry.
 * See [IORING_OP_SENDMSG], [io_sendmsg_prep], and [io_sendmsg] for processing this operation in the kernel.
 *
 * @param[in,out] sqe        a submission queue entry.
 * @param[in]     fd         a file descriptor for the socket to send to. will be used to set @b file in [struct io_sr_msg].
 * @param[in,out] msg        a message. will be set to @b msg in [struct io_sr_msg].
 * @param[in]     flags      flags. will be set to @b msg_flags in [struct io_sr_msg].
 *
 * [IORING_OP_SENDMSG]: https://elixir.bootlin.com/linux/v5.7-rc2/ident/IORING_OP_SENDMSG
 * [io_sendmsg_prep]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L403
 * [io_sendmsg]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L403
 * [struct io_sr_msg]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L403
 */
static inline void io_uring_prep_sendmsg(struct io_uring_sqe *sqe, int fd,
					 const struct msghdr *msg, unsigned flags)
{
	io_uring_prep_rw(IORING_OP_SENDMSG, sqe, fd, msg, 1, 0);
	sqe->msg_flags = flags;
}

/**
 * Prepare a poll_add operation for a sq entry.
 *
 * You need to set a user_data to the @b sqe with io_uring_sqe_set_data if you want to do a poll_remove operation later.
 *
 * See [IORING_OP_POLL_ADD], [io_poll_add_prep], and [io_poll_add] for processing this operation in the kernel.
 *
 * @param[in,out] sqe        a submission queue entry.
 * @param[in]     fd         a file descriptor. will be used to set @b file in [struct io_poll_iocb].
 * @param[in]     poll_mask  a mask for the poll. will be used to set @b events in [struct io_poll_iocb].
 *
 * [IORING_OP_POLL_ADD]: https://elixir.bootlin.com/linux/v5.7-rc2/ident/IORING_OP_POLL_ADD
 * [io_poll_add_prep]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L4522
 * [io_poll_add]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L4542
 * [struct io_poll_iocb]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L337
 */
static inline void io_uring_prep_poll_add(struct io_uring_sqe *sqe, int fd,
					  short poll_mask)
{
	io_uring_prep_rw(IORING_OP_POLL_ADD, sqe, fd, NULL, 0, 0);
	sqe->poll_events = poll_mask;
}

/**
 * Prepare a poll_remove operation for a sq entry.
 *
 * See [IORING_OP_POLL_REMOVE], [io_poll_remove_prep], and [io_poll_remove] for processing this operation in the kernel.
 *
 * @param[in,out] sqe        a submission queue entry.
 * @param[in]     fd         a file descriptor. will be used to set @b file in [struct io_poll_iocb].
 * @param[in]     user_data  the user data which was set to the poll_add sqe. will be set to @b addr in [struct io_poll_iocb].
 *
 * [IORING_OP_POLL_REMOVE]: https://elixir.bootlin.com/linux/v5.7-rc2/ident/IORING_OP_POLL_REMOVE
 * [io_poll_remove_prep]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L4427
 * [io_poll_remove]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L4444
 * [struct io_poll_iocb]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L337
 */
static inline void io_uring_prep_poll_remove(struct io_uring_sqe *sqe,
					     void *user_data)
{
	io_uring_prep_rw(IORING_OP_POLL_REMOVE, sqe, -1, user_data, 0, 0);
}

/**
 * Prepare a fsync operation for a sq entry.
 * See [IORING_OP_FSYNC], [io_prep_fsync], and [io_fsync] for processing this operation in the kernel.
 *
 * @param[in,out] sqe          a submission queue entry.
 * @param[in]     fd           a file descriptor. will be used to set @b file in [struct io_fsync].
 * @param[in]     fsync_flags  flags for the fsync. will be set to @b flags in [struct io_fsync].
 *
 * [IORING_OP_FSYNC]: https://elixir.bootlin.com/linux/v5.7-rc2/ident/IORING_OP_FSYNC
 * [io_prep_fsync]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L2814
 * [io_fsync]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L2871
 * [struct io_fsync]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L370
 */
static inline void io_uring_prep_fsync(struct io_uring_sqe *sqe, int fd,
				       unsigned fsync_flags)
{
	io_uring_prep_rw(IORING_OP_FSYNC, sqe, fd, NULL, 0, 0);
	sqe->fsync_flags = fsync_flags;
}

/**
 * Prepare a nop operation for a sq entry.
 * See [IORING_OP_NOP] and [io_nop] for processing this operation in the kernel.
 *
 * @param[in,out] sqe          a submission queue entry.
 *
 * [IORING_OP_NOP]: https://elixir.bootlin.com/linux/v5.7-rc2/ident/IORING_OP_NOP
 * [io_nop]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L2802
 */
static inline void io_uring_prep_nop(struct io_uring_sqe *sqe)
{
	io_uring_prep_rw(IORING_OP_NOP, sqe, -1, NULL, 0, 0);
}

/**
 * Prepare a timeout operation for a sq entry.
 *
 * Note you need to set the user data with the sq entry if you would like to cancel
 * the timeout event later using a timeout_remove operation.
 *
 * See [IORING_OP_TIMEOUT], [io_timeout_prep], and [io_timeout] for processing this operation in the kernel.
 *
 * @param[in,out] sqe          a submission queue entry.
 * @param[in]     ts           a timespec for the timeout. will be set to @b ts in
 *                             [struct io_timeout_data].
 * @param[in]     count        a count. will be set to @b count in [struct io_timeout_data].
 * @param[in]     flags        flags for the timeout. if flags contain
 *                             IORING_TIMEOUT_ABS, @b ts will be treated as an
 *                             absolute timestamp.
 *                             will be used to set @b mode in [struct io_timeout_data].
 *
 * [IORING_OP_TIMEOUT]: https://elixir.bootlin.com/linux/v5.7-rc2/ident/IORING_OP_TIMEOUT
 * [io_timeout_prep]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L4672
 * [io_timeout]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L4709
 * [struct io_timeout_data]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L355
 */
static inline void io_uring_prep_timeout(struct io_uring_sqe *sqe,
					 struct __kernel_timespec *ts,
					 unsigned count, unsigned flags)
{
	io_uring_prep_rw(IORING_OP_TIMEOUT, sqe, -1, ts, 1, count);
	sqe->timeout_flags = flags;
}

/**
 * Prepare a timeout_remove operation for a sq entry.
 *
 * See [IORING_OP_TIMEOUT_REMOVE], [io_timeout_remove_prep], and [io_timeout_remove] for processing this operation in the kernel.
 *
 * @param[in,out] sqe          a submission queue entry.
 * @param[in]     user_data    the user data which was set to the sqe for the
 *                             timeout operation, in the other words, the
 *                             timeout event which has the same user data will
 *                             be canceled. will be set to @b addr in
 *                             [struct io_timeout_data]
 * @param[in]     flags        flags. must be zero. will be set to @b flats in
 *                             [struct io_timeout_data]
 *
 * [IORING_OP_TIMEOUT_REMOVE]: https://elixir.bootlin.com/linux/v5.7-rc2/ident/IORING_OP_TIMEOUT_REMOVE
 * [io_timeout_remove_prep]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L4635
 * [io_timeout_remove]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L4654
 * [struct io_timeout_data]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L355
 */
static inline void io_uring_prep_timeout_remove(struct io_uring_sqe *sqe,
						__u64 user_data, unsigned flags)
{
	io_uring_prep_rw(IORING_OP_TIMEOUT_REMOVE, sqe, -1,
				(void *)(unsigned long)user_data, 0, 0);
	sqe->timeout_flags = flags;
}

/**
 * Prepare an accept operation for a sq entry.
 *
 * See [IORING_OP_ACCEPT], [io_accept_prep], and [io_accept] for processing this operation in the kernel.
 *
 * @param[in,out] sqe          a submission queue entry.
 * @param[in]     fd           a socket file descriptor. will be used to set @b file in [struct io_accept].
 * @param[in]     addr         an address for the socket. will be set to @b addr in [struct io_accept].
 * @param[in]     addrlen      the length of addr. will be set to @b addr_len in [struct io_accept].
 * @param[in]     flags        flags. will be set to @b flags in [struct io_accept].
 *
 * [IORING_OP_ACCEPT]: https://elixir.bootlin.com/linux/v5.7-rc2/ident/IORING_OP_ACCEPT
 * [io_accept_prep]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L3928
 * [io_accept]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L3975
 * [struct io_accept]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L362
 */
static inline void io_uring_prep_accept(struct io_uring_sqe *sqe, int fd,
					struct sockaddr *addr,
					socklen_t *addrlen, int flags)
{
	io_uring_prep_rw(IORING_OP_ACCEPT, sqe, fd, addr, 0,
				(__u64) (unsigned long) addrlen);
	sqe->accept_flags = flags;
}

/**
 * Prepare a cancel operation for a sq entry.
 *
 * See [IORING_OP_ASYNC_CANCEL], [io_async_cancel_prep], and [io_async_cancel] for processing this operation in the kernel.
 *
 * @param[in,out] sqe          a submission queue entry.
 * @param[in]     user_data    the address of the @b sqe to cancel.
 *                             will be set to @b addr in [struct io_cancel].
 * @param[in]     flags        flags. must be zero.
 *
 * [IORING_OP_ASYNC_CANCEL]: https://elixir.bootlin.com/linux/v5.7-rc2/ident/IORING_OP_ASYNC_CANCEL
 * [io_async_cancel_prep]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L4841
 * [io_async_cancel]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L4854
 * [struct io_cancel]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L362
 */
static inline void io_uring_prep_cancel(struct io_uring_sqe *sqe, void *user_data,
					int flags)
{
	io_uring_prep_rw(IORING_OP_ASYNC_CANCEL, sqe, -1, user_data, 0, 0);
	sqe->cancel_flags = flags;
}

/**
 * Prepare a link_timeout operation for a sq entry.
 *
 * See [IORING_OP_ASYNC_LINK_TIMEOUT], [io_timeout_prep], and [io_link_cancel_timeout] for processing this operation in the kernel.
 *
 * @param[in,out] sqe          a submission queue entry.
 * @param[in]     ts           a timespec.
 *                             will be set to @b ts in [struct io_timeout_data].
 * @param[in]     flags        flags for the timeout. if flags contain
 *                             IORING_TIMEOUT_ABS, @b ts will be treated as an
 *                             absolute timestamp.
 *                             will be used to set @b mode in [struct io_timeout_data].
 *
 * [IORING_OP_ASYNC_LINK_TIMEOUT]: https://elixir.bootlin.com/linux/v5.7-rc2/ident/IORING_OP_ASYNC_LINK_TIMEOUT
 * [io_timeout_prep]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L4672
 * [io_link_cancel_timeout]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L1433
 * [struct io_timeout_data]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L362
 */
static inline void io_uring_prep_link_timeout(struct io_uring_sqe *sqe,
					      struct __kernel_timespec *ts,
					      unsigned flags)
{
	io_uring_prep_rw(IORING_OP_LINK_TIMEOUT, sqe, -1, ts, 1, 0);
	sqe->timeout_flags = flags;
}

/**
 * Prepare a connect operation for a sq entry.
 *
 * See [IORING_OP_CONNECT], [io_connect_prep], and [io_connect] for processing this operation in the kernel.
 *
 * @param[in,out] sqe          a submission queue entry.
 * @param[in]     fd           a socket file descriptor. will be used to set @b file in [struct io_connect].
 * @param[in]     addr         an address for the socket. will be set to @b addr in [struct io_connect].
 * @param[in]     addrlen      the length of addr. will be set to @b addr_len in [struct io_connect].
 *
 * [IORING_OP_CONNECT]: https://elixir.bootlin.com/linux/v5.7-rc2/ident/IORING_OP_CONNECT
 * [io_connect_prep]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L3987
 * [io_connect]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L4007
 * [struct io_connect]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L397
 */
static inline void io_uring_prep_connect(struct io_uring_sqe *sqe, int fd,
					 struct sockaddr *addr,
					 socklen_t addrlen)
{
	io_uring_prep_rw(IORING_OP_CONNECT, sqe, fd, addr, 0, addrlen);
}

/**
 * Prepare a files_update operation for a sq entry.
 * This is used to update file descriptors which were registered with
 * io_uring_register_files.
 *
 * See [IORING_OP_FILES_UPDATE], [io_files_update_prep], and [io_files_update] for processing this operation in the kernel.
 *
 * @param[in,out] sqe          a submission queue entry.
 * @param[in]     fds          an array of file descriptors. will be set to @b args in [struct io_files_update].
 * @param[in]     nr_fds       the size of the @b fds. will be set to @b nr_args in [struct io_files_update].
 * @param[in]     offset       an offset in the registered file descriptors. will be set to @b offset in [struct io_files_update].
 *
 * [IORING_OP_FILES_UPDATE]: https://elixir.bootlin.com/linux/v5.7-rc2/ident/IORING_OP_FILES_UPDATE
 * [io_files_update_prep]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L4862
 * [io_files_update]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L4876
 * [struct io_files_update]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L427
 */
static inline void io_uring_prep_files_update(struct io_uring_sqe *sqe,
					      int *fds, unsigned nr_fds,
					      int offset)
{
	io_uring_prep_rw(IORING_OP_FILES_UPDATE, sqe, -1, fds, nr_fds, offset);
}

/**
 * Prepare a fallocate operation for a sq entry.
 *
 * See [IORING_OP_FALLOCATE], [io_fallocate_prep], and [io_fallocate] for processing this operation in the kernel.
 *
 * @param[in,out] sqe          a submission queue entry.
 * @param[in]     fd           a file descriptor. will be used to set @b file in [struct io_sync].
 * @param[in]     mode         a mode. will be set to @b mode in [struct io_sync].
 * @param[in]     offset       an offset. will be set to @b off in [struct io_sync].
 * @param[in]     len          a length. will be set to @b len in [struct io_sync].
 *
 * [IORING_OP_FALLOCATE]: https://elixir.bootlin.com/linux/v5.7-rc2/ident/IORING_OP_FALLOCATE
 * [io_fallocate_prep]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L2906
 * [io_fallocate]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L2919
 * [struct io_sync]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L370
 */
static inline void io_uring_prep_fallocate(struct io_uring_sqe *sqe, int fd,
					   int mode, off_t offset, off_t len)
{
	io_uring_prep_rw(IORING_OP_FALLOCATE, sqe, fd, (const void *) len, mode,
				offset);
}

/**
 * Prepare an openat operation for a sq entry.
 *
 * See [IORING_OP_OPENAT], [io_openat_prep], and [io_openat] for processing this operation in the kernel.
 *
 * @param[in,out] sqe          a submission queue entry.
 * @param[in]     dfd          a directory file descriptor for the openat. will be set to @b dfd in [struct io_open].
 * @param[in]     path         a path name. will be used to set @b filename in [struct io_open].
 * @param[in]     flags        flags. will be set to @b flags in [struct open_how].
 * @param[in]     mode         a mode. will be set to @b mode in [struct open_how].
 *
 * [IORING_OP_OPENAT]: https://elixir.bootlin.com/linux/v5.7-rc2/ident/IORING_OP_OPENAT
 * [io_openat_prep]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L2931
 * [io_openat]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L3039
 * [struct io_open]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L415
 * [struct open_how]: https://elixir.bootlin.com/linux/v5.7-rc2/source/include/uapi/linux/openat2.h#L19
 */
static inline void io_uring_prep_openat(struct io_uring_sqe *sqe, int dfd,
					const char *path, int flags, mode_t mode)
{
	io_uring_prep_rw(IORING_OP_OPENAT, sqe, dfd, path, mode, 0);
	sqe->open_flags = flags;
}

/**
 * Prepare an close operation for a sq entry.
 *
 * See [IORING_OP_CLOSE], [io_close_prep], and [io_close] for processing this operation in the kernel.
 *
 * @param[in,out] sqe          a submission queue entry.
 * @param[in]     fd           a file descriptor. will be set to @b fd in [struct io_close].
 *
 * [IORING_OP_CLOSE]: https://elixir.bootlin.com/linux/v5.7-rc2/ident/IORING_OP_CLOSE
 * [io_close_prep]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L3391
 * [io_close]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L3435
 * [struct io_close]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L349
 */
static inline void io_uring_prep_close(struct io_uring_sqe *sqe, int fd)
{
	io_uring_prep_rw(IORING_OP_CLOSE, sqe, fd, NULL, 0, 0);
}

/**
 * Prepare a read operation for a sq entry.
 *
 * See [IORING_OP_READ], [io_read_prep], [io_prep_rw], and [io_read] for processing this operation in the kernel.
 *
 * @param[in,out] sqe        a submission queue entry.
 * @param[in]     fd         a file descriptor. will be used to set @b file in [struct io_kiocb].
 * @param[in]     buf        a buffer which the read result will be written to. will be set to @b addr in [struct io_rw] @b rw in [struct io_kiocb].
 * @param[in]     nbytes     the maximum number of bytes to read. will be set to @b len in [struct io_rw] @b rw in [struct io_kiocb].
 * @param[in]     offset     an offset in the input. will be set to @b ki_pos in [struct kiocb] @b kiocb in [struct io_rw] @b rw in [struct io_kiocb].
 *
 * [IORING_OP_READ]: https://elixir.bootlin.com/linux/v5.7-rc2/ident/IORING_OP_READ
 * [io_read_prep]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L2519
 * [io_prep_rw]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L2049
 * [io_read]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L2549
 * [struct io_kiocb]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L593
 * [struct io_rw]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L390
 * [struct kiocb]: https://elixir.bootlin.com/linux/v5.7-rc2/source/include/linux/fs.h#L318
 */
static inline void io_uring_prep_read(struct io_uring_sqe *sqe, int fd,
				      void *buf, unsigned nbytes, off_t offset)
{
	io_uring_prep_rw(IORING_OP_READ, sqe, fd, buf, nbytes, offset);
}

/**
 * Prepare a write operation for a sq entry.
 *
 * See [IORING_OP_WRITE], [io_write_prep], [io_prep_rw], and [io_write] for processing this operation in the kernel.
 *
 * @param[in,out] sqe        a submission queue entry.
 * @param[in]     fd         a file descriptor. will be used to set @b file in [struct io_kiocb].
 * @param[in]     buf        a buffer which the read result will be written to. will be set to @b addr in [struct io_rw] @b rw in [struct io_kiocb].
 * @param[in]     nbytes     the maximum number of bytes to read. will be set to @b len in [struct io_rw] @b rw in [struct io_kiocb].
 * @param[in]     offset     an offset in the file. will be set to @b ki_pos in [struct kiocb] @b kiocb in [struct io_rw] @b rw in [struct io_kiocb].
 *
 * [IORING_OP_WRITE]: https://elixir.bootlin.com/linux/v5.7-rc2/ident/IORING_OP_WRITE
 * [io_write_prep]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L2608
 * [io_prep_rw]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L2049
 * [io_write]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L2640
 * [struct io_kiocb]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L593
 * [struct io_rw]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L390
 * [struct kiocb]: https://elixir.bootlin.com/linux/v5.7-rc2/source/include/linux/fs.h#L318
 */
static inline void io_uring_prep_write(struct io_uring_sqe *sqe, int fd,
				       const void *buf, unsigned nbytes, off_t offset)
{
	io_uring_prep_rw(IORING_OP_WRITE, sqe, fd, buf, nbytes, offset);
}

struct statx;
/**
 * Prepare a statx operation for a sq entry.
 *
 * See [IORING_OP_STATX], [io_statx_prep], and [io_statx] for processing this operation in the kernel.
 * @param[in,out] sqe        a submission queue entry.
 * @param[in]     dfd        a directory file descriptor. will be set to @b dfd in [struct io_open].
 * @param[in]     path       a path name. will be used to set @b filename in [struct io_open].
 * @param[in]     flags      flags. will be set to @b flags in [struct open_how].
 * @param[in]     mask       a mask. will be set to @b mask in [struct open_how].
 * @param[in]     statxbuf   a buffer. will be set to @b buffer in [struct io_open].
 *
 * [IORING_OP_STATX]: https://elixir.bootlin.com/linux/v5.7-rc2/ident/IORING_OP_STATX
 * [io_statx_prep]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L3317
 * [struct io_open]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L415
 * [struct open_how]: https://elixir.bootlin.com/linux/v5.7-rc2/source/include/uapi/linux/openat2.h#L19
 */
static inline void io_uring_prep_statx(struct io_uring_sqe *sqe, int dfd,
				const char *path, int flags, unsigned mask,
				struct statx *statxbuf)
{
	io_uring_prep_rw(IORING_OP_STATX, sqe, dfd, path, mask,
				(__u64) (unsigned long) statxbuf);
	sqe->statx_flags = flags;
}

/**
 * Prepare a fadvise operation for a sq entry.
 *
 * See [IORING_OP_FADVISE], [io_fadvise_prep], and [io_fadvise] for processing this operation in the kernel.
 *
 * @param[in,out] sqe        a submission queue entry.
 * @param[in]     fd         a file descriptor. will be used to set @b file in [struct io_fadvise].
 * @param[in]     offset     an offset. will be set to @b offset in [struct io_fadvise].
 * @param[in]     len        a length. will be set to @b len in [struct io_fadvise].
 * @param[in]     advice     an advise. will be set to @b advise in [struct io_fadvise].
 *
 * [IORING_OP_FADVISE]: https://elixir.bootlin.com/linux/v5.7-rc2/ident/IORING_OP_FADVISE
 * [io_fadvise_prep]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L3282
 * [io_fadvise]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L3293
 * [struct io_fadvise]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L434
 */
static inline void io_uring_prep_fadvise(struct io_uring_sqe *sqe, int fd,
					 off_t offset, off_t len, int advice)
{
	io_uring_prep_rw(IORING_OP_FADVISE, sqe, fd, NULL, len, offset);
	sqe->fadvise_advice = advice;
}

/**
 * Prepare a madvise operation for a sq entry.
 *
 * See [IORING_OP_MADVISE], [io_madvise_prep], and [io_madvise] for processing this operation in the kernel.
 *
 * @param[in,out] sqe        a submission queue entry.
 * @param[in]     addr       an address. will be set to @b addr in [struct io_madvise].
 * @param[in]     length     a length. will be set to @b len in [struct io_madvise].
 * @param[in]     advice     an advise. will be set to @b advice in [struct io_madvise].
 *
 * [IORING_OP_MADVISE]: https://elixir.bootlin.com/linux/v5.7-rc2/ident/IORING_OP_MADVISE
 * [io_madvise_prep]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L3247
 * [io_madvise]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L3262
 * [struct io_madvise]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L441
 */
static inline void io_uring_prep_madvise(struct io_uring_sqe *sqe, void *addr,
					 off_t length, int advice)
{
	io_uring_prep_rw(IORING_OP_MADVISE, sqe, -1, addr, length, 0);
	sqe->fadvise_advice = advice;
}

/**
 * Prepare a send operation for a sq entry.
 *
 * See [IORING_OP_SEND], [io_sendmsg_prep], and [io_send] for processing this operation in the kernel.
 *
 * @param[in,out] sqe        a submission queue entry.
 * @param[in]     sockfd     a socket file descriptor. will be used to set @b file in [struct io_sr_msg].
 * @param[in]     buf        a buffer. will be set to @b msg in [struct io_sr_msg].
 * @param[in]     len        a length. will be set to @b len in [struct io_sr_msg].
 * @param[in]     flags      flags. will be set to @b msg_flags in [struct io_sr_msg].
 *
 * [IORING_OP_SEND]: https://elixir.bootlin.com/linux/v5.7-rc2/ident/IORING_OP_SEND
 * [io_sendmsg_prep]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L3536
 * [io_send]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L3622
 * [struct io_sr_msg]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L403
 */
static inline void io_uring_prep_send(struct io_uring_sqe *sqe, int sockfd,
				      const void *buf, size_t len, int flags)
{
	io_uring_prep_rw(IORING_OP_SEND, sqe, sockfd, buf, len, 0);
	sqe->msg_flags = flags;
}

/**
 * Prepare a recv operation for a sq entry.
 *
 * See [IORING_OP_RECV], [io_recvmsg_prep], and [io_recv] for processing this operation in the kernel.
 *
 * @param[in,out] sqe        a submission queue entry.
 * @param[in]     sockfd     a socket file descriptor. will be used to set @b file in [struct io_sr_msg].
 * @param[in]     buf        a buffer. will be set to @b msg in [struct io_sr_msg].
 * @param[in]     len        a length. will be set to @b len in [struct io_sr_msg].
 * @param[in]     flags      flags. will be set to @b msg_flags in [struct io_sr_msg].
 *
 * [IORING_OP_RECV]: https://elixir.bootlin.com/linux/v5.7-rc2/ident/IORING_OP_RECV
 * [io_recvmsg_prep]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L3775
 * [io_recv]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L3868
 * [struct io_sr_msg]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L403
 */
static inline void io_uring_prep_recv(struct io_uring_sqe *sqe, int sockfd,
				      void *buf, size_t len, int flags)
{
	io_uring_prep_rw(IORING_OP_RECV, sqe, sockfd, buf, len, 0);
	sqe->msg_flags = flags;
}

/**
 * Prepare a openat2 operation for a sq entry.
 *
 * See [IORING_OP_OPENAT2], [io_openat2_prep], and [io_openat2] for processing this operation in the kernel.
 *
 * @param[in,out] sqe        a submission queue entry.
 * @param[in]     dfd        a directory file descriptor. will be set to @b dfd in [struct io_open].
 * @param[in]     path       a path name. will be used to set @b filename in [struct io_open]..
 * @param[in]     how        a pointer to a struct open_how.
 *                           will be copied to [struct open_how] @b how in [struct io_open].
 *
 * [IORING_OP_OPENAT2]: https://elixir.bootlin.com/linux/v5.7-rc2/ident/IORING_OP_OPENAT2
 * [io_openat2_prep]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L2962
 * [io_openat2]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L3004
 * [struct io_open]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L415
 * [struct open_how]: https://elixir.bootlin.com/linux/v5.7-rc2/source/include/uapi/linux/openat2.h#L19
 */
static inline void io_uring_prep_openat2(struct io_uring_sqe *sqe, int dfd,
					const char *path, struct open_how *how)
{
	io_uring_prep_rw(IORING_OP_OPENAT2, sqe, dfd, path, sizeof(*how),
				(uint64_t) (uintptr_t) how);
}

struct epoll_event;
/**
 * Prepare a epoll_ctl operation for a sq entry.
 *
 * See [IORING_OP_EPOLL_CTL], [io_epoll_ctl_prep], and [io_epoll_ctl] for processing this operation in the kernel.
 *
 * @param[in,out] sqe        a submission queue entry.
 * @param[in]     epfd       an epoll file descriptor. will be set to @b epfd in [struct io_epoll].
 * @param[in]     fd         a file descriptor. will be set to @b fd in [struct io_epoll].
 * @param[in]     op         an op code for the epoll. will be set to @b op in [struct io_epoll].
 * @param[in]     ev         an epoll event. will be copied to [struct epoll_event] @b event in [struct io_epoll].
 *
 * [IORING_OP_EPOLL_CTL]: https://elixir.bootlin.com/linux/v5.7-rc2/ident/IORING_OP_EPOLL_CTL
 * [io_epoll_ctl_prep]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L3202
 * [io_epoll_ctl]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L3227
 * [struct io_epoll]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L448
 * [struct epoll_event]: https://elixir.bootlin.com/linux/v5.7-rc2/source/include/uapi/linux/eventpoll.h#L77
 */
static inline void io_uring_prep_epoll_ctl(struct io_uring_sqe *sqe, int epfd,
					   int fd, int op,
					   struct epoll_event *ev)
{
	io_uring_prep_rw(IORING_OP_EPOLL_CTL, sqe, epfd, ev, op, fd);
}

/**
 * Prepare a provide_buffers operation for a sq entry.
 *
 * See [IORING_OP_PROVIDE_BUFFERS], [io_provide_buffers_prep], and [io_provide_buffers] for processing this operation in the kernel.
 *
 * @param[in,out] sqe        a submission queue entry.
 * @param[in]     addr       an address. will be set to @b addr in struct [io_provide_buf].
 * @param[in]     len        a length. will be set to @b len in struct [io_provide_buf].
 * @param[in]     nr         a number of buffers. will be set to @b nbufs in struct [io_provide_buf].
 * @param[in]     bgid       a buffer group id. will be set to @b bgid in struct [io_provide_buf].
 * @param[in]     bid        a buffer id. will be set to @b bid in struct [io_provide_buf].
 *
 * [IORING_OP_PROVIDE_BUFFERS]: https://elixir.bootlin.com/linux/v5.7-rc2/ident/IORING_OP_PROVIDE_BUFFERS
 * [io_provide_buffers_prep]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L3114
 * [io_provide_buffers]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L3168
 * [io_provide_buf]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L465
 */
static inline void io_uring_prep_provide_buffers(struct io_uring_sqe *sqe,
						 void *addr, int len, int nr,
						 int bgid, int bid)
{
	io_uring_prep_rw(IORING_OP_PROVIDE_BUFFERS, sqe, nr, addr, len, bid);
	sqe->buf_group = bgid;
}

/**
 * Prepare a remove_buffers operation for a sq entry.
 *
 * See [IORING_OP_REMOVE_BUFFERS], [io_remove_buffers_prep], and [io_remove_buffers] for processing this operation in the kernel.
 *
 * @param[in,out] sqe        a submission queue entry.
 * @param[in]     nr         a number of buffers. will be set to @b nbufs in struct [io_provide_buf].
 * @param[in]     bgid       a buffer group id. will be set to @b bgid in struct [io_provide_buf] at [io_remove_buffers_prep].
 *
 * [IORING_OP_REMOVE_BUFFERS]: https://elixir.bootlin.com/linux/v5.7-rc2/ident/IORING_OP_REMOVE_BUFFERS
 * [io_remove_buffers_prep]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L3045
 * [io_remove_buffers]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L3090
 * [io_provide_buf]: https://elixir.bootlin.com/linux/v5.7-rc2/source/fs/io_uring.c#L465
 */
static inline void io_uring_prep_remove_buffers(struct io_uring_sqe *sqe,
						int nr, int bgid)
{
	io_uring_prep_rw(IORING_OP_REMOVE_BUFFERS, sqe, nr, NULL, 0, 0);
	sqe->buf_group = bgid;
}

/**
 * Get the number of ready sq entries.
 * @param[in] ring      an io_uring queue.
 * @return the number of ready sq entries.
 */
static inline unsigned io_uring_sq_ready(struct io_uring *ring)
{
	/* always use real head, to avoid losing sync for short submit */
	return ring->sq.sqe_tail - *ring->sq.khead;
}

/**
 * Get the number of space left in sq entries.
 * @param[in] ring      an io_uring queue.
 * @return the number of space left in sq entries.
 */
static inline unsigned io_uring_sq_space_left(struct io_uring *ring)
{
	return *ring->sq.kring_entries - io_uring_sq_ready(ring);
}

/**
 * Get the number of ready cq entries.
 * @param[in] ring      an io_uring queue.
 * @return the number of ready cq entries.
 */
static inline unsigned io_uring_cq_ready(struct io_uring *ring)
{
	return io_uring_smp_load_acquire(ring->cq.ktail) - *ring->cq.khead;
}

/**
 * Helper for the peek single cqe functions. Exported because of that,
 * but probably shouldn't be used directly in an application.
 */
static int __io_uring_peek_cqe(struct io_uring *ring, struct io_uring_cqe **cqe_ptr)
{
	struct io_uring_cqe *cqe;
	unsigned head;
	int err = 0;

	do {
		io_uring_for_each_cqe(ring, head, cqe)
			break;
		if (cqe) {
			if (cqe->user_data == LIBURING_UDATA_TIMEOUT) {
				if (cqe->res < 0)
					err = cqe->res;
				io_uring_cq_advance(ring, 1);
				if (!err)
					continue;
				cqe = NULL;
			}
		}
		break;
	} while (1);

	*cqe_ptr = cqe;
	return err;
}

/**
 * Return an IO completion, waiting for 'wait_nr' completions if one isn't
 * readily available.
 * @param[in,out] ring    an io_uring queue.
 * @param[out]    cqe_ptr completion queue entry.
 * @param[in]     wait_nr number of completions to wait.
 * @return 0 with cqe_ptr filled in on success, -errno on failure.
 */
static inline int io_uring_wait_cqe_nr(struct io_uring *ring,
				      struct io_uring_cqe **cqe_ptr,
				      unsigned wait_nr)
{
	int err;

	err = __io_uring_peek_cqe(ring, cqe_ptr);
	if (err || *cqe_ptr)
		return err;

	return __io_uring_get_cqe(ring, cqe_ptr, 0, wait_nr, NULL);
}

/**
 * Return an IO completion, if one is readily available.
 * @param[in,out] ring    an io_uring queue.
 * @param[out]    cqe_ptr completion queue entry.
 * @return 0 with cqe_ptr filled in on success, -errno on failure.
 */
static inline int io_uring_peek_cqe(struct io_uring *ring,
				    struct io_uring_cqe **cqe_ptr)
{
	return io_uring_wait_cqe_nr(ring, cqe_ptr, 0);
}

/**
 * Return an IO completion, waiting for it if necessary.
 * @param[in,out] ring    an io_uring queue.
 * @param[out]    cqe_ptr completion queue entry.
 * @return 0 with cqe_ptr filled in on success, -errno on failure.
 */
static inline int io_uring_wait_cqe(struct io_uring *ring,
				    struct io_uring_cqe **cqe_ptr)
{
	return io_uring_wait_cqe_nr(ring, cqe_ptr, 1);
}

#ifdef __cplusplus
}
#endif

#endif
