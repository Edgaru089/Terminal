
#include "Terminal.hpp"
#include "vterm/vterm.h"

#include <cmath>

using namespace std;
using namespace sf;


namespace {
	Color reverseOf(Color col) {
		return Color(255 - col.r, 255 - col.g, 255 - col.b, col.a);
	}

	void reverse(Color& col) {
		col.r = 255 - col.r;
		col.g = 255 - col.g;
		col.b = 255 - col.b;
	}

	inline void pushQuadVertex(vector<Vertex>& arr, const Vertex& a, const Vertex& b, const Vertex& c, const Vertex& d) {
		arr.push_back(a);
		arr.push_back(b);
		arr.push_back(c);
		arr.push_back(b);
		arr.push_back(c);
		arr.push_back(d);
	}

	inline void pushVertexColor(vector<Vertex>& arr, FloatRect rect, Color color) {
		pushQuadVertex(arr,
			Vertex(Vector2f(rect.left, rect.top), color, Vector2f(1, 1)),
			Vertex(Vector2f(rect.left + rect.width, rect.top), color, Vector2f(1, 1)),
			Vertex(Vector2f(rect.left, rect.top + rect.height), color, Vector2f(1, 1)),
			Vertex(Vector2f(rect.left + rect.width, rect.top + rect.height), color, Vector2f(1, 1)));
	}

	inline void pushVertexTextureColor(vector<Vertex>& arr, FloatRect rect, IntRect texRect, Color color) {
		pushQuadVertex(arr,
			Vertex(Vector2f(rect.left, rect.top), color, Vector2f(texRect.left, texRect.top)),
			Vertex(Vector2f(rect.left + rect.width, rect.top), color, Vector2f(texRect.left + texRect.width, texRect.top)),
			Vertex(Vector2f(rect.left, rect.top + rect.height), color, Vector2f(texRect.left, texRect.top + texRect.height)),
			Vertex(Vector2f(rect.left + rect.width, rect.top + rect.height), color, Vector2f(texRect.left + texRect.width, texRect.top + texRect.height)));
	}

	VertexBuffer* buf;
	vector<Vertex> arr;
}


void Terminal::redraw(Font& font, vector<Vertex>& target, Color bgColor) {

	if (charTopOffset == -4096) {
		// Guess the character centering offset
		// Let's use the pipe character '|' as a reference
		Glyph g = font.getGlyph('|', charSize, false, 0);
		charTopOffset = (cellSize.y - g.bounds.height) / 2.0f + (g.bounds.height + g.bounds.top);
	}

	target.clear();
	if (bgColor != Color::Black)
		pushVertexColor(target, FloatRect(0, 0, (cols + 1) * cellSize.x, (rows + 1) * cellSize.y), bgColor);

	VTermPos curpos;
	VTermScreen* scr = vterm_obtain_screen(term);
	vterm_state_get_cursorpos(vterm_obtain_state(term), &curpos);

	for (int i = 0; i < rows; i++)
		for (int j = 0; j < cols; j++) {

			VTermScreenCell cell;

			if (altScreen) {
				if (!vterm_screen_get_cell(scr, VTermPos{ i, j }, &cell))
					continue;
			} else {
				if (i < scrollbackOffset) {
					auto& line = scrollback[scrollback.size() - scrollbackOffset + i];
					if (j < line.size())
						cell = line[j];
					else
						continue;
				} else if (!vterm_screen_get_cell(scr, VTermPos{ i - scrollbackOffset, j }, &cell))
					continue;
			}

			vterm_state_convert_color_to_rgb(vterm_obtain_state(term), &cell.fg);
			vterm_state_convert_color_to_rgb(vterm_obtain_state(term), &cell.bg);

			FloatRect cellRect(j * cellSize.x, i * cellSize.y, cellSize.x * cell.width, cellSize.y);

			Color bg(cell.bg.rgb.red, cell.bg.rgb.green, cell.bg.rgb.blue);
			Color fg(cell.fg.rgb.red, cell.fg.rgb.green, cell.fg.rgb.blue);
			if (cell.attrs.reverse)
				swap(bg, fg);

			// Draw the cursor
			if (cursorVisible && curpos.row == i - scrollbackOffset && curpos.col == j) {
				float thick = font.getUnderlineThickness(charSize);
				switch (cursorShape) {
				case VTERM_PROP_CURSORSHAPE_BLOCK:
					reverse(bg);
					reverse(fg);
					break;
				case VTERM_PROP_CURSORSHAPE_BAR_LEFT:
					pushVertexColor(target, FloatRect(cellRect.left, cellRect.top, thick, cellRect.height), reverseOf(bg));
					break;
				case VTERM_PROP_CURSORSHAPE_UNDERLINE:
					pushVertexColor(target, FloatRect(cellRect.left, cellRect.top + cellRect.height - 1 - thick, cellRect.width, thick), reverseOf(bg));
					break;
				}
			}

			if (bg != Color::Black)
				pushVertexColor(target, cellRect, bg);
			if (cell.attrs.underline) {
				float thick = font.getUnderlineThickness(charSize);
				pushVertexColor(target, FloatRect(cellRect.left, cellRect.top + cellRect.height - 1 - thick, cellRect.width, thick), fg);
			}
			if (cell.attrs.strike) { // I'm too lazy to somehow get the strikeline position
				float thick = font.getUnderlineThickness(charSize);
				pushVertexColor(target, FloatRect(cellRect.left, roundf(cellRect.top + cellRect.height * 0.6f - thick / 2.0f), cellRect.width, thick), fg);
			}

			if (cell.chars[0] && cell.chars[0] != ' ') {
				Glyph glyph = font.getGlyph(cell.chars[0], charSize, hasBold && cell.attrs.bold, 0.0F);

				FloatRect renderRect;
				renderRect.left = roundf(j * cellSize.x + (cellSize.x * cell.width - glyph.advance) / 2.0f + glyph.bounds.left);
				renderRect.width = glyph.bounds.width;
				renderRect.top = roundf((i + 1) * cellSize.y + glyph.bounds.top - charTopOffset);
				renderRect.height = glyph.bounds.height;

				pushVertexTextureColor(target, renderRect, glyph.textureRect, fg);
			}

			j += cell.width - 1;
		}
}


void Terminal::thRedrawerFunction() {
	buf = new VertexBuffer(PrimitiveType::Triangles, VertexBuffer::Dynamic);

	unique_lock<mutex> lock(vtermLock);

	for (;;) {

		update();
		if (!running)
			break;

		if (cbRedrawPre)
			cbRedrawPre();

		redraw(*drawFont, arr, Color(0, 0, 0, drawBgDarkness));

		if (drawUseVertexBufferObject) {
			if (buf->getVertexCount() < arr.size())
				buf->create(max((size_t)(cols * rows * (size_t)4), arr.size()));
			buf->update(arr.data(), arr.size(), 0);
			drawRenderTarget->draw(*buf, 0, arr.size(), &drawFont->getTexture(charSize));
		} else
			drawRenderTarget->draw(arr.data(), arr.size(), PrimitiveType::Triangles, &drawFont->getTexture(charSize));

		if (cbRedrawPost)
			cbRedrawPost();

		redrawConditional.wait(lock);
	}
	lock.unlock();

	delete buf;
}

