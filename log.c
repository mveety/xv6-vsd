#include "types.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "fs.h"
#include "buf.h"

// Simple logging that allows concurrent FS system calls.
//
// A log transaction contains the updates of multiple FS system
// calls. The logging system only commits when there are
// no FS system calls active. Thus there is never
// any reasoning required about whether a commit might
// write an uncommitted system call's updates to disk.
//
// A system call should call begin_op()/end_op() to mark
// its start and end. Usually begin_op() just increments
// the count of in-progress FS system calls and returns.
// But if it thinks the log is close to running out, it
// sleeps until the last outstanding end_op() commits.
//
// The log is a physical re-do log containing disk blocks.
// The on-disk log format:
//   header block, containing block #s for block A, B, C, ...
//   block A
//   block B
//   block C
//   ...
// Log appends are synchronous.

static void recover_from_log(uint dev);
static void commit(uint dev);

void
initlog(uint dev)
{
	struct superblock *sb;
	struct log *log;

	if (sizeof(struct logheader) >= BSIZE)
		panic("initlog: too big logheader");

	sb = getsuperblock(dev);
	log = getlog(dev);

	if(sb->nlog == 0)
		return;
	initlock(&log->lock, "log");
	log->start = sb->logstart;
	log->size = sb->nlog;
	log->dev = dev;
	recover_from_log(dev);
}

// Copy committed blocks from log to their home location
static void 
install_trans(uint dev)
{
	int tail;
	struct log *log;

	log = getlog(dev);
	for (tail = 0; tail < log->lh.n; tail++) {
		struct buf *lbuf = bread(log->dev, log->start+tail+1); // read log block
		struct buf *dbuf = bread(log->dev, log->lh.block[tail]); // read dst
		memmove(dbuf->data, lbuf->data, BSIZE);  // copy block to dst
		bwrite(dbuf);  // write dst to disk
		brelse(lbuf); 
		brelse(dbuf);
	}
}

// Read the log header from disk into the in-memory log header
static void
read_head(uint dev)
{
	struct log *log;
	struct buf *buf;
	struct logheader *lh;
	int i;

	log = getlog(dev);
	buf = bread(log->dev, log->start);
	lh = (struct logheader *) (buf->data);

	log->lh.n = lh->n;
	for (i = 0; i < log->lh.n; i++) {
		log->lh.block[i] = lh->block[i];
	}
	brelse(buf);
}

// Write in-memory log header to disk.
// This is the true point at which the
// current transaction commits.
static void
write_head(uint dev)
{
	struct log *log;
	struct buf *buf;
	struct logheader *hb;
	int i;

	log = getlog(dev);
	buf = bread(log->dev, log->start);
	hb = (struct logheader *) (buf->data);
	hb->n = log->lh.n;
	for (i = 0; i < log->lh.n; i++) {
		hb->block[i] = log->lh.block[i];
	}
	bwrite(buf);
	brelse(buf);
}

static void
recover_from_log(uint dev)
{
	struct log *log;

	log = getlog(dev);
	read_head(dev);      
	install_trans(dev); // if committed, copy from log to disk
	log->lh.n = 0;
	write_head(dev); // clear the log
}

// called at the start of each FS system call.
void
begin_op(uint dev)
{
	struct log *log;

	log = getlog(dev);
	if(log->size == 0)
		return;
	acquire(&log->lock);
	while(1){
		if(log->committing){
			sleep(log, &log->lock);
		} else if(log->lh.n + (log->outstanding+1)*MAXOPBLOCKS > LOGSIZE){
			// this op might exhaust log space; wait for commit.
			sleep(log, &log->lock);
		} else {
			log->outstanding += 1;
			release(&log->lock);
			break;
		}
	}
}

// called at the end of each FS system call.
// commits if this was the last outstanding operation.
void
end_op(uint dev)
{
	int do_commit = 0;
	struct log *log;

	log = getlog(dev);
	if(log->size == 0)
		return;
	acquire(&log->lock);
	log->outstanding -= 1;
	if(log->committing)
		panic("log->committing");
	if(log->outstanding == 0){
		do_commit = 1;
		log->committing = 1;
	} else {
		// begin_op() may be waiting for log space.
		wakeup(log);
	}
	release(&log->lock);

	if(do_commit){
		// call commit w/o holding locks, since not allowed
		// to sleep with locks.
		commit(dev);
		acquire(&log->lock);
		log->committing = 0;
		wakeup(log);
		release(&log->lock);
	}
}

// Copy modified blocks from cache to log.
static void 
write_log(uint dev)
{
	int tail;
	struct log *log;

	log = getlog(dev);
	for (tail = 0; tail < log->lh.n; tail++) {
		struct buf *to = bread(log->dev, log->start+tail+1); // log block
		struct buf *from = bread(log->dev, log->lh.block[tail]); // cache block
		memmove(to->data, from->data, BSIZE);
		bwrite(to);  // write the log
		brelse(from); 
		brelse(to);
	}
}

static void
commit(uint dev)
{
	struct log *log;

	log = getlog(dev);
	if (log->lh.n > 0) {
		write_log(dev);     // Write modified blocks from cache to log
		write_head(dev);    // Write header to disk -- the real commit
		install_trans(dev); // Now install writes to home locations
		log->lh.n = 0; 
		write_head(dev);    // Erase the transaction from the log
	}
}

// Caller has modified b->data and is done with the buffer.
// Record the block number and pin in the cache with B_DIRTY.
// commit()/write_log() will do the disk write.
//
// log_write() replaces bwrite(); a typical use is:
//   bp = bread(...)
//   modify bp->data[]
//   log_write(bp)
//   brelse(bp)
void
log_write(struct buf *b)
{
	int i;
	struct log *log;

	log = getlog(b->dev);
	if(log->size == 0){
		bwrite(b);
		return;
	}
	if (log->lh.n >= LOGSIZE || log->lh.n >= log->size - 1)
		panic("too big a transaction");
	if (log->outstanding < 1)
		panic("log_write outside of trans");

	acquire(&log->lock);
	for (i = 0; i < log->lh.n; i++) {
		if (log->lh.block[i] == b->blockno)   // log absorbtion
			break;
	}
	log->lh.block[i] = b->blockno;
	if (i == log->lh.n)
		log->lh.n++;
	b->flags |= B_DIRTY; // prevent eviction
	release(&log->lock);
}
