// Intel 8250 serial port (UART).

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

#define COM1    0x3f8

int sysuart;
static int uart;    // is there a uart?
struct spinlock uartlock;
struct inputbuf uartin;

int uart_read(struct inode*, char*, int, int);
int uart_write(struct inode*, char*, int, int);

int
nouart_read(struct inode *ip, char *c, int nbytes, int off)
{
	seterr(EDNOTEXIST);
	return -1;
}

int
nouart_write(struct inode *ip, char *c, int nbytes, int off)
{
	seterr(EDNOTEXIST);
	return -1;
}

void
uartinit(void)
{
	char *p;

	// Turn off the FIFO
	outb(COM1+2, 0);
	// 9600 baud, 8 data bits, 1 stop bit, parity off.
	outb(COM1+3, 0x80);    // Unlock divisor
	outb(COM1+0, 115200/9600);
	outb(COM1+1, 0);
	outb(COM1+3, 0x03);    // Lock divisor, 8 data bits.
	outb(COM1+4, 0);
	outb(COM1+1, 0x01);    // Enable receive interrupts.

	// If status is 0xFF, no serial port.
	uartin.iputc = &uartputc;
	if(inb(COM1+5) == 0xFF){
		devsw[4].write = &nouart_write;
		devsw[4].read = &nouart_read;
		return;
	}
	uart = 1;
	initlock(&uartlock, "uart lock");
	devsw[4].write = &uart_write;
	devsw[4].read = &uart_read;
	// Acknowledge pre-existing interrupt conditions;
	// enable interrupts.
	inb(COM1+2);
	inb(COM1+0);
	picenable(IRQ_COM1);
	ioapicenable(IRQ_COM1, 0);
	
	// Announce that we're here.
	for(p="xv6...\n"; *p; p++)
		uartputc(*p);
}

#define BACKSPACE 0x100

void
uartputc(int c)
{
	int i;

	if(!uart)
		return;
	for(i = 0; i < 128 && !(inb(COM1+5) & 0x20); i++)
		microdelay(10);
	if(c == BACKSPACE)
		c = C('H');  // could also be \x7f
	outb(COM1+0, c);
}

static int
uartgetc(void)
{
	if(!uart)
		return -1;
	if(!(inb(COM1+5) & 0x01))
		return -1;
	return inb(COM1+0);
}

void
uartintr(void)
{
	Lock(uartlock);
	consoleintr(uartgetc, &uartin);
	Unlock(uartlock);
}

int
uart_read(struct inode *ip, char *buf, int nbytes, int off)
{
	int c;
	int tot;
	
	tot = nbytes;
	iunlock(ip);
	Lock(uartlock);
	while(nbytes > 0){
		while(uartin.r == uartin.w){
			if(proc->killed){
				Unlock(uartlock);
				ilock(ip);
				return -1;
			}
			sleep(&uartin.r, &uartlock);
		}
		c = uartin.buf[uartin.r++ % INPUT_BUF];
		if(c == C('C')){
		//	proc->killed = 1;
		//	break;
		}
		if(c == C('D')){
			if(nbytes < tot)
				uartin.r--;
			break;
		}
		*buf++ = c;
		--nbytes;
		if(c == '\n')
			break;
	}
	Unlock(uartlock);
	ilock(ip);
	return tot - nbytes;
}

int
uart_write(struct inode *ip, char *buf, int nbytes, int off)
{
	int i;
	iunlock(ip);
	Lock(uartlock);
	for(i = 0; i < nbytes; i++)
		uartputc(buf[i] & 0xff);
	Unlock(uartlock);
	ilock(ip);
	return nbytes;
}

