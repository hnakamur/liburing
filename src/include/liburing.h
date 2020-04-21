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
 * @param[in,out] sqe        a submission queue entry.
 * @param[in]     data       a user data.
 */
static inline void io_uring_sqe_set_data(struct io_uring_sqe *sqe, void *data)
{
	sqe->user_data = (unsigned long) data;
}

/** 
 * Get user data from a cq entry which was set wth io_uring_sqe_set_data.
 * @param[in]     cqe        a completion queue entry.
 * @return the user data which was set wth io_uring_sqe_set_data.
 */
static inline void *io_uring_cqe_get_data(const struct io_uring_cqe *cqe)
{
	return (void *) (uintptr_t) cqe->user_data;
}

/**
 * Set flags to a sq entry.
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
 * @param[in]     op         an opcode.
 * @param[in,out] sqe        a submission queue entry.
 * @param[in]     fd         a file descriptor.
 * @param[in]     addr       an address.
 * @param[in]     len        a length.
 * @param[in]     offset     an offset.
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
 * @param[in,out] sqe           a submission queue entry.
 * @param[in]     fd_in         a file descriptor for an input.
 * @param[in]     off_in        an offset in the input.
 * @param[in]     fd_out        a file descriptor for an output.
 * @param[in]     off_out       an offset in the output.
 * @param[in]     nbytes        a number of bytes.
 * @param[in]     splice_flags  flags for splice.
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
 * @param[in,out] sqe        a submission queue entry.
 * @param[in]     fd         a file descriptor for an input.
 * @param[in]     iovecs     a fixed set of iovec buffers.
 * @param[in]     nr_iovecs  a number of iovec buffers.
 * @param[in]     offset     an offset in the input.
 */
static inline void io_uring_prep_readv(struct io_uring_sqe *sqe, int fd,
				       const struct iovec *iovecs,
				       unsigned nr_vecs, off_t offset)
{
	io_uring_prep_rw(IORING_OP_READV, sqe, fd, iovecs, nr_vecs, offset);
}

/**
 * Prepare a read operation using a fixed buffer for a sq entry.
 * @param[in,out] sqe        a submission queue entry.
 * @param[in]     fd         a file descriptor for an input.
 * @param[out]    buf        a buffer which the read result will be written to.
 * @param[in]     nbytes     the maximum number of bytes to read.
 * @param[in]     offset     an offset in the input.
 * @param[in]     buf_index  the index of the fixed buffer to use.
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
 * @param[in,out] sqe        a submission queue entry.
 * @param[in]     fd         a file descriptor for an output.
 * @param[in]     iovecs     a fixed set of iovec buffers.
 * @param[in]     nr_iovecs  a number of iovec buffers.
 * @param[in]     offset     an offset in the output.
 */
static inline void io_uring_prep_writev(struct io_uring_sqe *sqe, int fd,
					const struct iovec *iovecs,
					unsigned nr_vecs, off_t offset)
{
	io_uring_prep_rw(IORING_OP_WRITEV, sqe, fd, iovecs, nr_vecs, offset);
}

/**
 * Prepare a write operation using a fixed buffer for a sq entry.
 * @param[in,out] sqe        a submission queue entry.
 * @param[in]     fd         a file descriptor for an output.
 * @param[out]    buf        a source buffer.
 * @param[in]     nbytes     the number of bytes to write.
 * @param[in]     offset     an offset in the output.
 * @param[in]     buf_index  the index of the fixed buffer to use.
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
 * @param[in,out] sqe        a submission queue entry.
 * @param[in]     fd         a file descriptor for the socket to receive from.
 * @param[in,out] msg        a message header.
 * @param[in]     flags      the flags for the recvmsg syscall.
 */
static inline void io_uring_prep_recvmsg(struct io_uring_sqe *sqe, int fd,
					 struct msghdr *msg, unsigned flags)
{
	io_uring_prep_rw(IORING_OP_RECVMSG, sqe, fd, msg, 1, 0);
	sqe->msg_flags = flags;
}

/**
 * Prepare a sendmsg operation for a sq entry.
 * @param[in,out] sqe        a submission queue entry.
 * @param[in]     fd         a file descriptor for the socket to send to.
 * @param[in,out] msg        a message header.
 * @param[in]     flags      the flags for the sendmsg syscall.
 */
static inline void io_uring_prep_sendmsg(struct io_uring_sqe *sqe, int fd,
					 const struct msghdr *msg, unsigned flags)
{
	io_uring_prep_rw(IORING_OP_SENDMSG, sqe, fd, msg, 1, 0);
	sqe->msg_flags = flags;
}

/**
 * Prepare a poll_add operation for a sq entry.
 * @param[in,out] sqe        a submission queue entry.
 * @param[in]     fd         a file descriptor.
 * @param[in]     poll_mask  a mask for the poll.
 */
static inline void io_uring_prep_poll_add(struct io_uring_sqe *sqe, int fd,
					  short poll_mask)
{
	io_uring_prep_rw(IORING_OP_POLL_ADD, sqe, fd, NULL, 0, 0);
	sqe->poll_events = poll_mask;
}

/**
 * Prepare a poll_remove operation for a sq entry.
 * @param[in,out] sqe        a submission queue entry.
 * @param[in]     fd         a file descriptor.
 * @param[in]     poll_mask  a mask for the poll.
 */
static inline void io_uring_prep_poll_remove(struct io_uring_sqe *sqe,
					     void *user_data)
{
	io_uring_prep_rw(IORING_OP_POLL_REMOVE, sqe, -1, user_data, 0, 0);
}

/**
 * Prepare a fsync operation for a sq entry.
 * @param[in,out] sqe          a submission queue entry.
 * @param[in]     fd           a file descriptor.
 * @param[in]     fsync_flags  flags for the fsync.
 */
static inline void io_uring_prep_fsync(struct io_uring_sqe *sqe, int fd,
				       unsigned fsync_flags)
{
	io_uring_prep_rw(IORING_OP_FSYNC, sqe, fd, NULL, 0, 0);
	sqe->fsync_flags = fsync_flags;
}

/**
 * Prepare a nop operation for a sq entry.
 * @param[in,out] sqe          a submission queue entry.
 */
static inline void io_uring_prep_nop(struct io_uring_sqe *sqe)
{
	io_uring_prep_rw(IORING_OP_NOP, sqe, -1, NULL, 0, 0);
}

/**
 * Prepare a timeout operation for a sq entry.
 *
 * Note you need to set the user data with the sq entry if you would like to cancel
 * the timeout event later.
 *
 * @param[in,out] sqe          a submission queue entry.
 * @param[in]     ts           a timespec for the timeout.
 * @param[in]     count        a count. TODO: for what?
 * @param[in]     flags        flags for the timeout. if flags contain
 *                             IORING_TIMEOUT_ABS, ts will be treated as an
 *                             absolute timestamp.
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
 * @param[in,out] sqe          a submission queue entry.
 * @param[in]     user_data    the user data which was set to the sqe for the
 *                             timeout operation, in the other words, the
 *                             timeout event which has the same user data will
 *                             be cancelled.
 * @param[in]     flags        flags for the timeout.
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
 * @param[in,out] sqe          a submission queue entry.
 * @param[in]     fd           a socket file descriptor.
 * @param[in]     addr         an address for the socket.
 * @param[in]     addrlen      the length of addr.
 * @param[in]     flags        flags for the accept.
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
 * @param[in,out] sqe          a submission queue entry.
 * @param[in]     user_data    the same user data as set to the cancellation
 *                             target event entry.
 * @param[in]     flags        flags for the cancellation.
 */
static inline void io_uring_prep_cancel(struct io_uring_sqe *sqe, void *user_data,
					int flags)
{
	io_uring_prep_rw(IORING_OP_ASYNC_CANCEL, sqe, -1, user_data, 0, 0);
	sqe->cancel_flags = flags;
}

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
 * @param[in,out] sqe          a submission queue entry.
 * @param[in]     fd           a socket file descriptor.
 * @param[in]     addr         an address for the socket.
 * @param[in]     addrlen      the length of addr.
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
 * @param[in,out] sqe          a submission queue entry.
 * @param[in]     fds          an array of file descriptors.
 * @param[in]     nr_fds       the size of the fds.
 * @param[in]     offset       an offset in the registered file descriptors.
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
 * @param[in,out] sqe          a submission queue entry.
 * @param[in]     fd           a file descriptor.
 * @param[in]     mode         a mode for the fallocate.
 * @param[in]     offset       an offset for the fallocate.
 * @param[in]     len          a length for the fallocate.
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
 * @param[in,out] sqe          a submission queue entry.
 * @param[in]     dfd          a directory file descriptor for the openat.
 * @param[in]     path         a path name for the openat.
 * @param[in]     flags        flags for the openat.
 * @param[in]     mode         a mode for the openat.
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
 * @param[in,out] sqe          a submission queue entry.
 * @param[in]     fd           a file descriptor for the close operation.
 */
static inline void io_uring_prep_close(struct io_uring_sqe *sqe, int fd)
{
	io_uring_prep_rw(IORING_OP_CLOSE, sqe, fd, NULL, 0, 0);
}

/**
 * Prepare a read operation for a sq entry.
 * @param[in,out] sqe        a submission queue entry.
 * @param[in]     fd         a file descriptor to read from.
 * @param[out]    buf        a buffer which the read result will be written to.
 * @param[in]     nbytes     the maximum number of bytes to read.
 * @param[in]     offset     an offset in the input. TODO: really?
 */
static inline void io_uring_prep_read(struct io_uring_sqe *sqe, int fd,
				      void *buf, unsigned nbytes, off_t offset)
{
	io_uring_prep_rw(IORING_OP_READ, sqe, fd, buf, nbytes, offset);
}

/**
 * Prepare a write operation for a sq entry.
 * @param[in,out] sqe        a submission queue entry.
 * @param[in]     fd         a file descriptor to write to.
 * @param[out]    buf        a buffer for the content to write.
 * @param[in]     nbytes     the number of bytes to write.
 * @param[in]     offset     an offset in the input. TODO: really?
 */
static inline void io_uring_prep_write(struct io_uring_sqe *sqe, int fd,
				       const void *buf, unsigned nbytes, off_t offset)
{
	io_uring_prep_rw(IORING_OP_WRITE, sqe, fd, buf, nbytes, offset);
}

struct statx;
/**
 * Prepare a statx operation for a sq entry.
 * @param[in,out] sqe        a submission queue entry.
 * @param[in]     dfd        a directory file descriptor.
 * @param[in]     path       a path name.
 * @param[in]     flags      flags for the statx operation.
 * @param[in]     mask       a mask for the statx operation.
 * @param[out]    statxbuf   a buffer for the result of the statx operation.
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
 * @param[in,out] sqe        a submission queue entry.
 * @param[in]     fd         a file descriptor.
 * @param[in]     offset     an offset for the fadvise operation.
 * @param[in]     len        a length for the fadvise operation.
 * @param[in]     advice     an advise for the fadvise operation.
 */
static inline void io_uring_prep_fadvise(struct io_uring_sqe *sqe, int fd,
					 off_t offset, off_t len, int advice)
{
	io_uring_prep_rw(IORING_OP_FADVISE, sqe, fd, NULL, len, offset);
	sqe->fadvise_advice = advice;
}

/**
 * Prepare a madvise operation for a sq entry.
 * @param[in,out] sqe        a submission queue entry.
 * @param[in]     addr       an range beginning address for the madvise
 *                           operation.
 * @param[in]     length     the size of the range for the madvise operation.
 * @param[in]     advice     an advise for the madvise operation.
 */
static inline void io_uring_prep_madvise(struct io_uring_sqe *sqe, void *addr,
					 off_t length, int advice)
{
	io_uring_prep_rw(IORING_OP_MADVISE, sqe, -1, addr, length, 0);
	sqe->fadvise_advice = advice;
}

/**
 * Prepare a send operation for a sq entry.
 * @param[in,out] sqe        a submission queue entry.
 * @param[in]     sockfd     a socket file descriptor.
 * @param[in]     buf        a buffer containing a message to send.
 * @param[in]     len        the length of the message to send.
 * @param[in]     flags      flags for the send operation.
 */
static inline void io_uring_prep_send(struct io_uring_sqe *sqe, int sockfd,
				      const void *buf, size_t len, int flags)
{
	io_uring_prep_rw(IORING_OP_SEND, sqe, sockfd, buf, len, 0);
	sqe->msg_flags = flags;
}

/**
 * Prepare a recv operation for a sq entry.
 * @param[in,out] sqe        a submission queue entry.
 * @param[in]     sockfd     a socket file descriptor.
 * @param[in]     buf        a buffer to hold the received message.
 * @param[in]     len        the size of the buffer.
 * @param[in]     flags      flags for the recv operation.
 */
static inline void io_uring_prep_recv(struct io_uring_sqe *sqe, int sockfd,
				      void *buf, size_t len, int flags)
{
	io_uring_prep_rw(IORING_OP_RECV, sqe, sockfd, buf, len, 0);
	sqe->msg_flags = flags;
}

/**
 * Prepare a openat2 operation for a sq entry.
 * @param[in,out] sqe        a submission queue entry.
 * @param[in]     dfd        a directory file descriptor.
 * @param[in]     path       a path name.
 * @param[in]     how        a struct containing parameters for openat.
 *                           see open_how for fields in the struct.
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
 * @param[in,out] sqe        a submission queue entry.
 * @param[in]     epfd       an epoll file descriptor.
 * @param[in]     fd         a file descriptor.
 * @param[in]     op         an op code for the epoll.
 * @param[in]     ev         an epoll event.
 */
static inline void io_uring_prep_epoll_ctl(struct io_uring_sqe *sqe, int epfd,
					   int fd, int op,
					   struct epoll_event *ev)
{
	io_uring_prep_rw(IORING_OP_EPOLL_CTL, sqe, epfd, ev, op, fd);
}

/**
 * Prepare a provide_buffers operation for a sq entry.
 * @param[in,out] sqe        a submission queue entry.
 * @param[in]     addr       an address. TODO: for what?
 * @param[in]     len        a length. TODO: of what?
 * @param[in]     nr         a number. TODO: of what?
 * @param[in]     bgid       a group id for the buffer? TODO: what is this?
 * @param[in]     bid        a user id for the buffer? TODO: what is this?
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
 * @param[in,out] sqe        a submission queue entry.
 * @param[in]     nr         a number. TODO: of what?
 * @param[in]     bgid       a group id for the buffer? TODO: what is this?
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
