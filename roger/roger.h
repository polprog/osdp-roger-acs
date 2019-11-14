#include <inttypes.h>

int init_tty(const char *tty_device, int tty_baud_flag);
int init_net(const char* host, short port);

uint8_t epso_checksum(uint8_t *buf, uint8_t len);
int epso_write(int tty_fd, uint8_t addr, uint8_t func, uint8_t data);
int epso_write_read(int tty_fd, uint8_t addr, uint8_t func, uint8_t data, char *buf_out, uint16_t buf_len);

char readCardPin(int serial, char *buf, int bufSize, uint64_t* card, uint64_t* pin, char **cardStr, char **pinStr);
char readCardPin2(int serial, char* buf, int bufSize, uint64_t* cardNum, uint64_t* pinNum, char* cardStr, char* pinMd5Str);
