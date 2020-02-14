#pragma once

#include <string>
#include <atomic>
#include <mutex>
#include <thread>
#include <functional>
#include <vector>

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

#include "Frontend.hpp"


struct VTerm;
struct VTermScreen;
struct VTermState;

class Terminal :public sf::NonCopyable {
public:

	Terminal(Frontend* frontend, int cols = 140, int rows = 40, sf::Vector2i cellSize = sf::Vector2i(10, 20), int charSize = 18, bool useBold = false);
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

	float charTopOffset;
	bool cursorVisible;

	sf::Vector2i lastMouseCell;

	bool needRedraw;

	std::string winTitle;

	VTerm* term;
	VTermScreen* screen;
	VTermState* state;

	void* screencb; // TODO: This is ugly; it's of type VTermScreenCallbacks*

	std::atomic_bool running;

	Frontend* frontend;

};

