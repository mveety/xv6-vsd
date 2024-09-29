#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <limits.h>
#include <ctype.h>

#define stat xv6_stat
#include "../types.h"
#include "../fs.h"
#include "../stat.h"
#include "../param.h"

#ifndef static_assert
#define static_assert(a, b) do { switch (0) case 0: case (a): ; } while (0)
#endif

#define min(a,b) ((a) < (b) ? (a) : (b))

#define LIBLOCK(i, sb)     ((i) / IPB + sb.inodestart)

#define sysperms U_READ|W_READ
#define binperms U_READ|U_EXEC|W_READ|W_EXEC
#define etcperms U_READ|U_WRITE|W_READ
#define dirperms U_READ|U_EXEC|W_READ|W_EXEC

typedef enum EntryType EntryType;
typedef struct Dir Dir;
typedef struct File File;
typedef struct Fs Fs;
typedef struct IndirPtr IndirPtr;
typedef struct Entry Entry;

enum {
	FSYS = 1,
	DIRECT = 2,
	EFILE = 3,
	KERNEL = 4,
	BOOTLOADER = 5,
	SYNC = 6,
	U_READ  =  (1<<0),
	U_WRITE  =  (1<<1),
	U_EXEC  =  (1<<2),
	U_HIDDEN  =  (1<<3),
	W_READ  =  (1<<4),
	W_WRITE  =  (1<<5),
	W_EXEC  =  (1<<6),
	W_HIDDEN  =  (1<<7),
};

struct Fs {
	char *label;
	char *fname;
	int fd;
	uint size;
	uint freeinode;
	uint ninodes;
	char *bmap;
	struct superblock sb;
	Dir *dirs;
	File *files;
	File *bootloader;
	File *kernel;
};

struct Dir {
	char *name;
	Fs *fs;
	Dir *parent;
	uint ino;
	int owner;
	short perms;
	Dir *next;
};

struct File {
	char *name;
	char *srcfile;
	Dir *home;
	int owner;
	short perms;
	uint ino;
	File *next;
};

struct IndirPtr {
	uint block; // block address
	uint ptr; // inode pointer
};

struct Entry {
	int type;
	int active;
	Fs *fs;
	char *name;
	char *home;
	char *source;
	int owner;
	uint size;
	short perms;
	Entry *next;
};

int verbose;
int debug;
int neuter;
int use_altfsfile;
int use_altsize;

char zeroes[BSIZE];
Fs *active;
Entry *entries;
int fsbootable;
int bootino;
int kernbrand;
int kernino;
uint blocks_used;
char *altfsfile;
char *altsize;

void*
emallocz(size_t size)
{
	void *p;

	p = malloc(size);
	if(!p){
		dprintf(2, "mkproto: unable to allocate memory\n");
		exit(1);
	}
	memset(p, 0, size);
	return p;
}

// we're going to try to wrap things in helpers to make this
// more portable to xv6 in the future. I can see something like
// this being used to install the system
char*
readline(FILE *stdfd)
{
	char buf[256];
	char *line;

	memset(buf, 0, 256);
	if((line = fgets(buf, 256, stdfd)) != nil)
		return strdup(line);
	return nil;
}

int
parseperms2(char *strperms, int world)
{
	int perms = 0;
	int strpermslen;
	int i = 0;

	strpermslen = strlen(strperms);

	if(strperms[i] == 'r' && i < strpermslen){
		perms |= world ? W_READ : U_READ;
		i++;
	} else if(strperms[i] == '-' && i < strpermslen) {
		i++;
	}
	if(strperms[i] == 'w' && i < strpermslen){
		perms |= world ? W_WRITE : U_WRITE;
		i++;
	} else if(strperms[i] == '-' && i < strpermslen) {
		i++;
	}

	if(strperms[i] == 'x' && i < strpermslen){
		perms |= world ? W_EXEC : U_EXEC;
		i++;
	} else if(strperms[i] == '-' && i < strpermslen) {
		i++;
	}

	if(strperms[i] == 'h' && i < strpermslen){
		perms |= world ? W_HIDDEN : U_HIDDEN;
		i++;
	}
	return perms;
}

int
parseperms1(char *strperms)
{
	if(strperms[1] != ':')
		return -1;
	if(strperms[0] == 'u' || strperms[0] == 'U')
		return parseperms2(strperms+2, 0);
	if(strperms[0] == 'w' || strperms[0] == 'W')
		return parseperms2(strperms+2, 1);
	return -1;
}

int
parseperms(char *field1, char *field2)
{
	int perms1 = 0;
	int perms2 = 0;

	perms1 = parseperms1(field1);
	perms2 = parseperms1(field2);
	return perms1 | perms2;
}

char* // the buffer needs to be longer than 16 bytes
fmtpermsn(int perms, char *buf, int buflen)
{
//	"u:rwxh w:rwxh"

	int i = 0;

	if(buflen < 16)
		return nil;
	memset(buf, 0, buflen);

	buf[i++] = 'u';
	buf[i++] = ':';

	if((perms&U_READ) == U_READ)
		buf[i++] = 'r';
	else
		buf[i++] = '-';
	if((perms&U_WRITE) == U_WRITE)
		buf[i++] = 'w';
	else
		buf[i++] = '-';
	if((perms&U_EXEC) == U_EXEC)
		buf[i++] = 'x';
	else
		buf[i++] = '-';
	if((perms&U_HIDDEN) == U_HIDDEN)
		buf[i++] = 'h';
	else
		buf[i++] = '-';

	buf[i++] = ' ';
	buf[i++] = 'w';
	buf[i++] = ':';

	if((perms&W_READ) == W_READ)
		buf[i++] = 'r';
	else
		buf[i++] = '-';
	if((perms&W_WRITE) == W_WRITE)
		buf[i++] = 'w';
	else
		buf[i++] = '-';
	if((perms&W_EXEC) == W_EXEC)
		buf[i++] = 'x';
	else
		buf[i++] = '-';
	if((perms&W_HIDDEN) == W_HIDDEN)
		buf[i++] = 'h';
	else
		buf[i++] = '-';

	return buf;
}


IndirPtr
offset2real(uint off)
{
	IndirPtr buf;

	buf.block = off / NINDIRECT;
	buf.ptr = off % NINDIRECT;
	return buf;
}

// convert to intel byte order
ushort
xshort(ushort x)
{
	ushort y;
	uchar *a = (uchar*)&y;
	a[0] = x;
	a[1] = x >> 8;
	return y;
}

uint
xint(uint x)
{
	uint y;
	uchar *a = (uchar*)&y;
	a[0] = x;
	a[1] = x >> 8;
	a[2] = x >> 16;
	a[3] = x >> 24;
	return y;
}

char*
strtrim(char *str)
{
	char *p, *s, *rval;
	int len;

	s = strdup(str);
	p = s;
	len = strlen(s);
	while(isspace(p[len - 1]))
		p[--len] = 0;
	while(*p && isspace(*p)){
		p++;
		len--;
	}
	memmove(s, p, len+1);
	rval = strdup(s);
	free(s);
	return rval;
}

void
wsect(Fs *fs, void *buf, uint blk)
{
	if(lseek(fs->fd, blk * BSIZE, 0) != blk * BSIZE){
		perror("lseek");
		exit(1);
	}
	if(write(fs->fd, buf, BSIZE) != BSIZE){
		perror("write");
		exit(1);
	}
}

void
rsect(Fs *fs, void *buf, uint blk)
{
	if(lseek(fs->fd, blk * BSIZE, 0) != blk * BSIZE){
		perror("lseek");
		exit(1);
	}
	if(read(fs->fd, buf, BSIZE) != BSIZE){
		perror("read");
		exit(1);
	}
}

void
rinode(Fs *fs, uint inum, struct dinode *ip)
{
	char buf[BSIZE];
	uint bn;
	struct dinode *dip;

	bn = LIBLOCK(inum, fs->sb);
	rsect(fs, buf, bn);
	dip = ((struct dinode*)buf) + (inum % IPB);
	*ip = *dip;
}

void
winode(Fs *fs, uint inum, struct dinode *ip)
{
	char buf[BSIZE];
	uint bn;
	struct dinode *dip;

	bn = LIBLOCK(inum, fs->sb);
	rsect(fs, buf, bn);
	dip = ((struct dinode*)buf) + (inum % IPB);
	*dip = *ip;
	wsect(fs, buf, bn);
}

uint
ialloc(Fs *fs, ushort type, int owner, ushort perms)
{
	uint inum = fs->freeinode++;
	struct dinode din;

	bzero(&din, sizeof(din));
	din.type = xshort(type);
	din.nlink = xshort(1);
	din.size = xint(0);
	din.owner = xint(owner);
	din.perms = perms;
	winode(fs, inum, &din);
	return inum;
}

uint
balloc(Fs *fs)
{
	char *bmap = fs->bmap;
	uint i;

	for(i = 1; i < fs->size; i++)
		if(!(bmap[i/8] & (1<<(i%8)))){
			bmap[i/8] |= (1<<(i%8));
			return i;
		}
	dprintf(2, "mkproto: unable to allocate block. filesystem full\n");
	exit(1);
}

void
iappend(Fs *fs, uint ino, void *data, uint sz)
{
	char *p = (char*)data;
	struct dinode din;
	char buf[BSIZE];
	IndirPtr bptr;
	uint indirect[NINDIRECT];
	uint fbn, raddr, off, sboff, wrlen;

	rinode(fs, ino, &din);
	off = xint(din.size);

	while(sz > 0){
		sboff = fbn = 0;

		// work out which block pointer and the offset within it
		if(off%BSIZE){
			fbn = off/BSIZE;
			sboff = (off%BSIZE);
			wrlen = (sz < (BSIZE-sboff) ? sz : BSIZE - sboff);
		}else if(off == 0){
			fbn = 0;
			sboff = 0;
			wrlen = ((sz < BSIZE-sboff) ? sz : BSIZE-sboff);
		} else {
			fbn = off/BSIZE;
			sboff = 0;
			wrlen = ((sz < BSIZE) ? sz : BSIZE);
		}

		// get the offset's address within the inode and fetch/allocate and update
		// the block pointers and get the block to write
		bptr = offset2real(fbn);
		if(xint(din.addrs[bptr.block]) == 0)
			din.addrs[bptr.block] = xint(balloc(fs));
		rsect(fs, (char*)indirect, xint(din.addrs[bptr.block]));
		if(indirect[bptr.ptr] == 0){
			indirect[bptr.ptr] = xint(balloc(fs));
			wsect(fs, (char*)indirect, xint(din.addrs[bptr.block]));
		}
		raddr = xint(indirect[bptr.ptr]);

		if(debug)
			dprintf(2, "mkproto: writing at block %u+%u (real %u) %u bytes\n",
					fbn, sboff, raddr, wrlen);

		// read and update the block
		rsect(fs, buf, raddr);
		memcpy(buf+sboff, p, wrlen);
		wsect(fs, buf, raddr);

		// update the counters
		p += wrlen;
		sz -= wrlen;
		off += wrlen;
	}

	// update the inode
	din.size = xint(off);
	winode(fs, ino, &din);
}

void
syncfs(Fs *fs)
{
	char buf[BSIZE];

	char *bmap = fs->bmap;
	uint bmapsz = fs->size/(BSIZE*8) + 1;
	int i;

	for(i = 0; i*BSIZE < bmapsz; i++)
		wsect(fs, bmap+(i*BSIZE), fs->sb.bmapstart+i);
	memset(buf, 0, sizeof(buf));
	memmove(buf, &(fs->sb), sizeof(struct superblock));
	wsect(fs, buf, 1);
}

void
closefs(Fs *fs)
{
	uint i;
	char *bmap = fs->bmap;

	blocks_used = 1;
	for(i = 1; i < fs->size; i++){
		if((bmap[i/8] & (1<<(i%8))))
			blocks_used++;
	}
	close(fs->fd);
	fs->fd = -1;
}

Fs*
neuter_makefs(char *file, char *label, uint size, uint logsize, uint inodes)
{
	Fs *newfs;

	newfs = emallocz(sizeof(Fs));
	newfs->label = strdup(label);
	
	return newfs;
}

Fs*
makefs(char *file, char *label, uint size, uint logsize, uint ninodes)
{
	Fs *newfs;
	int nbitmap;
	int nlog;
	int nmeta;
	int nblocks;
	int ninodeblocks;
	int i;
	char buf[BSIZE];
	Dir *root;
	struct dirent de;

	newfs = emallocz(sizeof(Fs));
	newfs->fd = open(file, O_RDWR|O_CREAT|O_TRUNC, 0666);
	if(newfs->fd < 0){
		perror(file);
		exit(1);
	}
	if(strlen(label) > 10){
		dprintf(2, "mkproto: label %s too long (must be <=10)\n", label);
		exit(1);
	}

	nbitmap = size/(BSIZE*8) + 1;
	ninodeblocks = ninodes / IPB + 1;
	nlog = logsize;
	nmeta = 2 + nlog + ninodeblocks + nbitmap;
	nblocks = size - nmeta;

	memset(newfs->sb.label, 0, 11);
	strcpy(newfs->sb.label, label);	
	newfs->sb.version = 0;
	newfs->sb.nblocks = xint(nblocks);
	newfs->sb.ninodes = xint(ninodes);
	newfs->sb.nlog = xint(nlog);
	newfs->sb.logstart = xint(2);
	newfs->sb.inodestart = xint(2+nlog);
	newfs->sb.bmapstart = xint(2+nlog+ninodeblocks);
	newfs->sb.bootable = 0;
	newfs->sb.system = 1;
	newfs->sb.bootinode = 0;
	newfs->sb.size = size;
	newfs->bmap = emallocz(nbitmap*BSIZE);
	newfs->freeinode = 1;
	newfs->size = size;
	newfs->label = newfs->sb.label;

	// reserve used blocks
	for(i = 0; i < nmeta; i++)
		newfs->bmap[i/8] |= 1 << (i %8);

	dprintf(2, "mkproto: %s: vsd new filesystem\n", file);
	dprintf(2, "mkproto: %s: label %s, version %d\n", file, newfs->sb.label, newfs->sb.version);
	dprintf(2, "mkproto: %s: meta %d (boot, super, log %u, inode %u, bitmap %u) blocks %d total %d\n",
				 file, nmeta, nlog, ninodeblocks, nbitmap, nblocks, newfs->size);

	// ream the filesystem
	dprintf(2, "mkproto: reaming %s", label);
	for(i = 0; i < size; i++){
		if(!(i%100))
			dprintf(2, ".");
		wsect(newfs, zeroes, i);
	}
	memset(buf, 0, sizeof(buf));
	memmove(buf, &(newfs->sb), sizeof(struct superblock));
	wsect(newfs, buf, 1);

	// make the root directory
	root = emallocz(sizeof(Dir));
	root->name = strdup("/");
	root->parent = root;
	root->ino = ialloc(newfs, T_DIR, -1, dirperms);
	root->perms = dirperms;
	root->fs = newfs;
	root->next = nil;
	bzero(&de, sizeof(de));
	de.inum = xshort(root->ino);
	strcpy(de.name, ".");
	iappend(newfs, root->ino, &de, sizeof(de));
	bzero(&de, sizeof(de));
	de.inum = xshort(root->ino);
	strcpy(de.name, "..");
	iappend(newfs, root->ino, &de, sizeof(de));
	newfs->dirs = root;
	dprintf(2, "done\n");

	syncfs(newfs);

	return newfs;
}

Dir*
makedir(char *name, Dir *parent, int owner, short perms)
{
	struct dirent de;
	Dir *newdir;
	Dir *dirs;

	newdir = emallocz(sizeof(Dir));
	newdir->name = strdup(name);
	newdir->fs = parent->fs;
	newdir->parent = parent;
	newdir->ino = ialloc(parent->fs, T_DIR, owner, perms);
	newdir->owner = owner;
	newdir->perms = perms;

	// add to parent
	bzero(&de, sizeof(de));
	de.inum = xshort(newdir->ino);
	strcpy(de.name, newdir->name);
	iappend(parent->fs, parent->ino, &de, sizeof(de));

	// populate self
	bzero(&de, sizeof(de));
	de.inum = xshort(newdir->ino);
	strcpy(de.name, ".");
	iappend(newdir->fs, newdir->ino, &de, sizeof(de));
	bzero(&de, sizeof(de));
	de.inum = xshort(parent->ino);
	strcpy(de.name, "..");
	iappend(newdir->fs, newdir->ino, &de, sizeof(de));

	// add to fs object
	for(dirs = newdir->fs->dirs; dirs->next != nil; dirs = dirs->next)
		;
	dirs->next = newdir;

	return newdir;
}

Dir*
finddir(Fs *fs, char *name)
{
	Dir *dirs;

	for(dirs = fs->dirs; dirs != nil; dirs = dirs->next)
		if(!strcmp(name, dirs->name))
			return dirs;
	return nil;
}

File*
makefile(char *name, Dir *home, char *srcfile, int owner, short perms)
{
	File *newfile;
	File *files;
	struct dirent de;
	int srcfd;
	int c;
	char buf[BSIZE];

	newfile = emallocz(sizeof(File));
	newfile->name = strdup(name);
	newfile->srcfile = strdup(srcfile);
	newfile->home = home;
	newfile->owner = owner;
	newfile->perms = perms;
	newfile->ino = ialloc(newfile->home->fs, T_FILE, owner, perms);

	if((srcfd = open(srcfile, 0)) < 0){
		perror("open");
		exit(1);
	}

	bzero(&de, sizeof(de));
	de.inum = xshort(newfile->ino);
	strncpy(de.name, newfile->name, DIRSIZ);
	iappend(newfile->home->fs, newfile->home->ino, &de, sizeof(de));

	while((c = read(srcfd, buf, sizeof(buf))) > 0)
		iappend(newfile->home->fs, newfile->ino, buf, c);

	close(srcfd);

	if(newfile->home->fs->files == nil)
		newfile->home->fs->files = newfile;
	else {
		for(files = newfile->home->fs->files; files->next != nil; files = files->next)
			;
		files->next = newfile;
	}

	return newfile;
}

File*
findfile(Fs *fs, char *name)
{
	File *files;

	for(files = fs->files; files != nil ; files = files->next)
		if(!strcmp(name, files->name))
			return files;
	return nil;
}

void
kernelbrand(File *kernel)
{
	Fs *fs;
	
	fs = kernel->home->fs;
	fs->sb.kerninode = xshort(kernel->ino);
	kernbrand = 1;
	kernino = kernel->ino;
}

void
bootbrand(File *bootloader)
{
	Fs *fs;

	fs = bootloader->home->fs;
	fs->sb.bootable = 1;
	fs->sb.bootinode = xshort(bootloader->ino);
	fsbootable = 1;
	bootino = bootloader->ino;
}

Entry*
makeentry(Fs *fs, int type, char *name, char *home, char *source, uint size,
			int owner, short perms)
{
	Entry *newentry;

	newentry = emallocz(sizeof(Entry));
	newentry->type = type;
	newentry->active = 1;
	newentry->fs = fs;
	newentry->name = strtrim(name);
	newentry->home = strtrim(home);
	newentry->source = strtrim(source);
	newentry->size = size;
	newentry->owner = owner;
	newentry->perms = perms;

	return newentry;
}

void
addentry(Entry *newentry)
{
	Entry *e;

	if(!entries)
		entries = newentry;
	else {
		for(e = entries; e->next != nil; e = e->next)
			;
		e->next = newentry;
	}
}

char*
entrytype(Entry *e)
{
	switch(e->type){
	case FSYS:
		return "fsys";
	case DIRECT:
		return "direct";
	case EFILE:
		return "file";
	case KERNEL:
		return "kernel";
	case BOOTLOADER:
		return "boot";
	case SYNC:
		return "sync";
	}
	return "unknown";
}

void
printentrystring(Entry *e, int prefix)
{
	char buf[17];

	if(prefix)
		dprintf(2, "mkproto: ");
	switch(e->type){
	case FSYS:
		dprintf(2, "%s %s %s %d\n", entrytype(e), e->name, e->source, e->size);
		break;
	case DIRECT:
		dprintf(2, "%s: %s %s %s %d %s\n", e->fs->label, entrytype(e),
			e->name, e->home, e->owner, fmtpermsn(e->perms, buf, sizeof(buf)));
		break;
	case EFILE:
		dprintf(2, "%s: %s %s %s %s %d %s\n", e->fs->label, entrytype(e),
			e->home, e->name, e->source, e->owner, fmtpermsn(e->perms, buf, sizeof(buf)));
		break;
	case KERNEL:
	case BOOTLOADER:
		dprintf(2, "%s: %s %s\n", e->fs->label, entrytype(e), e->name);
		break;
	case SYNC:
		dprintf(2, "%s: %s\n", e->fs->label, entrytype(e));
		break;
	default:
		dprintf(2, "unknown type %d\n", e->type);
	}
}


void* // returns nil except for type FSYS
doentry(Entry *e)
{
	Dir *searchdir;
	File *searchfile;

	if(verbose)
		printentrystring(e, 1);
	if(!e->active)
		return nil;
	if(!neuter && e->type != FSYS)
		dprintf(2,".");
	switch(e->type){
	case FSYS:
		e->active = 0;
		if(neuter)
			return neuter_makefs(e->source, e->name, e->size, LOGSIZE, e->size/10);
		return makefs(e->source, e->name, e->size, LOGSIZE, e->size/10);
		break;
	case DIRECT:
		searchdir = finddir(e->fs, e->home);
		if(!searchdir){
			dprintf(2, "mkproto: directory \"%s\" not defined!\n", e->home);
			exit(1);
		}
		makedir(e->name, searchdir, e->owner, e->perms);
		break;
	case EFILE:
		searchdir = finddir(e->fs, e->home);
		if(!searchdir){
			dprintf(2, "mkproto: directory %s not defined!\n", e->home);
			exit(1);
		}
		makefile(e->name, searchdir, e->source, e->owner, e->perms);
		break;
	case KERNEL:
		searchfile = findfile(e->fs, e->name);
		if(!searchfile){
			dprintf(2, "mkproto: file %s not defined!\n", e->home);
			exit(1);
		}
		kernelbrand(searchfile);
		break;
	case BOOTLOADER:
		searchfile = findfile(e->fs, e->name);
		if(!searchfile){
			dprintf(2, "mkproto: file %s not defined!\n", e->home);
			exit(1);
		}
		bootbrand(searchfile);
		break;
	case SYNC:
		syncfs(e->fs);
		closefs(e->fs);
		break;
	default:
		dprintf(2, "mkproto: unknown entry type %d\n", e->type);
		exit(1);
		break;
	}
	e->active = 0;
	return nil;
}

char*
strsep_wrapper(char **stringp, const char *delim)
{
	char *token = nil;
	if(stringp == nil)
		return nil;
	if(*stringp == nil)
		return nil;
	do {
		token = strsep(stringp, delim);
		if(token == nil)
			return nil;
	} while(*token == 0);
	return token;
}

int
parseentrystring(char *line)
{
	char *freeme;
	char *linedup;
	char *token;

	char *name;
	char  *home;
	char *source0;
	char *source;
	char *s_owner;
	int owner;
	char *s_size0;
	char *s_size;
	uint size;
	char *perms1;
	char *perms2;
	short perms;
	
	Entry *ent;

	freeme = linedup = strtrim(line);
	token = strsep_wrapper(&linedup, " \t");
	if(token == nil){
		free(freeme);
		return 0;
	}
	if(*token == '\n'){
		free(freeme);
		return 0;
	}
	if(!strcmp(token, "fsys")){
		name = strsep_wrapper(&linedup, " \t");
		source0 = strsep_wrapper(&linedup, " \t");
		if(use_altfsfile)
			source = altfsfile;
		else
			source = source0;
		s_size0 = strsep_wrapper(&linedup, " \t");
		if(use_altsize)
			s_size = altsize;
		else
			s_size = s_size0;
		if(!name || !source0 || !s_size)
			goto fail;
		size = (uint)strtoul(s_size, nil, 10);
		ent = makeentry(nil, FSYS, name, "", source, size, 0, 0);
		active = doentry(ent);
		addentry(ent);
	} else if(!strcmp(token, "direct")) {
		name = strsep_wrapper(&linedup, " \t");
		home = strsep_wrapper(&linedup, " \t");
		s_owner = strsep_wrapper(&linedup, " \t");
		perms1 = strsep_wrapper(&linedup, ", \t");
		perms2 = strsep_wrapper(&linedup, ", \t");
		if(!name || !home || !s_owner || !perms1 || !perms2)
			goto fail;
		owner = atoi(s_owner);
		perms = parseperms(perms1, perms2);
		ent = makeentry(active, DIRECT, name, home, "", 0, owner, perms);
		addentry(ent);
	} else if(!strcmp(token, "file")) {
		home = strsep_wrapper(&linedup, " \t");
		name = strsep_wrapper(&linedup, " \t");
		source = strsep_wrapper(&linedup, " \t");
		s_owner = strsep_wrapper(&linedup, " \t");
		perms1 = strsep_wrapper(&linedup, ", \t");
		perms2 = strsep_wrapper(&linedup, ", \t");
		if(!name || !home || !source || !s_owner || !perms1 || !perms2)
			goto fail;
		owner = atoi(s_owner);
		perms = parseperms(perms1, perms2);
		ent = makeentry(active, EFILE, name, home, source, 0, owner, perms);
		addentry(ent);
	} else if(!strcmp(token, "kernel")) {
		name = strsep_wrapper(&linedup, " \t");
		if(!name)
			goto fail;
		ent = makeentry(active, KERNEL, name, "", "", 0, 0, 0);
		addentry(ent);
	} else if(!strcmp(token, "boot")) {
		name = strsep_wrapper(&linedup, " \t");
		if(!name)
			goto fail;
		ent = makeentry(active, BOOTLOADER, name, "", "", 0, 0, 0);
		addentry(ent);
	} else if(!strcmp(token, "sync")) {
		ent = makeentry(active, SYNC, "", "", "", 0, 0, 0);
		addentry(ent);
	} else {
		dprintf(2, "mkproto: unknown entry type \"%s\"\n", token);
	}
	free(freeme);
	return 0;

fail:
	dprintf(2, "mkproto: line missing fields \"%s\"\n", freeme);
	free(freeme);
	return -1;
}

void
runentries(Entry *ents)
{
	Entry *e;

	if(!neuter)
		dprintf(2, "mkproto: adding files");
	for(e = ents; e != nil; e = e->next){
		if(neuter)
			printentrystring(e, 1);
		else
			doentry(e);
	}
	if(!neuter){
		dprintf(2, "done\n");
		if(kernbrand)
			dprintf(2, "mkproto: kernel is inode %u\n", kernino);
		if(fsbootable)
			dprintf(2, "mkproto: fs bootable. bootloader is inode %u\n", bootino);
		dprintf(2, "mkproto: first %u blocks have been allocated\n", blocks_used);
	}
}

void
printentries(Entry *ents)
{
	Entry *e;

	for(e = ents; e != nil; e = e->next)
		printentrystring(e, 1);
}

void
usage(void)
{
	dprintf(2, "usage: mkproto [-dvnc] -f protofile [-o output] [-s size]\n");
	exit(0);
}

int
main(int argc, char *argv[])
{
	int havefile, ch;
	char *protofile;
	FILE *protofd;
	char *line;
	int cont;

	debug = 0;
	verbose = 0;
	neuter = 0;
	cont = 0;
	havefile = 0;
	fsbootable = 0;
	kernbrand = 0;
	use_altfsfile = 0;
	use_altsize = 0;

	while((ch = getopt(argc, argv, "dvncf:o:s:")) != -1){
		switch(ch){
		case 'd':
			debug = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'n':
			neuter = 1;
			break;
		case 'c':
			cont = 1;
			break;
		case 'f':
			havefile = 1;
			protofile = optarg;
			break;
		case 'o':
			use_altfsfile = 1;
			altfsfile = strdup(optarg);
			break;
		case 's':
			use_altsize = 1;
			altsize = strdup(optarg);
			break;
		case '?':
		default:
			usage();
		}
	}

	if(!havefile)
		usage();

	if(!(protofd = fopen(protofile, "r"))){
		dprintf(2, "mkproto: unable to open proto file %s\n", protofile);
		return -1;
	}

	while((line = readline(protofd))){
		if(parseentrystring(line) && !cont){
			dprintf(2, "mkproto: exiting...\n");
			return -1;
		}
		free(line);
	}

	runentries(entries);
	return 0;
}
