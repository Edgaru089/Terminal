
#include "Terminal.hpp"
#include "vterm/vterm.h"
#include "vterm/vterm_keycodes.h"

#include <cmath>

using namespace std;
using namespace sf;


namespace {
#ifndef SFML_SYSTEM_MACOS
	VTermModifier getModifier() {
		int mod = VTERM_MOD_NONE;
		if (Keyboard::isKeyPressed(Keyboard::LShift) || Keyboard::isKeyPressed(Keyboard::RShift))
			mod |= VTERM_MOD_SHIFT;
		if (Keyboard::isKeyPressed(Keyboard::LControl) || Keyboard::isKeyPressed(Keyboard::RControl))
			mod |= VTERM_MOD_CTRL;
		if (Keyboard::isKeyPressed(Keyboard::LAlt) || Keyboard::isKeyPressed(Keyboard::RAlt))
			mod |= VTERM_MOD_ALT;
		return (VTermModifier)mod;
	}
#endif

	Vector2i getMouseCell(int x, int y, Vector2i cellSize) {
		return Vector2i(x / cellSize.x, y / cellSize.y);
	}
}

#ifdef SFML_SYSTEM_MACOS
VTermModifier Terminal::getModifier() {
	int mod = VTERM_MOD_NONE;
	if (leftShiftDown || rightShiftDown)
		mod |= VTERM_MOD_SHIFT;
	if (leftCtrlDown || rightCtrlDown)
		mod |= VTERM_MOD_CTRL;
	if (leftAltDown || rightAltDown)
		mod |= VTERM_MOD_ALT;
	return (VTermModifier)mod;
}
#endif

void Terminal::processEvent(RenderWindow& win, Event e) {

	switch (e.type) {
	case Event::TextEntered:
	{
		VTermModifier mod = getModifier();
		if (e.text.unicode <= 31 && mod & VTERM_MOD_CTRL) {
			// Control characters produced by <CTRL-x> keystrokes
			switch (e.text.unicode) {
				// These keys have speical meaning:
			case 0x08: //  BS (Ctrl-H) Backspace
			case 0x09: //  HT (Ctrl-I) Tab
			case 0x0A: //  LF (Ctrl-J) Line feed
			case 0x0D: //  CR (Ctrl-M) Return
			case 0x1B: // ESC (Ctrl-[) Escape
				// Let's cancel out the Ctrl modifier
				mod = VTermModifier(mod & (~VTERM_MOD_CTRL));
				break;
			default:
				e.text.unicode += 'A' - 1;
				if (!(mod & VTERM_MOD_SHIFT) && isalpha((int)e.text.unicode))
					e.text.unicode += 'a' - 'A';
			}
		}

		if (e.text.unicode == 127) {
			// Under X11, the Delete key would trigger a 0x7f text event
			// Let's ignore it
			break;
		}

		if (e.text.unicode == 'V' && (mod & VTERM_MOD_CTRL)) {
			// Paste
			fprintf(stderr, "Paste\n");
			vterm_keyboard_start_paste(term);
			String s = Clipboard::getString();
			for (Uint32 c : s) {
				if (c == '\r')
					continue;
				vterm_keyboard_unichar(term, c, VTermModifier(0));
			}
			vterm_keyboard_end_paste(term);
			break;
		}

		// Replace \n with \r
		if (e.text.unicode == '\n')
			e.text.unicode = '\r';

		fprintf(stderr, "Main: Event::TextEntered, keycode=%d(%c), Ctrl:%s, Shift:%s, Alt:%s\n", (int)e.text.unicode, (char)e.text.unicode,
			(mod & VTERM_MOD_CTRL) ? "Yes" : "No", (mod & VTERM_MOD_SHIFT) ? "Yes" : "No", (mod & VTERM_MOD_ALT) ? "Yes" : "No");
		vterm_keyboard_unichar(term, e.text.unicode, mod);

		// Let's also reset the scrollback position
		if (!altScreen) {
			scrollbackOffset = 0;
			invalidate();
		}

		break;
	}
	case Event::MouseMoved:
	{
		if (IntRect(Vector2i(0, 0), Vector2i(win.getSize())).contains(e.mouseMove.x, e.mouseMove.y) && lastMouseCell != getMouseCell(e.mouseMove.x, e.mouseMove.y, cellSize)) {
			lastMouseCell = getMouseCell(e.mouseMove.x, e.mouseMove.y, cellSize);
			vterm_mouse_move(term, lastMouseCell.y, lastMouseCell.x, getModifier());
		}
		break;
	}
	case Event::MouseButtonPressed:
	case Event::MouseButtonReleased:
	{
		if (mouseState != VTERM_PROP_MOUSE_NONE) {
			int b = 0;
			if (e.mouseButton.button == Mouse::Left)
				b = 1; // Left
			else if (e.mouseButton.button == Mouse::Middle)
				b = 2; // Middle
			else if (e.mouseButton.button == Mouse::Right)
				b = 3; // Right;
			vterm_mouse_button(term, b, e.type == Event::MouseButtonPressed, getModifier());
		}
		break;
	}
#ifdef SFML_SYSTEM_MACOS
	case Event::MouseWheelScrolled:
	{
		if (fabs(e.mouseWheelScroll.delta) < 1e-5)
			break;
		bool up = e.mouseWheelScroll.delta > 0;
#else
	case Event::MouseWheelMoved:
	{
		bool up = e.mouseWheel.delta > 0;
#endif
		if (mouseState != VTERM_PROP_MOUSE_NONE) {
			vterm_mouse_button(term, (up ? 4 : 5), true, getModifier());
			vterm_mouse_button(term, (up ? 4 : 5), false, getModifier());
		} else if (altScreen) {
			// Let's press the UP/DOWN key 3 times
			VTermModifier mod = getModifier();
			if (up) {
				vterm_keyboard_key(term, VTERM_KEY_UP, mod);
				vterm_keyboard_key(term, VTERM_KEY_UP, mod);
				vterm_keyboard_key(term, VTERM_KEY_UP, mod);
			} else {
				vterm_keyboard_key(term, VTERM_KEY_DOWN, mod);
				vterm_keyboard_key(term, VTERM_KEY_DOWN, mod);
				vterm_keyboard_key(term, VTERM_KEY_DOWN, mod);
			}
		} else {
			// Let's scroll back/forth
			if (scrollbackOffset < 0)
				scrollbackOffset = -scrollbackOffset;
			if (up) {
				scrollbackOffset = min(scrollbackOffset + 3, (int)scrollback.size());
			} else {
				scrollbackOffset = max(scrollbackOffset - 3, 0);
			}
			invalidate();
		}
		break;
	}
	case Event::KeyPressed:
	{
		VTermModifier mod = getModifier();
		VTermKey key = VTERM_KEY_NONE;
		switch (e.key.code) {
			//case Keyboard::Enter:
			//	key = VTERM_KEY_ENTER; break;
			//case Keyboard::BackSpace:
			//	key = VTERM_KEY_BACKSPACE; break;
			//case Keyboard::Tab:
			//	key = VTERM_KEY_TAB; break;
		case Keyboard::Escape:
			key = VTERM_KEY_ESCAPE; break;

		case Keyboard::Up:
			key = VTERM_KEY_UP; break;
		case Keyboard::Down:
			key = VTERM_KEY_DOWN; break;
		case Keyboard::Left:
			key = VTERM_KEY_LEFT; break;
		case Keyboard::Right:
			key = VTERM_KEY_RIGHT; break;

		case Keyboard::Insert:
			key = VTERM_KEY_INS; break;
		case Keyboard::Home:
			key = VTERM_KEY_HOME; break;
		case Keyboard::End:
			key = VTERM_KEY_END; break;
		case Keyboard::PageUp:
			key = VTERM_KEY_PAGEUP; break;
		case Keyboard::PageDown:
			key = VTERM_KEY_PAGEDOWN; break;
		case Keyboard::Delete:
			key = VTERM_KEY_DEL; break;
#ifdef SFML_SYSTEM_LINUX
		case Keyboard::Space:
			// Ctrl-Space does not fire a NUL text input under X, we have to add it
			if (mod & VTERM_MOD_CTRL)
				vterm_keyboard_unichar(term, ' ', mod);
			break;
#endif
		}
#ifdef SFML_SYSTEM_MACOS
		// On macOS, most CTRL-X keys does not count as TextInput events
		// We have to fill them in ourselves

		// Minor hack to get modifier keys states without using Keyboard::isKeyPressed
		if (e.key.code == Keyboard::LControl)
			leftCtrlDown = true;
		if (e.key.code == Keyboard::RControl)
			rightCtrlDown = true;
		if (e.key.code == Keyboard::LShift)
			leftShiftDown = true;
		if (e.key.code == Keyboard::RShift)
			rightShiftDown = true;
		if (e.key.code == Keyboard::LAlt)
			leftAltDown = true;
		if (e.key.code == Keyboard::RAlt)
			rightAltDown = true;

		if (e.key.code >= Keyboard::A && e.key.code <= Keyboard::Z || e.key.code == Keyboard::LBracket || e.key.code == Keyboard::RBracket) {
			//if (Keyboard::isKeyPressed(Keyboard::LControl) || Keyboard::isKeyPressed(Keyboard::RControl)) {
			if (leftCtrlDown || rightCtrlDown) {
				VTermModifier mod = getModifier();
				int code;
				if (e.key.code >= Keyboard::A && e.key.code <= Keyboard::Z)
					code = e.key.code - Keyboard::A + ((mod & VTERM_MOD_SHIFT) ? 'A' : 'a');
				else {
					switch (e.key.code) {
					case Keyboard::LBracket:
						code = '['; break;
					case Keyboard::RBracket:
						code = ']'; break;
					}
				}

				// Let's not forget pasting
				if (code == 'V') {
					fprintf(stderr, "Paste (macOS)\n");
					vterm_keyboard_start_paste(term);
					String s = Clipboard::getString();
					for (Uint32 c : s) {
						if (c == '\r')
							continue;
						vterm_keyboard_unichar(term, c, VTermModifier(0));
					}
					vterm_keyboard_end_paste(term);
				} else {
					fprintf(stderr, "Main: TextEnter(macOS), keycode=%d(%c), Ctrl:%s, Shift:%s, Alt:%s\n", (int)code, (char)code,
						(mod & VTERM_MOD_CTRL) ? "Yes" : "No", (mod & VTERM_MOD_SHIFT) ? "Yes" : "No", (mod & VTERM_MOD_ALT) ? "Yes" : "No");
					vterm_keyboard_unichar(term, code, mod);
				}
			}
		}
#endif
		if (e.key.code >= Keyboard::F1 && e.key.code <= Keyboard::F15)
			key = (VTermKey)(VTERM_KEY_FUNCTION((int)e.key.code - Keyboard::F1 + 1));
		if (key != VTERM_KEY_NONE)
			vterm_keyboard_key(term, key, mod);

		// Let's also reset the scrollback position
		if (!altScreen) {
			scrollbackOffset = 0;
			invalidate();
		}

		break;
	}
#ifdef SFML_SYSTEM_MACOS
	case Event::KeyReleased:
	{
		// Minor hack to get modifier keys states without using Keyboard::isKeyPressed (Part II)
		if (e.key.code == Keyboard::LControl)
			leftCtrlDown = false;
		if (e.key.code == Keyboard::RControl)
			rightCtrlDown = false;
		if (e.key.code == Keyboard::LShift)
			leftShiftDown = false;
		if (e.key.code == Keyboard::RShift)
			rightShiftDown = false;
		if (e.key.code == Keyboard::LAlt)
			leftAltDown = false;
		if (e.key.code == Keyboard::RAlt)
			rightAltDown = false;
		break;
	}
#endif
	case Event::Resized:
	{
		if (e.size.width / cellSize.x != cols || e.size.height / cellSize.y != rows) {
			cols = e.size.width / cellSize.x;
			rows = e.size.height / cellSize.y;
			vterm_set_size(term, rows, cols);

			frontend->resizeTerminal(rows, cols);

			// Note that the scrollback buffer might have resized
			scrollbackOffset = min(scrollbackOffset, (int)scrollback.size());

			invalidate();
		}
		break;
	}
#ifndef SFML_SYSTEM_WINDOWS
	case Event::GainedFocus:
	{
		invalidate();
		break;
	}
#endif
	}
}


void Terminal::update() {

	char buffer[4096];
	size_t readlen;

	while (readlen = frontend->tryRead(buffer, sizeof(buffer))) {
		vterm_input_write(term, buffer, readlen);
	}

	if (!frontend->isRunning()) {
		fprintf(stderr, "Terminal::update(): frontend session ended\n");
		running = false;
	}
}

