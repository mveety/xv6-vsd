#include <libc.h>

typedef struct Thread Thread;
typedef struct Threadstate Threadstate;

struct Thread {
	int pid;
	void *stack;
	Thread *next;
};

struct Threadstate {
	Lock *lock;
	Thread *head;
	Thread *tail;
};

int _threads_useclone;
Threadstate *_thread_state;

void
_vsd_threadstart(void)
{
	_threads_useclone = 0;
	_thread_state = mallocz(sizeof(Threadstate));
	if(_thread_state == nil){
		printf(2, "threads: unable to initialize\n");
		exit();
	}
	_thread_state->lock = makelock("thread management lock");
}

Thread*
_vsd_allocate_thread()
{
	Thread *t;

	if(_thread_state->head == nil){
		t = mallocz(sizeof(Thread));
		_thread_state->head = t;
		_thread_state->tail = t;
		return t;
	}

	for(t = _thread_state->head; t != nil; t = t->next)
		if(t->pid == -1)
			return t;

	t = mallocz(sizeof(Thread));
	if(t == nil){
		unlock(_thread_state->lock);
		return nil;
	}
	_thread_state->tail->next = t;
	_thread_state->tail = _thread_state->tail->next;
	return t;
}

void
_vsd_threadentry(Thread *me, void (*entry)(void*), void *args)
{
	printf(2, "thread: start\n");
	entry(args);
	me->pid = -1;
	printf(2, "thread: exit\n");
	exit();
	printf(2, "thread: undead\n");
}

int
spawn(void (*entry)(void*), void *args)
{
	int tpid;
	Thread *new;

	lock(_thread_state->lock);
	if((new = _vsd_allocate_thread()) == nil){
		printf(2, "threads: panic: unable to allocate new thread\n");
		unlock(_thread_state->lock);
		exit();
	}
	new->pid = 0;
	if(_threads_useclone){
		unlock(_thread_state->lock);
		if(new->stack == nil){
			new->stack = malloc(4096);
			if(new->stack == nil){
				printf(2, "threads: panic: unable to allocate new stack\n");
				exit();
			}
		}
		switch(tpid = clone((uint)new->stack)){
		case 0:
			_vsd_threadentry(new, entry, args);
			exit();
			break;
		case -1:
			printf(2, "threads: unable to spawn new thread\n");
			new->pid = -1;
			return -1;
		}
	} else {
		switch(tpid = fork()){
		case 0:
			_vsd_threadentry(new, entry, args);
			exit();
			break;
		case -1:
			printf(2, "threads: fork: unable to spawn new thread\n");
			new->pid = -1;
			return -1;
		}
	}
	new->pid = tpid;
	return tpid;
}
