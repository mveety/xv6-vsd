#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "defs.h"
#include "spinlock.h"
#include "fs.h"
#include "file.h"
#include "errstr.h"
#include "x86.h"
#include "elf.h"
#include "users.h"

int
exec(char *path, char **argv)
{
	char *s, *last;
	int i, off;
	uint argc, sz, sp, ustack[3+MAXARG+1], ssp;
	struct elfhdr elf;
	struct inode *ip;
	struct proghdr ph;
	pde_t *pgdir, *oldpgdir;

	fail(proc->type == THREAD, EPTHREADEXEC, -1);

	if((ip = namei_direct(path)) == 0){
		seterr(EPCANTFIND);
		return -1;
	}
	begin_op(ip->dev);
	ilock(ip);
	pgdir = 0;
	if(checkinodeperm(ip, OP_EXEC) && singleuser == 0){
		seterr(ESNOPERMS);
		goto bad;
	}

	// Check ELF header
	if(readi(ip, (char*)&elf, 0, sizeof(elf)) < sizeof(elf)){
		cprintf("cpu%d: exec: short read\n", cpu->id);
		goto bad;
	}
	if(elf.magic != ELF_MAGIC){
		seterr(EPBADMAGIC);
		goto bad;
	}
	if((pgdir = setupkvm()) == 0){
		seterr(EKKVMFAIL);
		goto bad;
	}

	// Load program into memory.
	sz = 0;
	for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
		if(readi(ip, (char*)&ph, off, sizeof(ph)) != sizeof(ph)){
			seterr(EPBADHEADERREAD);
			goto bad;
		}
		if(ph.type != ELF_PROG_LOAD){
			continue;
		}
		if(ph.memsz < ph.filesz){
			seterr(EPFILETOOBIG);
			goto bad;
		}
		if((sz = allocuvm(pgdir, sz, ph.vaddr + ph.memsz)) == 0){
			seterr(EKBADUVMALLOC);
			goto bad;
		}
		if(loaduvm(pgdir, (char*)ph.vaddr, ip, ph.off, ph.filesz) < 0){
			seterr(EKBADUVMLOAD);
			goto bad;
		}
	}
	iunlockput(ip);
	end_op(ip->dev);
	ip = 0;

	// Allocate two pages at the next page boundary.
	// Make the first inaccessible.  Use the second as the user stack.
	sz = PGROUNDUP(sz);
	if((sz = allocuvm(pgdir, sz, sz + 2*PGSIZE)) == 0){
		seterr(EKBADUVMALLOC);
		goto bad;
	}
	clearpteu(pgdir, (char*)(sz - 2*PGSIZE));
	sp = sz;
	ssp = sp;

	// Push argument strings, prepare rest of stack in ustack.
	for(argc = 0; argv[argc]; argc++) {
		if(argc >= MAXARG){
			seterr(EPTOOMANYARGS);
			goto bad;
		}
		sp = (sp - (strlen(argv[argc]) + 1)) & ~3;
		if(copyout(pgdir, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
			goto bad;
		ustack[3+argc] = sp;
	}
	ustack[3+argc] = 0;

	ustack[0] = 0xffffffff;  // fake return PC
	ustack[1] = argc;
	ustack[2] = sp - (argc+1)*4;  // argv pointer

	sp -= (3+argc+1) * 4;
	if(copyout(pgdir, sp, ustack, (3+argc+1)*4) < 0)
		goto bad;

	// Save program name for debugging.
	for(last=s=path; *s; s++)
		if(*s == '/')
			last = s+1;
	safestrcpy(proc->name, last, sizeof(proc->name));

	// Commit to the user image.
	oldpgdir = proc->pgdir;
	proc->pgdir = pgdir;
	proc->sz = sz;
	proc->tf->eip = elf.entry;  // main
	proc->tf->esp = sp;
	proc->ssp = ssp;
	switchuvm(proc);
	freevm(oldpgdir);
	return 0;

 bad:
	if(pgdir)
		freevm(pgdir);
	if(ip){
		iunlockput(ip);
		end_op(ip->dev);
	}
	
	return -1;
}

