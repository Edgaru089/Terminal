

#include <cstdlib>
#include <cstring>
#include <string>
#include <cmath>
#include <sstream>
#include <fstream>

#include <thread>
#include <atomic>
#include <mutex>

#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

#ifdef SFML_SYSTEM_WINDOWS
#define NOMINMAX
#include <Windows.h>
#include <signal.h>
#else
#include <pty.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <signal.h>
#endif

using namespace std;
using namespace sf;

#include "vterm/vterm.h"
#include "vterm/vterm_keycodes.h"

#include "OptionFile.hpp"
#include "Terminal.hpp"
#include "SystemFrontend.hpp"

OptionFile option;
Terminal* term;

RenderWindow* win;


int main(int argc, char* argv[]) {

	option.loadFromFile("Terminal.ini");
	int rows, cols;
	Vector2i cellSize = Vector2i(atoi(option.getContent("cell_width").c_str()), atoi(option.getContent("cell_height").c_str()));
	int charSize = atoi(option.getContent("fontsize").c_str());
	bool useBold = option.getContent("use_bold") == "true";
	VideoMode mode;
	bool fullscreen = (argc > 1 && strcmp(argv[1], "--fullscreen") == 0);
	if (fullscreen) {
		mode = VideoMode::getDesktopMode();
		cols = mode.width / cellSize.x;
		rows = mode.height / cellSize.y;
		const string& rc = option.getContent("run_on_startup");
		if (!rc.empty())
			system(rc.c_str());
	} else {
		rows = atoi(option.getContent("rows").c_str());
		cols = atoi(option.getContent("cols").c_str());
		mode = VideoMode(cols * cellSize.x, rows * cellSize.y);
	}


	Font font;
#ifdef SFML_SYSTEM_WINDOWS
	//font.loadFromFile("C:\\Windows\\Fonts\\ConsolasDengXianSemiBold.ttf");
	font.loadFromFile("C:\\Windows\\Fonts\\" + option.getContent("font"));
#else
	//font.loadFromFile("/mnt/c/Windows/Fonts/ConsolasDengXianSemiBold.ttf");
	if (!font.loadFromFile("/usr/share/fonts/" + option.getContent("font")))
		if (!font.loadFromFile("/mnt/Windows/Windows/Fonts/ConsolasDengXianSemiBold.ttf"))
			if (!font.loadFromFile("/mnt/c/Windows/Fonts/ConsolasDengXianSemiBold.ttf"))
				font.loadFromFile("/usr/share/fonts/truetype/unifont/unifont.ttf");
#endif

	if (fullscreen)
		win = new RenderWindow(mode, "Terminal", Style::None);
	else
		win = new RenderWindow(mode, "Terminal", Style::Titlebar | Style::Close | Style::Resize);
	win->clear();
	win->display();

	if (fullscreen) {
		win->setPosition(Vector2i(0, 0));
		win->setMouseCursorVisible(false);
	}
	win->setKeyRepeatEnabled(true);
	win->setVerticalSyncEnabled(true);
	win->setFramerateLimit(60);

	VertexArray arrtop;
	arrtop.setPrimitiveType(PrimitiveType::Triangles);
	vector<Vertex> arr;

	term = new Terminal(new SystemFrontend(option.getContent("shell"), cols, rows), cols, rows, cellSize, charSize, useBold);
	term->cbSetWindowSize = [&](int width, int height) {
		win->setSize(Vector2u(width, height));
		win->setView(View(FloatRect(0, 0, width, height)));
	};
	term->cbSetWindowTitle = [&](const string& title) {
		win->setTitle(String::fromUtf8(title.begin(), title.end()));
	};

	while (win->isOpen()) {

		Event e;
		while (win->pollEvent(e)) {
			if (e.type == Event::Closed)
				win->close();
			else if (e.type == Event::Resized)
				win->setView(View(FloatRect(0, 0, e.size.width, e.size.height)));
			term->processEvent(*win, e);
		}

		sleep(microseconds(1000));
		term->update();

		if (term->redrawIfRequired(font, arr) || fullscreen) {
			win->clear();

			win->draw(arr.data(), arr.size(), PrimitiveType::Triangles, &font.getTexture(charSize));

			if (fullscreen) {
				arrtop.clear();
				const Vector2f pos(Mouse::getPosition(*win));
				const float sqrt2 = 1.414213562f;
				const Vector2f off1(0, 16), off2(8 * sqrt2, 8 * sqrt2);
				arrtop.append(Vertex(pos, Color::White));
				arrtop.append(Vertex(pos + off1, Color::White));
				arrtop.append(Vertex(pos + Vector2f(8, 8), Color::White));
				arrtop.append(Vertex(pos, Color::White));
				arrtop.append(Vertex(pos + off2, Color::White));
				arrtop.append(Vertex(pos + Vector2f(0, 8 * sqrt2), Color::White));
				win->draw(arrtop);
			}

			win->display();

		} else {
			sleep(microseconds(15667));
		}

		if (!term->isRunning())
			win->close();
	}

	delete win;
	delete term;

	return 0;
}

