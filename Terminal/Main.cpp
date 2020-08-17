

#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

#ifdef SFML_SYSTEM_WINDOWS
#define NOMINMAX
#include <Windows.h>
#include <signal.h>
#else
#include <errno.h>
#include <pty.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#endif

using namespace std;
using namespace sf;

#include "OptionFile.hpp"
#include "SystemFrontend.hpp"
#include "Terminal.hpp"
#include "WslFrontend.hpp"
#include "vterm/vterm.h"
#include "vterm/vterm_keycodes.h"

OptionFile option;
Terminal* term;

RenderWindow* win;

Sprite coverAutoscale(Texture& texture, Vector2f asize) {
	Sprite sp(texture);
	Vector2f tsize(texture.getSize());
	if (tsize.x / tsize.y > asize.x / asize.y) {
		float scale = asize.y / tsize.y;
		sp.setScale(scale, scale);
		sp.setPosition(-(tsize.x * scale - asize.x) / 2.0f, 0);
	} else {
		float scale = asize.x / tsize.x;
		sp.setScale(scale, scale);
		sp.setPosition(0, -(tsize.y * scale - asize.y) / 2.0f);
	}
	return sp;
}

int main(int argc, char* argv[]) {
	if (argc < 2 || !option.loadFromFile(argv[1]))
		option.loadFromFile("Terminal.ini");

	int rows, cols;
	Vector2i cellSize = Vector2i(atoi(option.get("cell_width").c_str()),
		atoi(option.get("cell_height").c_str()));
	int charSize = atoi(option.get("fontsize").c_str());
	bool useBold = option.get("use_bold") == "true";
	VideoMode mode;
	bool fullscreen = (argc > 1 && strcmp(argv[1], "--fullscreen") == 0);
	if (fullscreen) {
		mode = VideoMode::getDesktopMode();
		cols = mode.width / cellSize.x;
		rows = mode.height / cellSize.y;
		const string& rc = option.get("run_on_startup");
		if (!rc.empty()) system(rc.c_str());
	} else {
		rows = atoi(option.get("rows").c_str());
		cols = atoi(option.get("cols").c_str());
		mode = VideoMode(cols * cellSize.x, rows * cellSize.y);
	}
	string bgFilename = option.get("background_image");
	Uint8 bgDarkness = atoi(option.get("bg_darkness").c_str());
	Texture bgTexture;
	Sprite bgSprite;
	bool useVbo = VertexBuffer::isAvailable();
	int scrollMaxLines = atoi(option.get("scrollback_max_lines").c_str());
	bool useWslExe = option.get("use_wsl_exe") == "true";

	if (!bgFilename.empty()) {
		if (!bgTexture.loadFromFile(bgFilename))
			bgFilename.clear();
		else {
			bgTexture.setSmooth(true);
		}
	}
	if (bgFilename.empty())
		bgDarkness = 255;
	else
		bgSprite = coverAutoscale(bgTexture, Vector2f(mode.width, mode.height));

	Font font;
#ifdef SFML_SYSTEM_WINDOWS
	// font.loadFromFile("C:\\Windows\\Fonts\\ConsolasDengXianSemiBold.ttf");
	font.loadFromFile("C:\\Windows\\Fonts\\" + option.get("font"));
#else
	// font.loadFromFile("/mnt/c/Windows/Fonts/ConsolasDengXianSemiBold.ttf");
	if (!font.loadFromFile("/usr/share/fonts/" + option.get("font")))
		if (!font.loadFromFile(
			"/mnt/Windows/Windows/Fonts/ConsolasDengXianSemiBold.ttf"))
			if (!font.loadFromFile(
				"/mnt/c/Windows/Fonts/ConsolasDengXianSemiBold.ttf"))
				font.loadFromFile("/usr/share/fonts/truetype/unifont/unifont.ttf");
#endif

	if (fullscreen)
		win = new RenderWindow(mode, "Terminal", Style::None);
	else
		win = new RenderWindow(mode, "Terminal",
			Style::Titlebar | Style::Close | Style::Resize);
	win->clear();
	win->display();

	if (fullscreen) {
		win->setPosition(Vector2i(0, 0));
		// win->setMouseCursorVisible(false);
	}
	win->setKeyRepeatEnabled(true);
	// win->setVerticalSyncEnabled(true);
	// win->setFramerateLimit(60);

	win->setActive(false);

#ifdef SFML_SYSTEM_WINDOWS
	bool useWsl = option.get("use_wsl_frontend") == "true";
	if (useWsl)
		term = new Terminal(
			new WslFrontend(option.get("wsl_backend_file"), option.get("shell"),
				option.get("wsl_working_dir"), rows, cols, useWslExe),
			rows, cols, cellSize, charSize, useBold, scrollMaxLines);
	else
		term =
		new Terminal(new SystemFrontend(option.get("shell"), rows, cols), rows,
			cols, cellSize, charSize, useBold, scrollMaxLines);
#else
	term = new Terminal(new SystemFrontend(option.get("shell"), rows, cols), rows,
		cols, cellSize, charSize, useBold, scrollMaxLines);
#endif
	term->cbSetWindowSize = [&](int width, int height) {
		// win->setSize(Vector2u(width, height));
		// win->setView(View(FloatRect(0, 0, width, height)));
	};
	term->cbSetWindowTitle = [&](const string& title) {
		// win->setTitle(String::fromUtf8(title.begin(), title.end()));
	};

	term->cbRedrawPre = [&]() {
		win->clear();

		if (bgDarkness != 255) win->draw(bgSprite);
	};

	term->cbRedrawPost = [&]() { win->display(); };

	term->lock();
	term->drawBgDarkness = bgDarkness;
	term->drawFont = &font;
	term->drawRenderTarget = win;
	term->drawUseVertexBufferObject = useVbo;
	term->unlock();

	sleep(milliseconds(20));

	term->lock();
	term->invalidate();
	term->unlock();

	Event e;
	while (win->waitEvent(e)) {
		term->lock();

		if (e.type == Event::Closed)
			win->close();
		else if (e.type == Event::Resized) {
			win->setView(View(FloatRect(0, 0, e.size.width, e.size.height)));

			if (e.size.width / cellSize.x != cols ||
				e.size.height / cellSize.y != rows) {
				cols = e.size.width / cellSize.x;
				rows = e.size.height / cellSize.y;
			}

			bgSprite =
				coverAutoscale(bgTexture, Vector2f(e.size.width, e.size.height));
		}
		term->processEvent(*win, e);
		term->flushVtermOutputBuffer();

		if (!term->isRunning()) win->close();

		term->unlock();
	}

	delete term;
	delete win;

	return 0;
}
