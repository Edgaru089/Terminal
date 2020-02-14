
#if defined _WIN32 && defined USE_CYGWIN_DLL

#include "CygwinPosixWrapper.hpp"

#define NOMINMAX
#include <windows.h>

namespace {
	HMODULE cyg = 0;

	template <class T> void get_symbol(const char* name, T& symbol) {
		FARPROC retval = NULL;

		retval = GetProcAddress(cyg, name);

		if (retval == NULL)
			throw windows_error("GetProcAddress", name);

		symbol = reinterpret_cast <T> (retval);
	}

	int (*_forkpty)(int*, char*, const struct termios*, const struct winsize*);
	int (*_setenv)(const char*, const char*, int);
	int (*_execvp)(const char*, const char* []);
	int (*_ioctl)(int, int, ...);
	ssize_t(*_read)(int, void*, size_t);
	ssize_t(*_write)(int, const void*, size_t);
	int (*_kill)(pid_t, int);
	int (*_close)(int);
}

void cygInit(const char* dllPath) {

	cyg = LoadLibraryA(dllPath);

	reinterpret_cast<void(*)()>(GetProcAddress(cyg, "cygwin_dll_init"))();

	get_symbol("forkpty", _forkpty);
	get_symbol("setenv", _setenv);
	get_symbol("execvp", _execvp);
	get_symbol("ioctl", _ioctl);
	get_symbol("read", _read);
	get_symbol("write", _write);
	get_symbol("kill", _kill);
	get_symbol("close", _close);

}

void cygFree() {
	if (cyg)
		FreeLibrary(cyg);
}


int forkpty(int* amaster, char* name, const struct termios* termp, const struct winsize* winp) {
	return _forkpty(amaster, name, termp, winp);
}

int setenv(const char* str, const char* value, int overwrite) {
	return _setenv(str, value, overwrite);
}

int execvp(const char* file, const char* cmd[]) {
	return _execvp(file, cmd);
}

int ioctl_TIOCGWINSZ(int fd, struct winsize* argp) {
	return _ioctl(fd, TIOCGWINSZ, argp);
}

int ioctl_TIOCSWINSZ(int fd, const struct winsize* argp) {
	return _ioctl(fd, TIOCSWINSZ, argp);
}

ssize_t read(int fd, void* buf, size_t bytecnt) {
	return _read(fd, buf, bytecnt);
}
ssize_t write(int fd, const void* buf, size_t bytecnt) {
	return _write(fd, buf, bytecnt);
}

int kill(pid_t pid, int sig) {
	return _kill(pid, sig);
}

int close(int fd) {
	return _close(fd);
}


#endif //defined _WIN32 && defined USE_CYGWIN_DLL
