
#include "SystemFrontend.hpp"

using namespace std;

#ifndef SFML_SYSTEM_WINDOWS

SystemFrontend::SystemFrontend(const string& shell, int rows, int cols) {
	struct winsize ws;
	ws.ws_row = rows;
	ws.ws_col = cols;
	int ret = forkpty(&pty, 0, 0, &ws);
	if (ret == 0) {
		setenv("TERM", "xterm-256color", 1);
		execlp(shell.c_str(), shell.c_str(), NULL);
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
			while (running && (readlen = ::read(pty, buffer, sizeof(buffer))) >= 0) {
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
	close(pty);
	if (running) {
		kill(child, SIGTERM);
		waitpid(child, NULL, 0);
	}
	if (thReader) {
		if (thReader->joinable())
			thReader->join();
		delete thReader;
	}
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
