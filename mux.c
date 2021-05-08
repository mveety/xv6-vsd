// COM2 driver and mux
// basically 

#include "types.h"
#include "defs.h"
#include "param.h"
#include "errstr.h"
#include "traps.h"
#include "kbd.h"
#include "spinlock.h"
#include "fs.h"
#include "file.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"

#define COM2 0x2F8
#define MUXBUFMAX 64
#define BACKSPACE 0x100

typedef struct Muxbuf {
	u8int partial;
	u8int header;
	u8int data;
} Muxbuf;

typedef struct Line {
	u8int st;
	struct spinlock lock;
	struct inputbuf input;
} Line;

static int mux;
static int uart1cons;
struct spinlock muxlock;
struct spinlock mblock;
uint muxbufi = 0;
Muxbuf muxbuf[MUXBUFMAX];
Muxbuf cur;
Line lines[16];

void muxputc(int);
int muxgetc(void);
void lputc(Line*, int);
void writeline(Line*, int);
void mbprocess(void);
int muxread(struct inode*, char*, int, int);
int muxwrite(struct inode*, char*, int, int);
int nomuxrw(struct inode*, char*, int, int);

void
muxinit(void)
{
	uchar i;
	struct spinlock *l;

	outb(COM2+2, 0);
	outb(COM2+3, 0x80);
	// 9600 baud, 8 data, 1 stop, no parity
	outb(COM2+0, 12);
	outb(COM2+1, 0);
	outb(COM2+3, 0x03);
	outb(COM2+4, 0);
	outb(COM2+1, 0x01);
	cprintf("cpu%d: driver: starting mux\n", cpu->id);
	if(inb(COM2+5) == 0xff){
		devsw[8].read = &nomuxrw;
		devsw[8].write = &nomuxrw;
	};
	mux = 1;
	initlock(&muxlock, "com2 mux lock");
	initlock(&mblock, "mux input buffer lock");
	for(i = 0; i < 16; i++){
		lines[i].st = i;
		initlock(&lines[i].lock, "mux line lock");
	}
	devsw[8].read = &muxread;
	devsw[8].write = &muxwrite;
	inb(COM2+2);
	inb(COM2+0);
	picenable(IRQ_COM2);
	ioapicenable(IRQ_COM2, 0);
}

void
muxputc(int c)
{
	int i;

	if(!mux)
		return;
	for(i = 0; i < 128 && !(inb(COM2+5) & 0x20); i++)
		microdelay(10);
	if(uart1cons && c == 0x100)
		c = C('H');
	outb(COM2+0, c);
}

int
muxgetc(void)
{
	if(!mux)
		return -1;
	if(!(inb(COM2+5) & 0x01))
		return -1;
	return inb(COM2+0);
}

void
lputc(Line *l, int c)
{
	uchar linen;
	uchar header;

	header = (l->st << 4) | 0x0d;
	muxputc(header);
	muxputc(c);
}

void
writeline(Line *l, int c)
{
	struct inputbuf *buf;

	Lock(l->lock);
	buf = &l->input;
	switch(c){
	case C('U'):
		while(buf->e != buf->w && buf->buf[(buf->e-1) % INPUT_BUF] != '\n'){
			buf->e--;
			lputc(l, BACKSPACE);
		}
		break;
	case C('H'):
	case '\x7f':
		if(buf->e != buf->w){
			buf->e--;
			lputc(l, BACKSPACE);
		}
		break;
	default:
		if(c != 0 && buf->e-buf->r < INPUT_BUF){
			c = (c == '\r') ? '\n' : c;
			buf->buf[buf->e++ % INPUT_BUF] = c;
			lputc(l, c);
			if(c == '\n' || c == C('D') || buf->e == buf->r+INPUT_BUF){
				buf->w = buf->e;
				wakeup(&buf->r);
			}
		}
		break;
	}
	Unlock(l->lock);
}

void
mbprocess(void)
{
	int i;
	uchar cmd;
	uchar line;
	uchar dat;
	Muxbuf *bf;

	Lock(mblock);
	for(i = 0; i < muxbufi; i++){
		bf = &muxbuf[i];
		line = (bf->header & 0xf0) >> 4;
		cmd = (bf->header & 0x0f);
		dat = bf->data;
		switch(cmd){
		case 0x0:
			continue;
		case 0x1:
		case 0x2:
		case 0x3:
		case 0x4:
		case 0x5:
		case 0x6:
		case 0x7:
		case 0x8:
			cprintf("cpu%d: mux: unknown command %x for line %d with data %x\n",
 				   cpu->id, cmd, line, dat);
			break;
		case 0xd:
			writeline(&lines[line], dat);
			break;
		}
	}
	muxbufi = 0;
	Unlock(mblock);
	return;
}

void
muxintr(void)
{
	uchar c;

	if(!mux)
		return;
	Lock(muxlock);
	while((c = muxgetc()) != -1){
		switch(cur.partial){
		case 0:
			cur.header = c;
			cur.partial++;
			break;
		case 1:
			cur.data = c;
			cur.partial++;
			Lock(mblock);
			cur.partial++;
			muxbuf[muxbufi] = cur;
			muxbufi++;
			Unlock(mblock);
			cur.partial = cur.header = cur.data = 0;
			if(muxbufi >= MUXBUFMAX)
				mbprocess();
			break;
		}
	}
	if(muxbufi > 0)
		mbprocess();
	Unlock(muxlock);
}

int
muxread(struct inode *ip, char *buf, int nbytes, int off)
{
	int c;
	int tot;
	Line *l;
	struct inputbuf *lb;

	if(ip->minor >= 16){
		seterr(EDNOTEXIST);
		return -1;
	}
	tot = nbytes;
	iunlock(ip);
	l = &lines[ip->minor];
	Lock(l->lock);
	lb = &l->input;
	while(nbytes > 0){
		while(lb->r == lb->w){
			if(proc->killed){
				Unlock(l->lock);
				ilock(ip);
				return -1;
			}
			sleep(&lb->r, &l->lock);
		}
		c = lb->buf[lb->r++ % INPUT_BUF];
		if(c == C('C')){
		}
		if(c == C('D')){
			if(nbytes < tot)
				lb->r--;
			break;
		}
		*buf++ = c;
		--nbytes;
		if(c == '\n')
			break;
	}
	Unlock(l->lock);
	ilock(ip);
	return tot - nbytes;
}

int
muxwrite(struct inode *ip, char *buf, int nbytes, int off)
{
	Line *line;
	int i;

	if(ip->minor >= 16){
		seterr(EDNOTEXIST);
		return -1;
	}
	iunlock(ip);
	Lock(muxlock);
	line = &lines[ip->minor];
	for(i = 0; i < nbytes; i++)
		lputc(line, buf[i] & 0xff);
	Unlock(muxlock);
	ilock(ip);
	return nbytes;
}

int
nomuxrw(struct inode *ip, char *buf, int nbytes, int off)
{
	return -1;
}

