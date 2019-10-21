
/**********
 * Polprog 2018
 * 3 clause BSD licence
 * http://polprog.net
 */
#ifndef _UTIL_H
#define _UTIL_H

#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include <stdlib.h>
#include <stdbool.h>
#include <sys/ioctl.h>

int open_port(char *devname);

bool write_chars(int fd, char *data);


/*
 * Return a "\r\n" ended string from port
 */
char *read_line(int fd, char *buf);

void read_all(int fd);


void set_rts(int fd);

void clr_rts(int fd);


void set_dtr(int fd);

void clr_dtr(int fd);

#endif //_UTIL_H
