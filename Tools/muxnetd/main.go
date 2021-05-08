package main

import (
	"bufio"
	"fmt"
	"io"
	"log"
	"net"
	"os"
	"sync"
)

type Line struct {
	id    byte
	inuse bool
	input chan byte // the input buffer
}

type Muxbuf struct {
	partial byte
	header  byte
	data    byte
}

type Muxchan struct {
	fd      *os.File
	input   chan Muxbuf
	output  chan Muxbuf
	rinput  chan byte
	routput chan byte
}

var com Muxchan
var linelock sync.Mutex
var lines [16]Line
var logger *log.Logger

func nop() {
}

func muxgetmsg(m *Muxchan) Muxbuf {
	return <-m.input
}

func muxputmsg(m *Muxchan, b Muxbuf) {
	m.output <- b
}

func muxrioproc(m *Muxchan) {
	// this is the byte reader
	go func(m *Muxchan) {
		var buf byte
		var err error
		rdr := bufio.NewReader(m.fd)
		for {
			buf, err = rdr.ReadByte()
			if err == io.EOF {
				break
			}
			m.rinput <- buf
		}
	}(m)
	// this is the byte writer
	var buf byte
	var err error
	wrtr := bufio.NewWriter(m.fd)
	for {
		buf = <-m.routput
		err = wrtr.WriteByte(buf)
		if err != nil {
			break
		}
	}
}

// TODO: might want to change this to a single coroutine and an alt
func muxioproc(m *Muxchan) {
	// this turns bytes from the channel to messages
	go func(m *Muxchan) {
		var buf byte
		var tmp Muxbuf
		for {
			buf = <-m.rinput
			switch tmp.partial {
			case 0:
				tmp.header = buf
				tmp.partial++
			case 1:
				tmp.data = buf
				tmp.partial = 0
				m.input <- tmp
				tmp.header = 0
				tmp.data = 0
			}
		}
	}(m)
	// this takes messages and turns them into bytes
	var buf Muxbuf
	for {
		buf = <-m.output
		m.routput <- buf.header
		m.routput <- buf.data
	}
}

func lputc(l *Line, c byte) {
	var buf Muxbuf
	buf.header = (l.id << 4) | 0x0D
	buf.data = c
	muxputmsg(&com, buf)
}

func marshal() {
	var buf Muxbuf
	var line, cmd, dat byte
	for {
		buf = muxgetmsg(&com)
		line = (buf.header & 0xf0) >> 4
		cmd = (buf.header & 0x0f)
		dat = buf.data
		switch cmd {
		case 0x0:
			continue
		case 0x1:
			fallthrough
		case 0x2:
			fallthrough
		case 0x3:
			fallthrough
		case 0x4:
			fallthrough
		case 0x5:
			fallthrough
		case 0x6:
			fallthrough
		case 0x7:
			fallthrough
		case 0x8:
			logger.Printf("muxnetd: unknown command %x for line %d with data %x\n",
				cmd, line, dat)
		case 0xd:
			if lines[line].inuse {
				lines[line].input <- dat
			} else {
				logger.Println("muxnetd: write to unused line", line)
			}
		}
	}
}

func line2conn(l *Line, cn *net.Conn) {
	die := make(chan int)
	// reader
	go func(ln *Line, cn *net.Conn) {
		var buf byte
		var err error
		rdr := bufio.NewReader(*cn)
		for {
			select {
			case <-die:
				break
			default:
				buf, err = rdr.ReadByte()
				if err != nil {
					die <- 1
					break
				}
				lputc(l, buf&0x7f)
			}
		}
	}(l, cn)
	// writer
	var buf byte
	var err error
	wrtr := bufio.NewWriter(*cn)
	for {
		select {
		case <-die:
			break
		case buf = <-l.input:
			err = wrtr.WriteByte(buf)
			if err != nil {
				die <- 1
				break
			}
		}
	}
	l.inuse = false
	for {
		select {
		case <-l.input:
			nop()
		default:
			break
		}
	}
}

func initlines() {
	var l *Line
	for i := 0; i < 16; i++ {
		l = &lines[i]
		l.input = make(chan byte, 16)
		l.inuse = false
		l.id = byte(i)
	}
}

func initmuxchan(ch *Muxchan, file string) error {
	var err error
	ch.input = make(chan Muxbuf, 32)
	ch.output = make(chan Muxbuf)
	ch.rinput = make(chan byte, 32)
	ch.routput = make(chan byte)
	ch.fd, err = os.OpenFile(file, os.O_RDWR, 0666)
	if err != nil {
		return err
	}
	return nil
}

func muxchanmain(ch *Muxchan, file string) {
	err := initmuxchan(ch, file)
	if err != nil {
		log.Fatal(err)
	}
	go muxrioproc(ch)
	go muxioproc(ch)
	logger.Println("mux serial channel open and running")
}

func clientconn(conn net.Conn, ch *Muxchan) {
	var l *Line
	linelock.Lock()
	for i := 0; i < 16; i++ {
		if !lines[i].inuse {
			l = &lines[i]
			l.inuse = true
			fmt.Fprintf(conn, "Connected to line %d\n", l.id)
			logger.Printf("Conn %v connected to line %d\n", conn, l.id)
			break
		}
	}
	linelock.Unlock()
	if l == nil {
		fmt.Fprint(conn, "Could not find a free line\n")
		logger.Printf("Could not find a free line for %v\n", conn)
		return
	}
	line2conn(l, &conn)
	logger.Printf("%v disconnected from line %v", conn, l.id)
	conn.Close()
}

func server(addr string, ch *Muxchan) {
	ln, err := net.Listen("tcp", addr)
	if err != nil {
		logger.Fatal(err)
	}
	for {
		conn, err := ln.Accept()
		if err != nil {
			logger.Fatal(err)
		}
		go clientconn(conn, ch)
	}
}

func main() {
	fmt.Println("telnet to veetymux software multiplexer\n")
	logger = log.New(os.Stderr, "muxnetd: ", log.Lshortfile|log.LstdFlags)
	initlines()
	if len(os.Args) == 1 {
		logger.Println("usage: muxnetd [com port]\n")
		os.Exit(-1)
	}
	err := initmuxchan(&com, os.Args[1])
	if err != nil {
		logger.Fatal(err)
	}
	server(":2323", &com)
}
