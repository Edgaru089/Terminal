
#include "Terminal.hpp"
#include "vterm/vterm.h"
#include "vterm/vterm_internal.h"

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
			term->cursorVisible = val->boolean;
			term->invalidate();
			return 1;
		case VTERM_PROP_CURSORSHAPE:
			term->cursorShape = val->number;
			return 1;
		case VTERM_PROP_MOUSE:
			term->mouseState = val->number;
			return 1;
		case VTERM_PROP_ALTSCREEN:
			term->renderAltScreen = term->altScreen = (bool)val->boolean;
			return 1;
		default:
			return 1;
		}
		// Should never reach here
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
		if (term->scrollback.empty())
			return 0;
		else {
			auto& back = term->scrollback.back();
			memcpy(data, back.data(), min((size_t)col, back.size()) * sizeof(VTermScreenCell));
			// Fill the extra cells
			if (back.size() < col) {
				VTermColor bg, fg;
				vterm_state_get_default_colors(term->state, &fg, &bg);
				for (int i = back.size(); i < col; i++) {
					memset(data + i, 0, sizeof(VTermScreenCell));
					data[i].width = 1;
					data[i].fg = fg;
					data[i].bg = bg;
				}
			}
			term->scrollback.pop_back();
			return 1;
		}
	}

	static int termPushLine(int col, const VTermScreenCell* data, void* user) {
		Terminal* term = reinterpret_cast<Terminal*>(user);
		term->scrollback.emplace_back(data, data + col);
		while (term->scrollback.size() > term->scrollbackMaxLength)
			term->scrollback.pop_front();
		term->invalidate();
		return 1;
	}

	static void writeCall(const char* s, size_t len, void* data) {
		Terminal* term = reinterpret_cast<Terminal*>(data);
		term->frontend->write(s, len);
	}
};


Terminal::Terminal(
	Frontend* frontend,
	int rows, int cols,
	sf::Vector2i cellSize,
	int charSize, bool useBold, int scrollbackMaxLength)
	:frontend(frontend), cols(cols), rows(rows), cellSize(cellSize), charSize(charSize), hasBold(useBold),
	scrollbackMaxLength(scrollbackMaxLength), winTitle("Terminal"), charTopOffset(-4096), cursorVisible(true) {

	running = true;

	// Allocate VTerm object
	term = vterm_new(rows, cols);
	vterm_state_reset(state = vterm_obtain_state(term), 1);
	vterm_screen_reset(screen = vterm_obtain_screen(term), 1);
	vterm_screen_enable_altscreen(screen, 1);
	vterm_set_utf8(term, true);
	vterm_state_set_bold_highbright(state, true);

	// Setup callbacks
	VTermScreenCallbacks* screencb = new VTermScreenCallbacks;
	memset(screencb, 0, sizeof(VTermScreenCallbacks));
	screencb->settermprop = &TermCb::termProp;
	screencb->damage = &TermCb::termDamage;
	screencb->movecursor = &TermCb::termMoveCursor;
	screencb->sb_pushline = &TermCb::termPushLine;
	screencb->sb_popline = &TermCb::termPopLine;
	screencb->bell = &TermCb::termBell;
	vterm_screen_set_callbacks(screen, screencb, reinterpret_cast<void*>(this));
	this->screencb = screencb;

	VTermColor bg, fg;
	vterm_color_rgb(&bg, 0, 0, 0);
	vterm_color_rgb(&fg, 255, 255, 255);
	vterm_state_set_default_colors(state, &fg, &bg);

	vterm_output_set_callback(term, &TermCb::writeCall, reinterpret_cast<void*>(this));
	vterm_set_size(term, rows, cols);

	// Looks like there's no much to do about the frontend instance
}

Terminal::~Terminal() {
	running = false;

	delete frontend;

	vterm_free(term);
	if (screencb)
		delete screencb;
}


void Terminal::stop() {
	if (frontend && frontend->isRunning())
		frontend->stop();
	running = false;
}


void Terminal::invalidate() {
	needRedraw = true;
}

