
#include "WslFrontend.hpp"
#include "WslOpcodes.hpp"

#ifdef SFML_SYSTEM_WINDOWS

using namespace std;
using namespace sf;


WslFrontend::WslFrontend(const string& backendFilename, const string& wslShell, const string& workingDirWsl, int rows, int cols) {

	TcpListener listener;
	listener.listen(TcpListener::AnyPort, IpAddress("127.0.0.1"));
	Uint16 port = listener.getLocalPort();
	fprintf(stderr, "WslFrontend::Constructor: Listening on port %d\n", (int)port);

	SetConsoleCP(CP_UTF8);
	//SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), ENABLE_VIRTUAL_TERMINAL_INPUT);
	//SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), ENABLE_VIRTUAL_TERMINAL_PROCESSING);

#define ERROR_EXIT(str) {fprintf(stderr, "WslFrontend::Constructor: Error: " str "\n"); return;}

	STARTUPINFOA si;
	PROCESS_INFORMATION pi;

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));

	// Start the child process.
	const char* fmtString = "wsl -e ./%s -- SHELL=%s CHDIR=%s ROWS=%d COLS=%d IP=127.0.0.1 PORT=%d";
	char* commandline = new char[backendFilename.length() + wslShell.length() + workingDirWsl.length() + strlen(fmtString) + 8];
	sprintf(commandline, fmtString, backendFilename.c_str(), wslShell.c_str(), workingDirWsl.c_str(), rows, cols, (int)port);
	fprintf(stderr, "WslCommand: %s\n", commandline);
	if (!CreateProcessA(
		NULL,                   // No module name (use command line)
		commandline,            // Command line
		NULL,                   // Process handle not inheritable
		NULL,                   // Thread handle not inheritable
		TRUE,                   // Handles are not inherited
#ifdef NDEBUG
		CREATE_NO_WINDOW,       // No console window
#else
		NULL,                   // Use console window in debug
#endif
		NULL,                   // Use parent's environment block
		NULL,                   // Use parent's working directory
		&si,                    // Pointer to STARTUPINFO structure
		&pi)                    // Pointer to PROCESS_INFORMATION structure
		) {
		ERROR_EXIT("CreateProcess failed");
	}
	// Close handles.
	delete[] commandline;
	childProcessHandle = pi.hProcess;
	CloseHandle(pi.hThread);

	fprintf(stderr, "Child Id=%d\n", (int)GetProcessId(childProcessHandle));

	// Process running checker
	processRunningChecker = new thread(
		[&] {
			DWORD exitCode = 0;
			while (WaitForSingleObject(childProcessHandle, 100) != WAIT_FAILED) {
				GetExitCodeProcess(childProcessHandle, &exitCode);
				if (exitCode != STILL_ACTIVE)
					break;
			}
			fprintf(stderr, "WslFrontend::ProcessRunningChecker: Stopped\n");
			running = false;
		}
	);

	socket = new TcpSocket();
	listener.setBlocking(false);
	Clock clock;
	for (;;) {
		if (listener.accept(*socket) == Socket::Done)
			break;
		if (clock.getElapsedTime() > seconds(5.0f)) {
			delete socket;
			socket = nullptr;
			break;
		}
		sleep(milliseconds(10));
	}

	if (!socket)
		ERROR_EXIT("Listener Timeout");
	socket->setBlocking(true);

	thReader = new thread(
		[&]() {
			Packet pack;
			while (running && socket->receive(pack) == Socket::Done) {
				processPacket(pack);
				pack.clear();
			}
			fprintf(stderr, "WslFrontend::thReader: Exited\n");
			running = false;
		}
	);

	running = true;

#undef ERROR_EXIT
}


WslFrontend::~WslFrontend() {
	if (socket) {
		Packet p;
		p << OPCODE_STOP;
		socket->send(p);
		socket->disconnect();
	}

	// Kill the child if it's still active for 500 milliseconds
	DWORD exitCode = 0;
	GetExitCodeProcess(childProcessHandle, &exitCode);
	if (exitCode == STILL_ACTIVE) {
		WaitForSingleObject(childProcessHandle, 500);
		GetExitCodeProcess(childProcessHandle, &exitCode);
		if (exitCode == STILL_ACTIVE)
			TerminateProcess(childProcessHandle, 0);
	}
	CloseHandle(childProcessHandle);

	if (thReader) {
		if (thReader->joinable())
			thReader->join();
		delete thReader;
	}
	if (processRunningChecker) {
		if (processRunningChecker->joinable())
			processRunningChecker->join();
		delete processRunningChecker;
	}
}


size_t WslFrontend::tryRead(void* data, size_t maxlen) {
	lock_guard<mutex> lock(bufReadLock);

	if (bufRead.size() >= maxlen) {
		auto it = bufRead.begin();
		for (int i = 0; i < maxlen; i++) {
			((char*)data)[i] = *it;
			it++;
		}
		bufRead.erase(bufRead.begin(), it);
		return maxlen;
	} else if (bufRead.empty())
		return 0;
	else {
		auto it = bufRead.begin();
		size_t ans = bufRead.size();
		for (int i = 0; i < ans; i++) {
			((char*)data)[i] = *it;
			it++;
		}
		bufRead.clear();
		return ans;
	}
}


size_t WslFrontend::getBufferedSize() {
	bufReadLock.lock();
	size_t ans = bufRead.size();
	bufReadLock.unlock();
	return ans;
}


bool WslFrontend::write(const void* data, size_t len) {
	if (!socket)
		return false;
	static Packet pack;
	pack.clear();
	// Optimize this!
	pack << OPCODE_STRING;
	//pack << string((const char*)data, len);
	// Equivalent: This is how they do this
	pack << (Uint32)len;
	pack.append(data, len);
	return socket->send(pack) == Socket::Done;
}


void WslFrontend::resizeTerminal(int rows, int cols) {
	if (!socket)
		return;
	static Packet pack;
	pack.clear();
	pack << OPCODE_RESIZE << (Int32)rows << (Int32)cols;
	socket->send(pack);
}


void WslFrontend::processPacket(Packet& p) {
	Uint8 opcode = 255;
	p >> opcode;
	switch (opcode) {
	case OPCODE_STRING:
	{
		bufReadLock.lock();
		bufRead.insert(bufRead.end(), reinterpret_cast<const char*>(p.getData()) + sizeof(Uint8) + sizeof(Uint32), reinterpret_cast<const char*>(p.getData()) + p.getDataSize());
		bufReadLock.unlock();
		break;
	}
	case OPCODE_RESIZE:
		break; // Ignoring
	case OPCODE_STOP:
		socket->disconnect();
		running = false;
		break;
	default:
		fprintf(stderr, "WslFrontend::ProcessPacket: Warning: Unknown OpCode %d\n", (int)opcode);
	}
}

#endif

