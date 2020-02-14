#pragma once

#if defined _WIN32 && defined USE_CYGWIN_DLL

#include "cygwin/cygload.h"

// Be sure to use this at the beginning of every stack calling POSIX functions
// The stack would be disorted; back up every variable you're going to use
#define CYGWIN_THREAD_PADDING cygwin::padding padding


// Call this only in the thread which you've already setup padding
void cygInit(const char* dllPath = "cygwin1.dll");


// Define the types and functions we're going to use
// Be sure to only use these functions in a single thread!!!

typedef int pid_t;
typedef int ssize_t;


// from sys/termios.h
#define NCCS 18

typedef unsigned char  cc_t;
typedef unsigned int   tcflag_t;
typedef unsigned int   speed_t;
typedef unsigned short otcflag_t;
typedef unsigned char  ospeed_t;

struct __oldtermios {
	otcflag_t	c_iflag;
	otcflag_t	c_oflag;
	otcflag_t	c_cflag;
	otcflag_t	c_lflag;
	char		c_line;
	cc_t		c_cc[NCCS];
	ospeed_t	c_ispeed;
	ospeed_t	c_ospeed;
};

struct termios {
	tcflag_t	c_iflag;
	tcflag_t	c_oflag;
	tcflag_t	c_cflag;
	tcflag_t	c_lflag;
	char		c_line;
	cc_t		c_cc[NCCS];
	speed_t	c_ispeed;
	speed_t	c_ospeed;
};

struct winsize {
	unsigned short ws_row, ws_col;
	unsigned short ws_xpixel, ws_ypixel;
};

#define TIOCGWINSZ (('T' << 8) | 1)
#define TIOCSWINSZ (('T' << 8) | 2)

#define SIGTERM 15

int forkpty(int* amaster, char* name, const struct termios* termp, const struct winsize* winp);

int setenv(const char* str, const char* value, int overwrite);

int execvp(const char* __file, const char* cmd[]);

int ioctl_TIOCGWINSZ(int fd, struct winsize* argp);
int ioctl_TIOCSWINSZ(int fd, const struct winsize* argp);

ssize_t read(int fd, void* buf, size_t bytecnt);
ssize_t write(int fd, const void* buf, size_t bytecnt);

int kill(pid_t pid, int sig);

int close(int fd);



#endif // defined _WIN32 && defined USE_CYGWIN_DLL
