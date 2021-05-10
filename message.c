#include <libc.h>

typedef struct mmessage mmessage;
struct mmessage {
	uint len;
	void *data;
};

void
msgpanic(char *str)
{
	printf(2, "msgpanic: %s: %r\n", str);
	exit();
}

mmessage*
marshal_message(Message *msg)
{
	mmessage *mmsg;
	void *fixedmsg;
	int *sentinel;
	void *payload;

	mmsg = mallocz(sizeof(mmessage));
	fixedmsg = mallocz(sizeof(int) + msg->len);

	if(!mmsg || !fixedmsg)
		msgpanic("bad malloc");
	
	sentinel = fixedmsg;
	payload = fixedmsg + sizeof(int);
	mmsg->len = msg->len + sizeof(int);

	*sentinel = msg->sentinel;
	memcpy(payload, msg->payload, msg->len);
	mmsg->data = fixedmsg;
	return mmsg;
}

Message*
unmarshal_message(mmessage *mmsg)
{
	Message *msg;
	int *sentinel;
	void *payload;

	msg = mallocz(sizeof(Message));
	if(!msg)
		msgpanic("bad malloc");
	msg->len = mmsg->len - sizeof(int);
	msg->payload = mallocz(msg->len);
	if(!msg->payload)
		msgpanic("bad malloc");
	sentinel = mmsg->data;
	payload = mmsg->data + sizeof(int);

	msg->sentinel = *sentinel;
	memcpy(msg->payload, payload, msg->len);
	return msg;
}

void
send2(int pid, mmessage *mmsg)
{
	sendmsg(pid, mmsg->data, mmsg->len);
}

mmessage*
recv2(void)
{
	mmessage *mmsg;

	mmsg = mallocz(sizeof(mmessage));
	if(!mmsg)
		msgpanic("bad malloc");
	mmsg->len = recvwait();
	mmsg->data = mallocz(mmsg->len);
	if(!mmsg)
		msgpanic("bad malloc");
	recvmsg(mmsg->data, mmsg->len);
	return mmsg;
}

void
send1(int pid, Message *msg)
{
	mmessage *mmsg;

	mmsg = marshal_message(msg);
	send2(pid, mmsg);
	free(mmsg);
}

Message*
recv1(void)
{
	mmessage *mmsg;
	Message *msg;

	mmsg = recv2();
	msg = unmarshal_message(mmsg);
	free(mmsg);
	return msg;
}

void
mboxpush(Mailbox *mbox, Message *msg)
{
	if(mbox->head == nil){
		mbox->head = msg;
		return;
	}
	msg->next = mbox->head;
	mbox->head = msg;
	mbox->messages++;
}

Message*
mboxnext(Mailbox *mbox)
{
	if(mbox->head == nil)
		return nil;
	if(mbox->cur == nil){
		mbox->looped = 0;
		mbox->cur = mbox->head;
		return mbox->cur;
	}
	mbox->cur = mbox->cur->next;
	if(mbox->cur == nil)
		mbox->looped = 1;
	return mbox->cur;
}

void
mboxselect(Mailbox *mbox, Message *msg)
{
	Message *newlst = nil;
	Message *ncur = nil;
	Message *cur;

	for(cur = mbox->head; cur != nil; cur = cur->next){
		if(msg == cur)
			continue;
		if(newlst == nil)
			newlst = ncur = cur;
		else {
			ncur->next = cur;
			ncur = cur;
		}
	}
	mbox->head = newlst;
	if(mbox->cur == msg){
		mbox->looped = 1;
		mbox->cur = nil;
	}
}

void
send(int pid, int sentinel, void *payload, int len)
{
	Message *msg;

	msg = mallocz(sizeof(Message));
	if(!msg)
		msgpanic("bad malloc");

	printf(2, "len = %d\n", len);
	msg->sentinel = sentinel;
	msg->payload = payload;
	msg->len = len;
	send1(pid, msg);
	free(msg);
}

Message*
receive(Mailbox *mbox)
{
	Message *msg;

	if(mbox->head == nil || (mbox->cur == nil && mbox->looped)){
		msg = recv1();
		mboxpush(mbox, msg);
		return msg;
	}
	return mboxnext(mbox);
}

void
selectmsg(Mailbox *mbox, Message *msg)
{
	mboxselect(mbox, msg);
	mbox->messages--;
	msg->next = nil;
}

int
freemsg(Message *msg)
{
	if(msg->next != nil)
		return -1;
	free(msg->payload);
	free(msg);
	return 0;
}

void
flush(Mailbox *mbox)
{
	Message *cur;
	Message *prev;

	cur = mbox->head;
	for(;;){
		prev = cur;
		cur = cur->next;
		free(prev->payload);
		free(prev);
		if(cur == nil)
			break;
	}
}

Mailbox*
mailbox(void)
{
	Mailbox *new;

	new = mallocz(sizeof(Mailbox));
	if(!new)
		msgpanic("bad malloc");
	return new;
}
