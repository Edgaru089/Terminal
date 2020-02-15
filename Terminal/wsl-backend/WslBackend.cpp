
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

#include <SFML/Network.hpp>

#include "../WslOpcodes.hpp"

using namespace std;
using namespace sf;

atomic_bool running;

int pty, child;

Int32 rows = 140, cols = 40;
char* shell = const_cast<char*>("bash"), *workingDir = const_cast<char*>("~");
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

TcpSocket* socket;
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

	socket = new TcpSocket();
	if (socket->connect(IpAddress(ipAddr), port, seconds(5.0f)) != Socket::Done) {
		delete socket;
		fprintf(stderr, "WslBackend: Error: Timeout\n");
		return 1;
	}

	fprintf(stderr, "WslBackend: Connected, Peer=%s, %d\n", socket->getRemoteAddress().toString().c_str(), (int)socket->getRemotePort());

	running = true;

	thReader = new thread(
		[&]() {
			Packet pack;
			char buffer[4096];
			ssize_t readlen;
			while (running && (readlen = ::read(pty, buffer, sizeof(buffer))) >= 0) {
				pack.clear();
				// Optimize this!
				pack << OPCODE_STRING;
				//pack << string((const char*)data, len);
				// Equivalent: This is how they do this
				pack << (Uint32)readlen;
				pack.append(buffer, readlen);
				socket->send(pack);
			}
			pack.clear();
			pack << OPCODE_STOP;
			socket->send(pack);
			running = false;
		}
	);

	Packet pack;
	string str;
	while (running && (socket->receive(pack) == Socket::Done)) {
		Uint8 opcode;
		pack >> opcode;
		switch (opcode) {
		case OPCODE_STRING:
			pack >> str;
			write(pty, str.c_str(), str.length());
			break;
		case OPCODE_RESIZE:
			pack >> rows >> cols;
			ws.ws_row = rows;
			ws.ws_col = cols;
			ioctl(pty, TIOCSWINSZ, &ws);
			break;
		case OPCODE_STOP:
			running = false;
			socket->disconnect();
			break;
		default:
			fprintf(stderr, "WslBackend: Warning: Unknown Opcode %d\n", (int)opcode);
		}
	}

	socket->disconnect();

	if (thReader->joinable())
		thReader->join();

	delete thReader;
	delete socket;
}

