
#include <cstring>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>

#include <pty.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <stdlib.h>

#include "Packet.hpp"
#include "Connection.hpp"
#include "../WslOpcodes.hpp"

using namespace std;

atomic_bool running;

int pty, child;

int32_t rows = 140, cols = 40;
char* shell = const_cast<char*>("bash"), * workingDir = const_cast<char*>("~");
char* ipAddr = const_cast<char*>("127.0.0.1");
int port = 54323;
vector<char*> args = { nullptr };


void processArgs(int argc, char* argv[]) {
	args.clear();
	args.push_back(0);
	for (int i = 1; i < argc; i++) {
		if (!strncmp(argv[i], "COLS=", 5))
			cols = atoi(argv[i] + 5);
		if (!strncmp(argv[i], "ROWS=", 5))
			rows = atoi(argv[i] + 5);

		if (!strncmp(argv[i], "SHELL=", 6))
			shell = argv[i] + 6;
		if (!strncmp(argv[i], "CHDIR=", 6))
			workingDir = argv[i] + 6;
		if (!strncmp(argv[i], "ARG=", 4))
			args.push_back(argv[i] + 4);

		if (!strncmp(argv[i], "PORT=", 5))
			port = atoi(argv[i] + 5);
		if (!strncmp(argv[i], "IP=", 3))
			ipAddr = argv[i] + 3;
	}
	args[0] = shell;
	args.push_back(0);
}

Connection conn;
thread* thReader;


int main(int argc, char* argv[]) {
	processArgs(argc, argv);


	struct winsize ws;
	ws.ws_row = rows;
	ws.ws_col = cols;
	int ret = forkpty(&pty, 0, 0, &ws);
	if (ret == 0) {
		chdir(workingDir);
		setenv("TERM", "xterm-256color", 1);
		setenv("SHELL", shell, 1);
		execvp(shell, args.data());
	} else if (ret == -1) {
		fprintf(stderr, "WslBackend: forkpty() failed\n");
		return 1;
	} else {
		child = ret;
		printf("childid = %d\n", child);
	}

	fprintf(stderr, "WslBackend: Shell=%s, (%dx%d), Connecting to %s Port %d\n", shell, cols, rows, ipAddr, (int)port);

	if (!conn.connect(ipAddr, port)) {
		fprintf(stderr, "WslBackend: Error: Failure\n");
		return 1;
	}

	fprintf(stderr, "WslBackend: Connected\n");

	running = true;

	thReader = new thread(
		[&]() {
			Packet pack;
			char buffer[65536];
			ssize_t readlen;
			while (running && (readlen = ::read(pty, buffer, sizeof(buffer))) >= 0) {
				pack.clear();
				// Optimize this!
				pack.putChar(OPCODE_STRING);
				//pack << string((const char*)data, len);
				// Equivalent: This is how they do this
				pack.putUint32(readlen);
				pack.putRawData(buffer, readlen);
				conn.send(pack);
			}
			pack.clear();
			pack.putChar(OPCODE_STOP);
			conn.send(pack);
			running = false;
		}
	);

	Packet pack;
	string str;
	while (running && conn.receive(pack)) {
		unsigned char opcode = pack.getUint8();
		uint32_t slen;
		switch (opcode) {
		case OPCODE_STRING:
			slen = pack.getUint32();
			write(pty, reinterpret_cast<const char*>(pack.getData()) + pack.getReadOffset(), slen);
			break;
		case OPCODE_RESIZE:
			rows = pack.getInt32();
			cols = pack.getInt32();
			ws.ws_row = rows;
			ws.ws_col = cols;
			ioctl(pty, TIOCSWINSZ, &ws);
			break;
		case OPCODE_STOP:
			running = false;
			conn.disconnect();
			break;
		default:
			fprintf(stderr, "WslBackend: Warning: Unknown Opcode %d\n", (int)opcode);
		}
	}

	conn.disconnect();

	if (thReader->joinable())
		thReader->join();

	delete thReader;

}

