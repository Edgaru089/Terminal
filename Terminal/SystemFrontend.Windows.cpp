
#include "SystemFrontend.hpp"

using namespace std;

#ifdef SFML_SYSTEM_WINDOWS

SystemFrontend::SystemFrontend(const string& shell, int rows, int cols) {
	SetConsoleCP(CP_UTF8);
	//SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), ENABLE_VIRTUAL_TERMINAL_INPUT);
	//SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), ENABLE_VIRTUAL_TERMINAL_PROCESSING);

	// Open pipes
	SECURITY_ATTRIBUTES saAttr;
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = NULL;
#define ERROR_EXIT(str) {fprintf(stderr, "Windows SystemFrontend::Constructor: " str "\n"); return;}
	// Create a pipe for the child process's STDOUT.
	if (!CreatePipe(&childStdOutPipeRead, &childStdOutPipeWrite, &saAttr, 0))
		ERROR_EXIT("Error on StdoutRd CreatePipe");
	// Ensure the read handle to the pipe for STDOUT is not inherited.
	if (!SetHandleInformation(childStdOutPipeRead, HANDLE_FLAG_INHERIT, 0))
		ERROR_EXIT("Error on Stdout SetHandleInformation");
	// Create a pipe for the child process's STDIN.
	if (!CreatePipe(&childStdInPipeRead, &childStdInPipeWrite, &saAttr, 0))
		ERROR_EXIT("Error on Stdin CreatePipe");
	// Ensure the write handle to the pipe for STDIN is not inherited.
	if (!SetHandleInformation(childStdInPipeWrite, HANDLE_FLAG_INHERIT, 0))
		ERROR_EXIT("Error on Stdin SetHandleInformation");

	STARTUPINFOA si;
	PROCESS_INFORMATION pi;

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	si.hStdError = childStdOutPipeWrite;
	si.hStdOutput = childStdOutPipeWrite;
	si.hStdInput = childStdInPipeRead;
	si.dwFlags |= STARTF_USESTDHANDLES;
	ZeroMemory(&pi, sizeof(pi));

	// Start the child process.
	char* commandline = new char[shell.length() + 1];
	memcpy(commandline, shell.c_str(), shell.length());
	commandline[shell.length()] = 0;
	if (!CreateProcessA(
		NULL,                   // No module name (use command line)
		commandline,            // Command line
		NULL,                   // Process handle not inheritable
		NULL,                   // Thread handle not inheritable
		TRUE,                   // Handles are inherited
		CREATE_NO_WINDOW,       // No console window
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

	// Process running checker
	processRunningChecker = new thread(
		[&] {
			DWORD exitCode = 0;
			while (WaitForSingleObject(childProcessHandle, 100) != WAIT_FAILED) {
				GetExitCodeProcess(childProcessHandle, &exitCode);
				if (exitCode != STILL_ACTIVE)
					break;
			}
			running = false;
		}
	);

#undef ERROR_EXIT

	thReader = new thread(
		[&]() {
			char buffer[4096];
			DWORD readlen;
			while (running && ReadFile(childStdOutPipeRead, buffer, sizeof(buffer), &readlen, NULL)) {
				bufReadLock.lock();
				bufRead.insert(bufRead.end(), buffer, buffer + readlen);
				bufReadLock.unlock();
			}
			fprintf(stderr, "Windows SystemFrontend::thReader: Exited\n");
			running = false;
		}
	);

	running = true;
}


SystemFrontend::~SystemFrontend() {
	if (childStdInPipeWrite)
		CloseHandle(childStdInPipeWrite);
	if (childStdOutPipeRead)
		CloseHandle(childStdOutPipeWrite);

	if (running) {
		// Kill the child if it's still active for 100 milliseconds
		DWORD exitCode = 0;
		WaitForSingleObject(childProcessHandle, 100);
		GetExitCodeProcess(childProcessHandle, &exitCode);
		if (exitCode == STILL_ACTIVE)
			TerminateProcess(childProcessHandle, 0);
		CloseHandle(childProcessHandle);
	}

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


size_t SystemFrontend::read(void* data, size_t len) {
	DWORD readlen;
	if (!ReadFile(childStdOutPipeRead, data, len, &readlen, NULL))
		return 0;
	return readlen;
}


bool SystemFrontend::write(const void* data, size_t len) {
	DWORD size;
	return WriteFile(childStdInPipeWrite, data, len, &size, NULL);
	// TODO I don't really know when would size != len
}


void SystemFrontend::resizeTerminal(int rows, int cols) {

}


#endif // SFML_SYSTEM_WINDOWS
