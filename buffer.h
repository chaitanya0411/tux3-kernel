#ifndef TUX3_BUFFER_H
#define TUX3_BUFFER_H

#ifdef __KERNEL__
#include "link.h"

/*
 * Choose carefully:
 * loff_t can be "long" or "long long" in userland. (not printf friendly)
 * sector_t can be "unsigned long" or "u64". (not printf friendly, and
 * would be hard to control on 32bits arch)
 *
 * we want 48bits for tux3, and error friendly. (FIXME: what is best?)
 */
typedef signed long long	block_t;

/* Maximum delta number (must be power of 2) */
#define TUX3_MAX_DELTA		2	/* 1 frontend + 1 backend */
#define TUX3_INIT_DELTA		0	/* initial delta number */

enum {
	BUFFER_FREED, BUFFER_EMPTY, BUFFER_CLEAN, BUFFER_DIRTY,
	BUFFER_STATES = BUFFER_DIRTY + TUX3_MAX_DELTA
};

struct sb;
struct tux3_iattr_data;
static inline block_t bufindex(struct buffer_head *buffer);

static inline void *bufdata(struct buffer_head *buffer)
{
	return buffer->b_data;
}

static inline size_t bufsize(struct buffer_head *buffer)
{
	return buffer->b_size;
}

static inline int bufcount(struct buffer_head *buffer)
{
	return atomic_read(&buffer->b_count);
}

static inline int buffer_clean(struct buffer_head *buffer)
{
	return !buffer_dirty(buffer) || buffer_uptodate(buffer);
}

static inline void blockput(struct buffer_head *buffer)
{
	put_bh(buffer);
}

static inline int buffer_empty(struct buffer_head *buffer)
{
	return 1;
}

static inline struct buffer_head *set_buffer_empty(struct buffer_head *buffer)
{
	return buffer;
}

int buffer_already_dirty(struct buffer_head *buffer, unsigned delta);
int buffer_can_modify(struct buffer_head *buffer, unsigned delta);
void tux3_set_buffer_dirty_list(struct address_space *mapping,
				struct buffer_head *buffer, int delta,
				struct list_head *head);
void tux3_set_buffer_dirty(struct address_space *mapping,
			   struct buffer_head *buffer, int delta);
void tux3_clear_buffer_dirty(struct buffer_head *buffer, unsigned delta);
void blockput_free(struct sb *sb, struct buffer_head *buffer);
void blockput_free_unify(struct sb *sb, struct buffer_head *buffer);
void tux3_invalidate_buffer(struct buffer_head *buffer);

/* buffer_writeback.c */
/* Helper for waiting I/O */
struct iowait {
	atomic_t inflight;		/* In-flight I/O count */
	struct completion done;		/* completion for in-flight I/O */
};

/* Helper for compressed I/O */
struct compressed_bio {
	/* number of bios pending for this compressed extent */
	atomic_t pending_bios;

	/* the pages with the compressed data on them */
	struct page **compressed_pages;

	/* inode that owns this data */
	struct inode *inode;

	/* FIFO structure for end_io */
	struct buffer_head *buffer;
	
	/* starting offset in the inode for our pages */
	block_t start;

	/* number of bytes in the inode we're working on */
	unsigned len;

	/* number of bytes on disk */
	unsigned compressed_len;

	/* the compression algorithm for this bio */
	int compress_type;

	/* number of compressed pages in the array */
	unsigned nr_pages;

	/* IO errors */
	int errors;

	/* for reads, this is the bio we are copying the data into */
	//struct bio *orig_bio;
};

/* Helper for buffer vector I/O */
#define BUFS_PER_PAGE_CACHE	(PAGE_CACHE_SIZE / 512)
struct bufvec {
	struct list_head *buffers;	/* The dirty buffers for this delta */
	struct list_head contig;	/* One logical contiguous range */
	unsigned contig_count;		/* Count of contiguous buffers */
	struct tux3_iattr_data *idata;	/* inode attrs for write */
	struct address_space *mapping;	/* address_space for dirty buffers */

	struct {
		struct buffer_head *buffer;
		block_t block;
	} on_page[BUFS_PER_PAGE_CACHE];
	unsigned on_page_idx;

	struct compressed_bio *cb;
	struct bio *bio;
	struct buffer_head *bio_lastbuf;
};

static inline struct inode *bufvec_inode(struct bufvec *bufvec)
{
	return bufvec->mapping->host;
}

static inline unsigned bufvec_contig_count(struct bufvec *bufvec)
{
	return bufvec->contig_count;
}

static inline struct buffer_head *bufvec_contig_buf(struct bufvec *bufvec)
{
	struct list_head *first = bufvec->contig.next;
	assert(!list_empty(&bufvec->contig));
	return list_entry(first, struct buffer_head, b_assoc_buffers);
}

/* buffer for each contiguous buffers */
#define bufvec_buffer_for_each_contig(b, v)	\
	list_for_each_entry(b, &(v)->contig, b_assoc_buffers)

static inline block_t bufvec_contig_index(struct bufvec *bufvec)
{
	return bufindex(bufvec_contig_buf(bufvec));
}

static inline block_t bufvec_contig_last_index(struct bufvec *bufvec)
{
	return bufvec_contig_index(bufvec) + bufvec_contig_count(bufvec) - 1;
}

void tux3_iowait_init(struct iowait *iowait);
void tux3_iowait_wait(struct iowait *iowait);
int bufvec_compressed_io(int rw, struct bufvec *bufvec, block_t physical, unsigned count);
int bufvec_io(int rw, struct bufvec *bufvec, block_t physical, unsigned count);
int bufvec_contig_add(struct bufvec *bufvec, struct buffer_head *buffer);
int flush_list(struct address_space *mapping, struct tux3_iattr_data *idata,
	       struct list_head *head);
int __tux3_volmap_io(int rw, struct bufvec *bufvec, block_t physical,
		     unsigned count);
int tux3_volmap_io(int rw, struct bufvec *bufvec);

/* block_fork.c */
#define PageForked(x)		PageChecked(x)
#define SetPageForked(x)	SetPageChecked(x)

/* For reliable check, lock_page() is needed */
static inline int buffer_forked(struct buffer_head *buffer)
{
	return PageForked(buffer->b_page);
}

void free_forked_buffers(struct sb *sb, struct inode *inode, int force);
struct buffer_head *blockdirty(struct buffer_head *buffer, unsigned newdelta);
struct page *pagefork_for_blockdirty(struct page *oldpage, unsigned newdelta);
int bufferfork_to_invalidate(struct address_space *mapping, struct page *page);
#endif /* !__KERNEL__ */
#endif /* !TUX3_BUFFER_H */
