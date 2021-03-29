
#include "SystemFrontend.hpp"
#include <util.h>
#include <SFML/Config.hpp>

using namespace std;

#ifndef SFML_SYSTEM_WINDOWS

SystemFrontend::SystemFrontend(const string& shell, int rows, int cols) {
	struct winsize ws;
	ws.ws_row = rows;
	ws.ws_col = cols;
	int ret = forkpty(&pty, 0, 0, &ws);
	if (ret == 0) {
		setenv("TERM", "xterm-256color", 1);
#ifdef SFML_SYSTEM_MACOS // macOS expects terminal shells as login shells as default
		execlp(shell.c_str(), shell.c_str(), "--login", NULL);
#else
		execlp(shell.c_str(), shell.c_str(), NULL);
#endif
	} else if (ret == -1) {
		fprintf(stderr, "Unix SystemFrontend::Constructor: forkpty() failed\n");
		return;
	} else {
		child = ret;
		printf("childid = %d\n", child);
	}

	thReader = new thread(
		[&]() {
			char buffer[4096];
			ssize_t readlen;
			while (running && (readlen = ::read(pty, buffer, sizeof(buffer))) > 0) {
				bufReadLock.lock();
				bufRead.insert(bufRead.end(), buffer, buffer + readlen);
				bufReadLock.unlock();
			}
			running = false;
		}
	);

	running = true;
}


SystemFrontend::~SystemFrontend() {
	fprintf(stderr, "SystemFrontend.Unix shutting down\n");
	if (running) {
		if (kill(child, SIGTERM) != 0) // Process already ended
			waitpid(child, NULL, 0);
	}
	fprintf(stderr, "SystemFrontend.Unix: waitpid() ok\n");
#ifndef SFML_SYSTEM_MACOS
	close(pty);
	if (thReader) {
		if (thReader->joinable())
			thReader->join();
		delete thReader;
	}
#endif
	fprintf(stderr, "SystemFrontend.Unix shutdown\n");
}


bool SystemFrontend::write(const void* data, size_t len) {
	return ::write(pty, data, len) != -1;
}


void SystemFrontend::resizeTerminal(int rows, int cols) {
	struct winsize ws;
	ws.ws_row = rows;
	ws.ws_col = cols;
	ioctl(pty, TIOCSWINSZ, &ws);
}


#endif // ifndef SFML_SYSTEM_WINDOWS
