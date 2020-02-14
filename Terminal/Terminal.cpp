
#include "Terminal.hpp"
#include "vterm/vterm.h"
#include "vterm/vterm_internal.h"

#ifdef USE_CYGWIN_DLL
#include "CygwinPosixWrapper.hpp"
#endif

#include <climits>
#include <cstring>

using namespace std;
using namespace sf;


// Terminal callbacks
class TermCb {
public:

	static int termBell(void* user) {
		Terminal* term = reinterpret_cast<Terminal*>(user);
		// TODO Bell
		return 0;
	}

	static int termResize(int rows, int cols, void* user) {
		Terminal* term = reinterpret_cast<Terminal*>(user);
		if ((term->rows != rows || term->cols != cols)) {
			term->rows = rows;
			term->cols = cols;
			if (term->cbSetWindowSize)
				term->cbSetWindowSize(cols * term->cellSize.x, rows * term->cellSize.y);
			term->invalidate();
		}
		return 1;
	}

	static int termProp(VTermProp prop, VTermValue* val, void* user) {
		Terminal* term = reinterpret_cast<Terminal*>(user);
		switch (prop) {
		case VTERM_PROP_TITLE:
		{
			string str(val->string);
			if (str.empty())
				str = "Terminal";
			else
				str += " - Terminal";
			if (term->winTitle != str) {
				term->winTitle = move(str);
				if (term->cbSetWindowTitle)
					term->cbSetWindowTitle(term->winTitle);
			}
			return 0;
		}
		case VTERM_PROP_CURSORVISIBLE:
		{
			term->cursorVisible = val->boolean;
			term->invalidate();
		}
		}
		return 1;
	}

	static int termDamage(VTermRect rect, void* user) {
		Terminal* term = reinterpret_cast<Terminal*>(user);
		term->invalidate();
		return 0;
	}

	static int termMoveCursor(VTermPos newpos, VTermPos oldpos, int visible, void* user) {
		Terminal* term = reinterpret_cast<Terminal*>(user);
		term->invalidate();
		return 0;
	}

	static int termPopLine(int col, VTermScreenCell* data, void* user) {
		Terminal* term = reinterpret_cast<Terminal*>(user);
		term->invalidate();
		return 0;
	}

	static int termPushLine(int col, const VTermScreenCell* data, void* user) {
		Terminal* term = reinterpret_cast<Terminal*>(user);
		term->invalidate();
		return 0;
	}

	// Platform-depedent write function
	static void writeCall(const char* s, size_t len, void* data) {
		Terminal* term = reinterpret_cast<Terminal*>(data);
#if defined SFML_SYSTEM_WINDOWS && !defined USE_CYGWIN_DLL
		DWORD size;
		WriteFile(term->childStdInPipeWrite, s, len, &size, NULL);
#else
		write(term->pty, (void*)s, len);
#endif
	}

	// Platform-depedent reader that runs in another thread
	static void reader(Terminal* term) {
#if defined SFML_SYSTEM_WINDOWS && !defined USE_CYGWIN_DLL
		char buffer[1024];
		DWORD readlen;
		while (term->running && ReadFile(term->childStdOutPipeRead, buffer, sizeof(buffer) - 1, &readlen, NULL)) {
			buffer[readlen] = '\0';
			fprintf(stderr, "Output:%s\n", buffer);
			term->tlock.lock();
			vterm_input_write(term->term, buffer, readlen);
			term->tlock.unlock();
		}
#else
		char buffer[1024];
		ssize_t readlen;
		while (term->running && (readlen = read(term->pty, buffer, sizeof(buffer))) >= 0) {
			term->tlock.lock();
			//buffer[readlen] = '\0';
			//printf("Read %d bytes: %s\n", (int)readlen, buffer);
			vterm_input_write(term->term, buffer, readlen);
			term->tlock.unlock();
		}
		term->stop();
#endif
	}
};


Terminal::Terminal(
	int cols, int rows,
	sf::Vector2i cellSize,
	int charSize, bool useBold)
	:cols(cols), rows(rows), cellSize(cellSize), charSize(charSize), hasBold(useBold), winTitle("Terminal"), charTopOffset(-4096), cursorVisible(true) {

	running = true;

	// Allocate VTerm object
	term = vterm_new(rows, cols);
	vterm_state_reset(state = vterm_obtain_state(term), 1);
	vterm_screen_reset(screen = vterm_obtain_screen(term), 1);
	vterm_screen_enable_altscreen(screen, 1);
	vterm_set_utf8(term, true);
	vterm_state_set_bold_highbright(state, true);

	// Setup callbacks
	//VTermStateCallbacks statecb;
	//memset(&statecb, 0, sizeof(statecb));
	//statecb.resize = &termResize;
	//vterm_state_set_callbacks(vterm_obtain_state(term), &statecb, 0);

	VTermScreenCallbacks* screencb = new VTermScreenCallbacks;
	memset(screencb, 0, sizeof(VTermScreenCallbacks));
	screencb->settermprop = &TermCb::termProp;
	//screencb->resize = &TermCb::termResize;
	screencb->damage = &TermCb::termDamage;
	screencb->movecursor = &TermCb::termMoveCursor;
	screencb->sb_pushline = &TermCb::termPushLine;
	screencb->sb_popline = &TermCb::termPopLine;
	screencb->bell = &TermCb::termBell;
	vterm_screen_set_callbacks(screen, screencb, reinterpret_cast<void*>(this));
	this->screencb = reinterpret_cast<void*>(screencb);

	VTermColor bg, fg;
	vterm_color_rgb(&bg, 0, 0, 0);
	vterm_color_rgb(&fg, 255, 255, 255);
	vterm_state_set_default_colors(state, &fg, &bg);

	vterm_output_set_callback(term, &TermCb::writeCall, reinterpret_cast<void*>(this));
	vterm_set_size(term, rows, cols);

}

Terminal::~Terminal() {
	running = false;

#if defined SFML_SYSTEM_WINDOWS && !defined USE_CYGWIN_DLL
	// Let's close the write side of handle StdIn and StdOut
	CloseHandle(childStdInPipeWrite);
	CloseHandle(childStdOutPipeWrite);
	// Kill the child if it's still active for 100 milliseconds
	DWORD exitCode = 0;
	WaitForSingleObject(childProcessHandle, 100);
	GetExitCodeProcess(childProcessHandle, &exitCode);
	if (exitCode == STILL_ACTIVE)
		TerminateProcess(childProcessHandle, 0);
#else
	kill(child, SIGTERM);
	close(pty);
#endif

	if (reader) {
		if (reader->joinable())
			reader->join();
		delete reader;
	}
#if defined SFML_SYSTEM_WINDOWS && !defined USE_CYGWIN_DLL
	if (processRunningChecker) {
		if (processRunningChecker->joinable())
			processRunningChecker->join();
		delete processRunningChecker;
	}
	CloseHandle(childProcessHandle);
#endif
	vterm_free(term);
	if (screencb)
		delete reinterpret_cast<VTermScreenCallbacks*>(screencb);
}


bool Terminal::launch(const string& childCommand) {
#if defined SFML_SYSTEM_WINDOWS && !defined USE_CYGWIN_DLL
	SetConsoleCP(CP_UTF8);
	//SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), ENABLE_VIRTUAL_TERMINAL_INPUT);
	//SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), ENABLE_VIRTUAL_TERMINAL_PROCESSING);

	// Open pipes
	SECURITY_ATTRIBUTES saAttr;
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = NULL;
#define ERROR_EXIT(str) {lastError = "Windows Terminal::launch: " str; vterm_input_write(term, lastError.c_str(), lastError.size()); invalidate(); return false;}
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
	const string& shell = childCommand;
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
	childProcessHandle = pi.hProcess;
	fprintf(stderr, "Child PID=%d\n", (int)GetProcessId(childProcessHandle));
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
			if (exitCode == 0)
				running = false;
			else {
				winTitle += " [INACTIVE: Status: " + to_string(exitCode) + ']';
				if (cbSetWindowTitle)
					cbSetWindowTitle(winTitle);
			}
			fprintf(stderr, "ExitCode:%d\n", (int)exitCode);
		}
	);

#undef ERROR_EXIT

#else
	struct winsize ws;
	ws.ws_row = rows;
	ws.ws_col = cols;
	int ret = forkpty(&pty, 0, 0, &ws);
	if (ret == 0) {
		string shell = childCommand;
		setenv("TERM", "xterm-256color", 1);
		const char* args[] = { shell.data(),0 };
		execvp(shell.data(), args);
	} else if (ret == -1) {
		printf("Unix Terminal::launch: forkpty() failed");
		lastError = ("Unix Terminal::launch: forkpty() failed");
		return false;
	} else {
		child = ret;
		printf("childid = %d\n", child);
}
#endif

	reader = new thread(TermCb::reader, this);

	return true;
}


void Terminal::stop() {
	running = false;
}


void Terminal::invalidate() {
	needRedraw = true;
}

