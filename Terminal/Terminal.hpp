#pragma once

#include <string>
#include <atomic>
#include <mutex>
#include <thread>
#include <functional>
#include <vector>
#include <deque>

#include <SFML/Graphics.hpp>

#ifdef SFML_SYSTEM_WINDOWS
#define NOMINMAX
#include <windows.h>
#else
#include <pty.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <signal.h>
#endif

#include "vterm/vterm.h"

#include "Frontend.hpp"


class Terminal :public sf::NonCopyable {
public:

	Terminal(Frontend* frontend, int rows = 40, int cols = 140, sf::Vector2i cellSize = sf::Vector2i(10, 20), int charSize = 18, bool useBold = false, int scrollbackMaxLength = 1000);
	~Terminal();

	void stop();

public:

	// Invalidates the whole screen
	void invalidate();

	// Returns true if redrawed, false otherwise
	bool redrawIfRequired(sf::Font& font, std::vector<sf::Vertex>& target, sf::Color bgColor = sf::Color::Black);
	// Forces a redraw
	void forceRedraw(sf::Font& font, std::vector<sf::Vertex>& target, sf::Color bgColor = sf::Color::Black);

public:

	void processEvent(sf::RenderWindow& win, sf::Event event);

	void update();

public:

	bool isRunning() {
		return running;
	}

	const std::string& getLastError() {
		return lastError;
	}

public:

	std::function<void()> bell;
	std::function<void(std::string)> cbSetWindowTitle;
	std::function<void(int width, int height)> cbSetWindowSize;

private:

	std::string lastError;

	// Terminal callbacks
	friend class TermCb;

	int cols, rows;
	sf::Vector2i cellSize;
	int charSize;
	bool hasBold;
	int scrollbackMaxLength;

	float charTopOffset;
	bool cursorVisible;

	sf::Vector2i lastMouseCell;

	bool needRedraw;

	std::string winTitle;

	VTerm* term;
	VTermScreen* screen;
	VTermState* state;

	VTermScreenCallbacks* screencb = nullptr;

	// One of VTERM_PROP_MOUSE_NONE, VTERM_PROP_MOUSE_CLICK, VTERM_PROP_MOUSE_DRAG, VTERM_PROP_MOUSE_MOVE
	// 0 means MOUSE_NONE
	int mouseState = VTERM_PROP_MOUSE_NONE;
	bool altScreen = false; // Is the alt-screen enabled?
	// Shape of the cursor
	// One of VTERM_PROP_CURSORSHAPE_BLOCK, VTERM_PROP_CURSORSHAPE_BAR_LEFT, VTERM_PROP_CURSORSHAPE_UNDERLINE
	int cursorShape = VTERM_PROP_CURSORSHAPE_BLOCK;

	std::deque<std::vector<VTermScreenCell>> scrollback;

	// Current scrollback offset, from the tail of the buffer
	int scrollbackOffset = 0;

	std::atomic_bool running;

	Frontend* frontend;

};

