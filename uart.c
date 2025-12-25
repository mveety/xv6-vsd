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

struct uart {
	u16int addr;
	struct spinlock uartlock;
	struct inputbuf uartin;
};

int sysuart;
int uart = 0;    // is there a uart?
int uartn = 0;

struct uart uarts[4];

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
earlyuartputc(char c)
{
	// here we're going to assume the uart is initialized and working
	// from the bootloader. this ends up being basically the bootloader's
	// uartputch
	int i, j;

	for(i = 0; i < 128 && !(inb(COM1+5) & 0x20); i++)
		for(j = 0; j < 20; j++)
			;
	if(c == '\b')
		c = 'H' - '@';
	outb(COM1+0, c);
}

void
earlyuartprintstr(char *str)
{
	char *p;

	for(p = str; *p ; p++)
		earlyuartputc(*p);
}

void
uartinit(u16int addr)
{
	char *p;
	struct uart *curuart;

	cprintf("cpu%d: driver: starting uart%d\n", cpu->id, uartn);
	curuart = &uarts[uartn];
	curuart->addr = addr;
	// Turn off the FIFO
	outb(curuart->addr+2, 0);
	// 9600 baud, 8 data bits, 1 stop bit, parity off.
	outb(curuart->addr+3, 0x80);    // Unlock divisor
	outb(curuart->addr+0, 115200/9600);
	outb(curuart->addr+1, 0);
	outb(curuart->addr+3, 0x03);    // Lock divisor, 8 data bits.
	outb(curuart->addr+4, 0);
	outb(curuart->addr+1, 0x01);    // Enable receive interrupts.

	// If status is 0xFF, no serial port.
	curuart->uartin.iputc = &uartputc;
	if(inb(curuart->addr+5) == 0xFF){
		if(uart == 0){
			devsw[4].write = &nouart_write;
			devsw[4].read = &nouart_read;
		}
		return;
	}
	if(uart == 0){
	uart = 1;
		initlock(&curuart->uartlock, "uart lock");
		devsw[4].write = &uart_write;
		devsw[4].read = &uart_read;
	}
	// Acknowledge pre-existing interrupt conditions;
	// enable interrupts.
	inb(curuart->addr+2);
	inb(curuart->addr+0);

	switch(uartn){
	case 0:
		picenable(IRQ_COM1);
		ioapicenable(IRQ_COM1, 0);
		break;
	case 1:
		picenable(IRQ_COM2);
		ioapicenable(IRQ_COM2, 0);
		break;
	default:
		panic("uartn > 1");
		break;
	}
	uartn++;
}

#define BACKSPACE 0x100

void
uartputc(int saddr, int c)
{
	int i;
	u16int addr = (u16int)saddr;

	if(!uart)
		return;
	for(i = 0; i < 128 && !(inb(addr+5) & 0x20); i++)
		microdelay(10);
	if(c == BACKSPACE)
		c = C('H');  // could also be \x7f
	outb(addr+0, c);
}

void
sysuartputc(int c)
{
	struct uart *curuart = &uarts[0];

	uartputc(curuart->addr, c);
}

static int
uartgetc(int saddr)
{
	u16int addr = (u16int)saddr;

	if(!uart)
		return -1;
	if(!(inb(addr+5) & 0x01))
		return -1;
	return inb(addr+0);
}

void
uartintr(int uart)
{
	struct uart *curuart;

	if (uart >= uartn)
		return;

	curuart = &uarts[uart];
	Lock(curuart->uartlock);
	consoleintr(uartgetc, &curuart->uartin, curuart->addr);
	Unlock(curuart->uartlock);
}

int
uart_read(struct inode *ip, char *buf, int nbytes, int off)
{
	int c;
	int tot;
	int uart = 0;
	struct uart *curuart;

	uart = ip->minor;
	if(uart >= uartn){
		seterr(EDNOTEXIST);
		return -1;
	}
	curuart = &uarts[uart];

	tot = nbytes;
	iunlock(ip);
	Lock(curuart->uartlock);
	while(nbytes > 0){
		while(curuart->uartin.r == curuart->uartin.w){
			if(proc->killed){
				Unlock(curuart->uartlock);
				ilock(ip);
				return -1;
			}
			sleep(&curuart->uartin.r, &curuart->uartlock);
		}
		c = curuart->uartin.buf[curuart->uartin.r++ % INPUT_BUF];
		if(c == C('C')){
		//	proc->killed = 1;
		//	break;
		}
		if(c == C('D')){
			if(nbytes < tot)
				curuart->uartin.r--;
			break;
		}
		*buf++ = c;
		--nbytes;
		if(c == '\n')
			break;
	}
	Unlock(curuart->uartlock);
	ilock(ip);
	return tot - nbytes;
}

int
uart_write(struct inode *ip, char *buf, int nbytes, int off)
{
	int i;
	int uart = 0;
	struct uart *curuart;

	uart = ip->minor;
	if(uart >= uartn){
		seterr(EDNOTEXIST);
		return -1;
	}
	curuart = &uarts[uart];

	iunlock(ip);
	Lock(curuart->uartlock);
	for(i = 0; i < nbytes; i++)
		uartputc(curuart->addr, buf[i] & 0xff);
	Unlock(curuart->uartlock);
	ilock(ip);
	return nbytes;
}

