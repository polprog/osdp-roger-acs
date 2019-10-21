/**********
 * Polprog 2018
 * 3 clause BSD licence
 * http://polprog.net
 */
#include "util.h"

int open_port(char *devname){
  int fd;
  fd = open(devname, O_RDWR | O_NOCTTY | O_NDELAY); //open rw, not a controlling terminal, ignore DCD
  if(fd < 0){
    fprintf(stderr, "Cannot open %s: %s\n", devname, strerror(errno));
    return fd;
  }
  return fd;
  
}


bool write_chars(int fd, char *data){
  printf("Writing %s; len=%d\n", data, strlen(data));
  int status = write(fd, data, strlen(data));
  printf("Written %s; len=%d\n", data, strlen(data));
  if(status < 0){
    fprintf(stderr, "Cannot write data to port: %s\n", strerror(errno));
    return false;
  }
  return true;
}

char *read_line(int fd, char *buf){
  
  int pos = 0;
  while(true){
    int status = read(fd, buf+pos, 1);
    if(status == -1) {
      if(errno == EAGAIN || errno == EWOULDBLOCK){
	//End of data
	*(buf+pos) = 0; //Null terminate the string
	break;
      }else{
	fprintf(stderr, "Error while reading: %s\n", strerror(errno));
	break;
      }
    }
    if(*(buf+pos) == '\n') {
      //Received end of line
      *(buf+pos) = 0; //Null terminate the string
      break;
    }
    pos++;
  }
  return buf;
}

void read_all(int fd){
  char * dummy = (char*) malloc(sizeof(char)); 
  while(read(fd, dummy, 1) != 0);
  free(dummy);
  
}


void set_rts(int fd){
  int flag = TIOCM_RTS;
  ioctl(fd, TIOCMBIS, &flag);
}


void clr_rts(int fd){
  int flag = TIOCM_RTS;
  ioctl(fd, TIOCMBIC, &flag);
}




void set_dtr(int fd){
  int flag = TIOCM_DTR;
  ioctl(fd, TIOCMBIS, &flag);
}


void clr_dtr(int fd){
  int flag = TIOCM_DTR;
  ioctl(fd, TIOCMBIC, &flag);
}
