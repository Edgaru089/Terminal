#pragma once

#include <SFML/Config.hpp>
#include <SFML/Network.hpp>

#ifdef SFML_SYSTEM_WINDOWS
#include <deque>
#include <thread>
#include <mutex>
#include <condition_variable>

#include <windows.h>

#include "Frontend.hpp"


class WslFrontend :public Frontend {
public:

	WslFrontend(const std::string& backendFilename, const std::string& wslShell, const std::string& workingDirWsl, int rows, int cols, bool useWslExe);
	~WslFrontend() override;

public:

	virtual size_t tryRead(void* data, size_t maxlen) override;
	virtual size_t getBufferedSize() override;

	virtual bool write(const void* data, size_t len) override;

public:

	virtual void resizeTerminal(int rows, int cols) override;

private:

	void processPacket(sf::Packet& p);

	std::deque<char> bufRead;
	std::mutex bufReadLock;
	std::thread* thReader = 0;

	std::thread* processRunningChecker = 0;
	HANDLE childProcessHandle = 0;

	sf::TcpSocket* socket = 0;

};


#endif // SFML_SYSTEM_WINDOWS
