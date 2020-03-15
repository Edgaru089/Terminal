#pragma once

#include <cstdlib>
#include <string>
#include <atomic>

#include <SFML/System.hpp>
#include <SFML/Network.hpp>


// Derivatives should have a construcor that does all the init work
class Frontend :public sf::NonCopyable {
public:

	static constexpr size_t npos = static_cast<size_t>(-1);

public:

	Frontend() {}
	virtual ~Frontend() {}

	virtual void stop() { running = false; }

	bool isRunning() { return running; }

public:

	// This function blocks; return value of 0 means failure
	virtual size_t read(void* data, size_t maxlen) { return npos; }

	// This function should never block
	// Return false means failure
	virtual bool write(const void* data, size_t len) = 0;

public:

	// This function shouldn't block
	virtual void resizeTerminal(int rows, int cols) {}

protected:

	std::atomic_bool running;

};

