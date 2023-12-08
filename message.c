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
	if(msg->len > 0)
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
send(int pid, int sentinel, void *payload, int len)
{
	Message *msg;

	msg = mallocz(sizeof(Message));
	if(!msg)
		msgpanic("bad malloc");

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

	if(mbox != nil){
		lock(mbox);
		if(mbox->cur == nil){
			msg = recv1();
			msg->next = mbox->head;
			mbox->head = msg;
			mbox->cur = msg;
			mbox->messages++;
			unlock(mbox);
			return msg;
		}
		msg = mbox->cur;
		mbox->cur = mbox->cur->next;
		unlock(mbox);
		return msg;
	}
	msg = recv1();
	freemsg(msg);
	return nil;
}

void
selectmsg(Mailbox *mbox, Message *msg)
{
	Message *cur = nil;
	Message *prev = nil;

	if(mbox){
		lock(mbox);
		for(cur = mbox->head; cur != nil; cur = cur->next){
			if(cur == msg){
				if(prev == nil)
					mbox->head = cur->next;
				else
					prev->next = cur->next;
				if(msg == mbox->cur)
					mbox->cur = cur->next;
				mbox->messages--;
				msg->next = nil;
				if(mbox)
					unlock(mbox);
				return;
			}
			prev = cur;
		}
		unlock(mbox);
	}
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
	Message *prev = nil;

	cur = mbox->head;
	if(cur == nil)
		return;
	lock(mbox);
	for(;;){
		prev = cur;
		cur = cur->next;
		if(prev != nil){
			free(prev->payload);
			free(prev);
			mbox->messages--;
		}
		if(cur == nil)
			break;
	}
	unlock(mbox);
}

Mailbox*
mailbox(void)
{
	Mailbox *new;

	new = mallocz(sizeof(Mailbox));
	if(!new)
		msgpanic("bad malloc");
	initlock(new, "mailbox lock");
	return new;
}
