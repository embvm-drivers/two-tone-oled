// Copyright 2020 Embedded Artistry LLC
// SPDX-License-Identifier: MIT

#include "ssd1306.hpp"
#include "font/font5x7.h"
#include "font/font8x16.h"
#include <algorithm>
#include <bits/bits.hpp>
#include <gsl/gsl>

using namespace embdrv;

/**
 * NOTE: the SSD1306 DOES NOT WORK WITH A REPEATED START
 * CONDITION There must be no start between the data byte and the data stream
 */

const std::array<const uint8_t*, 2> ssd1306::fonts_ = {font5x7, font8x16};

namespace
{
constexpr uint8_t font_count_ = UINT8_C(2);
constexpr uint8_t FONT_HEADER_SIZE = UINT8_C(6);

uint8_t i2c_data_byte_ = UINT8_C(0x40);
constexpr uint8_t I2C_COMMAND_REG = UINT8_C(0x0);

constexpr uint8_t LCD_PAGE_HEIGHT = UINT8_C(8);
constexpr uint8_t BITS_PER_ROW = UINT8_C(8);
constexpr uint8_t SET_CONTRAST = UINT8_C(0x81);
constexpr uint8_t DISPLAY_ALL_ON_RESUME = UINT8_C(0xA4);
constexpr uint8_t NORMAL_DISPLAY = UINT8_C(0xA6);
constexpr uint8_t INVERT_DISPLAY = UINT8_C(0xA7);
constexpr uint8_t DISPLAY_OFF = UINT8_C(0xAE);
constexpr uint8_t DISPLAY_ON = UINT8_C(0xAF);
constexpr uint8_t SET_DISPLAY_OFFSET = UINT8_C(0xD3);
constexpr uint8_t SET_COMP_INS = UINT8_C(0xDA);
constexpr uint8_t SET_VCOM_DESELECT = UINT8_C(0xDB);
constexpr uint8_t SET_DISPLAY_CLOCK_DIV = UINT8_C(0xD5);
constexpr uint8_t SET_PRECHARGE = UINT8_C(0xD9);
__attribute__((unused)) constexpr uint8_t DISPLAY_ALL_ON = UINT8_C(0xA5);
constexpr uint8_t SET_MULTIPLEX = UINT8_C(0xA8);

constexpr uint8_t SET_START_LINE = UINT8_C(0x40);

constexpr uint8_t COM_SCAN_INC = UINT8_C(0xC0);
constexpr uint8_t COM_SCAN_DEC = UINT8_C(0xC8);
constexpr uint8_t SEG_REMAP = UINT8_C(0xA0);
constexpr uint8_t CHARGE_PUMP = UINT8_C(0x8D);

// Addressing of data bytes
constexpr uint8_t SET_ADDRESSING_MODE = UINT8_C(0x20);
__attribute__((unused)) constexpr uint8_t PAGE_ADDRESSING_MODE = UINT8_C(0x2);
constexpr uint8_t HORIZONTAL_ADDRESSING_MODE = UINT8_C(0x0);
__attribute__((unused)) constexpr uint8_t VERTICAL_ADDRESSING_MODE = UINT8_C(0x1);
constexpr uint8_t SET_COLUMN_ADDRESS = UINT8_C(0x21);
constexpr uint8_t SET_PAGE_ADDRESS = UINT8_C(0x22);

// Scroll
constexpr uint8_t ACTIVATE_SCROLL = UINT8_C(0x2F);
constexpr uint8_t DEACTIVATE_SCROLL = UINT8_C(0x2E);
constexpr uint8_t RIGHT_HORIZONTAL_SCROLL = UINT8_C(0x26);
__attribute__((unused)) constexpr uint8_t SET_VERTICAL_SCROLL_AREA = UINT8_C(0xA3);
__attribute__((unused)) constexpr uint8_t LEFT_HORIZONTAL_SCROLL = UINT8_C(0x27);
__attribute__((unused)) constexpr uint8_t VERTICAL_RIGHT_HORIZONTAL_SCROLL = UINT8_C(0x29);
__attribute__((unused)) constexpr uint8_t VERTICAL_LEFTHORIZONTALSCROLL = UINT8_C(0x2A);

/* Unused Definitions
#define WIDGETSTYLE0 0
#define WIDGETSTYLE1 1
#define WIDGETSTYLE2 2
#define SETLOWCOLUMN 0x00
#define SETHIGHCOLUMN 0x10
#define MEMORYMODE 0x20
#define EXTERNALVCC 0x01
#define SWITCHCAPVCC 0x02
*/

} // namespace

void ssd1306::start_() noexcept
{
	// default 5x7 font
	fontType(0);
	drawColor(color::white);
	drawMode(mode::normal);
	cursor(0, 0);

	/**
	 * Display init sequence
	 * These values were inherited from Sparkfun's example
	 */
	command(DISPLAY_OFF);

	// the suggested ratio 0x80
	command(SET_DISPLAY_CLOCK_DIV, 0x80); // NOLINT

	command(SET_MULTIPLEX, 0x2F); // NOLINT

	// No offset
	command(SET_DISPLAY_OFFSET, 0x0);

	// line # 0
	command(SET_START_LINE | 0x0); // line #0

	// Enable the charge pump
	command(CHARGE_PUMP, 0x14); // NOLINT

	command(NORMAL_DISPLAY);
	command(DISPLAY_ALL_ON_RESUME);

	command(SEG_REMAP | 0x1);
	command(COM_SCAN_DEC);

	command(SET_COMP_INS, 0x12); // NOLINT

	command(SET_CONTRAST, 0x08F); // NOLINT

	command(SET_PRECHARGE, 0xF1); // NOLINT

	command(SET_VCOM_DESELECT, 0x40); // NOLINT

	command(SET_ADDRESSING_MODE);
	command(HORIZONTAL_ADDRESSING_MODE);

	// Set the column limits for horizontal data mode
	command(SET_COLUMN_ADDRESS, COLUMN_OFFSET, COLUMN_OFFSET + SCREEN_WIDTH - 1);

	// Limit the pages for a 64 x 48 display
	command(SET_PAGE_ADDRESS, 0, 5); // NOLINT

	clear();
	display();
	command(DISPLAY_ON);
}

void ssd1306::stop_() noexcept
{
	command(DISPLAY_OFF);
}

void ssd1306::i2c_write(const uint8_t* buffer, uint8_t size,
						const embvm::i2c::master::cb_t& cb) noexcept
{
	embvm::i2c::op_t t;
	t.op = embvm::i2c::operation::write;
	t.address = i2c_addr_;
	t.tx_size = size;
	t.tx_buffer = buffer;

	i2c_.transfer(t, cb);
}

void ssd1306::data(uint8_t c) noexcept
{
	auto* tx_buf = reinterpret_cast<uint8_t*>(i2c_pool_.create<uint16_t>());
	assert(tx_buf);

	tx_buf[0] = i2c_data_byte_;
	tx_buf[1] = c;

	i2c_write(tx_buf, sizeof(uint16_t), [&](auto op, auto status) {
		(void)status;
		// Static cast silences a GCC warning due to ptr cast changing alignment
		// But our alignment is fine - we're adjusting our types to work with APIs, not actually
		// changing alignment
		i2c_pool_.destroy<uint16_t>(
			reinterpret_cast<const uint16_t*>(static_cast<const void*>(op.tx_buffer)));
	});
}

void ssd1306::command(uint8_t cmd) noexcept
{
	auto* tx_buf = reinterpret_cast<uint8_t*>(i2c_pool_.create<uint16_t>());
	assert(tx_buf);

	tx_buf[0] = I2C_COMMAND_REG;
	tx_buf[1] = cmd;

	i2c_write(tx_buf, sizeof(uint16_t), [&](auto op, auto status) {
		(void)status;
		// Static cast silences a GCC warning due to ptr cast changing alignment
		// But our alignment is fine - we're adjusting our types to work with APIs, not actually
		// changing alignment
		i2c_pool_.destroy<uint16_t>(
			reinterpret_cast<const uint16_t*>(static_cast<const void*>(op.tx_buffer)));
	});
}

void ssd1306::command(uint8_t cmd, uint8_t arg1) noexcept
{
	auto* tx_buf = reinterpret_cast<uint8_t*>(i2c_pool_.create<uint32_t>());
	assert(tx_buf);

	tx_buf[0] = I2C_COMMAND_REG;
	tx_buf[1] = cmd;
	tx_buf[2] = arg1;

	i2c_write(tx_buf, 3, [&](auto op, auto status) {
		(void)status;
		// Static cast silences a GCC warning due to ptr cast changing alignment
		// But our alignment is fine - we're adjusting our types to work with APIs, not actually
		// changing alignment
		i2c_pool_.destroy<uint32_t>(
			reinterpret_cast<const uint32_t*>(static_cast<const void*>(op.tx_buffer)));
	});
}

void ssd1306::command(uint8_t cmd, uint8_t arg1, uint8_t arg2) noexcept
{
	auto* tx_buf = reinterpret_cast<uint8_t*>(i2c_pool_.create<uint32_t>());
	assert(tx_buf);

	tx_buf[0] = I2C_COMMAND_REG;
	tx_buf[1] = cmd;
	tx_buf[2] = arg1;
	tx_buf[3] = arg2;

	i2c_write(tx_buf, sizeof(uint32_t), [&](auto op, auto status) {
		(void)status;
		// Static cast silences a GCC warning due to ptr cast changing alignment
		// But our alignment is fine - we're adjusting our types to work with APIs, not actually
		// changing alignment
		i2c_pool_.destroy<uint32_t>(
			reinterpret_cast<const uint32_t*>(static_cast<const void*>(op.tx_buffer)));
	});
}

void ssd1306::putchar(uint8_t c) noexcept
{
	if(c == '\n')
	{
		cursorY_ += fontHeight_;
		cursorX_ = 0;
	}
	else if(c != '\r')
	{
		drawChar(cursorX_, cursorY_, c, color_, mode_);
		cursorX_ += fontWidth_ + 1;
		if((cursorX_ > (SCREEN_WIDTH - fontWidth_)))
		{
			cursorY_ += fontHeight_;
			cursorX_ = 0;
		}
	}
}

void ssd1306::clearAndDisplay() noexcept
{
	clear(0);
	display();
}

void ssd1306::clear() noexcept
{
	clear(0);
}

void ssd1306::clear(uint8_t c) noexcept
{
	memset(screen_buffer_, c, SCREEN_BUFFER_SIZE); // (64 x 48) / 8 = 384
}

void ssd1306::invert(enum invert inv) noexcept
{
	command(inv == invert::normal ? NORMAL_DISPLAY : INVERT_DISPLAY);
}

void ssd1306::contrast(uint8_t contrast) noexcept
{
	command(SET_CONTRAST); // 0x81
	command(contrast);
}

void ssd1306::cursor(coord_t x, coord_t y) noexcept
{
	cursorX_ = x;
	cursorY_ = y;
}

void ssd1306::pixel(coord_t x, coord_t y, color c, mode m) noexcept
{
	if((x >= SCREEN_WIDTH) || (y >= SCREEN_HEIGHT))
	{
		return;
	}

	if(m == mode::XOR && c == color::white)
	{
		screen_buffer_[x + (y / BITS_PER_ROW) * SCREEN_WIDTH] ^= SET_BIT((y % BITS_PER_ROW));
	}
	else
	{
		if(c == color::white)
		{
			screen_buffer_[x + (y / BITS_PER_ROW) * SCREEN_WIDTH] |= SET_BIT((y % BITS_PER_ROW));
		}
		else
		{
			screen_buffer_[x + (y / BITS_PER_ROW) * SCREEN_WIDTH] &= ~SET_BIT((y % BITS_PER_ROW));
		}
	}
}

// TODO: cleanup
void ssd1306::line(coord_t x0, coord_t y0, coord_t x1, coord_t y1, color c, mode m) noexcept
{
	auto steep = static_cast<uint8_t>(abs(y1 - y0) > abs(x1 - x0));
	if(steep != 0)
	{
		std::swap(x0, y0);
		std::swap(x1, y1);
	}

	if(x0 > x1)
	{
		std::swap(x0, x1);
		std::swap(y0, y1);
	}

	uint8_t dx = 0;
	uint8_t dy = 0;
	dx = x1 - x0;
	dy = static_cast<uint8_t>(abs(y1 - y0));

	auto err = static_cast<int8_t>(dx >> 1);
	int8_t ystep = 0;

	if(y0 < y1)
	{
		ystep = 1;
	}
	else
	{
		ystep = -1;
	}

	for(; x0 < x1; x0++)
	{
		if(steep == 0)
		{
			pixel(x0, y0, c, m);
		}
		else
		{
			pixel(y0, x0, c, m);
		}
		err -= dy;
		if(err < 0)
		{
			y0 += ystep;
			err += dx;
		}
	}
}

void ssd1306::rect(coord_t x, coord_t y, uint8_t width, uint8_t height, color c, mode m) noexcept
{
	uint8_t tempHeight = 0;

	lineH(x, y, width, c, m);
	lineH(x, y + height - 1, width, c, m);

	tempHeight = height - 2;

	// skip drawing vertical lines to avoid overlapping of pixel that will
	// affect XOR plot if no pixel in between horizontal lines
	if(tempHeight < 1)
		return;

	lineV(x, y + 1, tempHeight, c, m);
	lineV(x + width - 1, y + 1, tempHeight, c, m);
}

void ssd1306::rectFill(coord_t x, coord_t y, uint8_t width, uint8_t height, color c,
					   mode m) noexcept
{
	for(uint8_t i = x; i < x + width; i++)
	{
		lineV(i, y, height, c, m);
	}
}

void ssd1306::circle(coord_t x, coord_t y, uint8_t radius, color c, mode m) noexcept
{
	// TODO - find a way to check for no overlapping of pixels so that XOR draw mode will work
	// perfectly
	int8_t f = 1 - static_cast<int8_t>(radius);
	int8_t ddF_x = 1;
	int8_t ddF_y = -2 * radius;
	int8_t x1 = 0;
	auto y1 = static_cast<int8_t>(radius);

	pixel(x, static_cast<uint8_t>(y + radius), c, m);
	pixel(x, static_cast<uint8_t>(y - radius), c, m);
	pixel(static_cast<uint8_t>(x + radius), y, c, m);
	pixel(static_cast<uint8_t>(x - radius), y, c, m);

	while(x1 < y1)
	{
		if(f >= 0)
		{
			y1--;
			ddF_y += 2;
			f += ddF_y;
		}
		x1++;
		ddF_x += 2;
		f += ddF_x;

		pixel(static_cast<uint8_t>(x + x1), static_cast<uint8_t>(y + y1), c, m);
		pixel(static_cast<uint8_t>(x - x1), static_cast<uint8_t>(y + y1), c, m);
		pixel(static_cast<uint8_t>(x + x1), static_cast<uint8_t>(y - y1), c, m);
		pixel(static_cast<uint8_t>(x - x1), static_cast<uint8_t>(y - y1), c, m);

		pixel(static_cast<uint8_t>(x + y1), static_cast<uint8_t>(y + x1), c, m);
		pixel(static_cast<uint8_t>(x - y1), static_cast<uint8_t>(y + x1), c, m);
		pixel(static_cast<uint8_t>(x + y1), static_cast<uint8_t>(y - x1), c, m);
		pixel(static_cast<uint8_t>(x - y1), static_cast<uint8_t>(y - x1), c, m);
	}
}

// TODO: refactor so function inputs are x, y
void ssd1306::circleFill(coord_t x, coord_t y, uint8_t radius, color c, mode m) noexcept
{
	// TODO - - find a way to check for no overlapping of pixels so that XOR draw mode will work
	// perfectly
	int8_t f = 1 - static_cast<int8_t>(radius);
	int8_t ddF_x = 1;
	int8_t ddF_y = -2 * radius;
	int8_t x1 = 0;
	auto y1 = static_cast<int8_t>(radius);

	// Temporary disable fill circle for XOR mode.
	if(m == mode::XOR)
		return;

	for(uint8_t i = y - radius; i <= y + radius; i++)
	{
		pixel(x, i, c, m);
	}

	while(x1 < y1)
	{
		if(f >= 0)
		{
			y1--;
			ddF_y += 2;
			f += ddF_y;
		}
		x1++;
		ddF_x += 2;
		f += ddF_x;

		for(auto i = static_cast<uint8_t>(y - y1); i <= static_cast<uint8_t>(y + y1); i++)
		{
			pixel(static_cast<uint8_t>(x + x1), i, c, m);
			pixel(static_cast<uint8_t>(x - x1), i, c, m);
		}
		for(auto i = static_cast<uint8_t>(y - x1); i <= static_cast<uint8_t>(y + x1); i++)
		{
			pixel(static_cast<uint8_t>(x + y1), i, c, m);
			pixel(static_cast<uint8_t>(x - y1), i, c, m);
		}
	}
}

void ssd1306::drawCharSingleRow(coord_t x, coord_t y, uint8_t character, color c, mode m) noexcept
{
	for(uint8_t i = 0; i < fontWidth_ + 1; i++)
	{
		uint8_t temp = 0;

		// this is done in a weird way because for 5x7 font, there is no
		// margin, this code add a margin after col 5
		if(i == fontWidth_)
		{
			temp = 0;
		}
		else
		{
			temp = *(fonts_.at(fontType_) + FONT_HEADER_SIZE + (character * fontWidth_) + i);
		}

		for(uint8_t j = 0; j < LCD_PAGE_HEIGHT; j++)
		{
			auto check = temp & 0x1;
			if(check == 0)
			{
				auto set_color = c == color::white ? color::black : color::white;
				pixel(x + i, y + j, set_color, m);
			}
			else
			{
				pixel(x + i, y + j, c, m);
			}

			temp >>= 1;
		}
	}
}

void ssd1306::drawCharMultiRow(coord_t x, coord_t y, uint8_t character, color c, mode m) noexcept
{
	uint16_t charPerBitmapRow = 0;
	uint16_t charColPositionOnBitmap = 0;
	uint16_t charRowPositionOnBitmap = 0;
	uint16_t charBitmapStartPosition = 0;

	// NOLINTNEXTLINE
	uint8_t rowsToDraw = (fontHeight_ / BITS_PER_ROW); // TODO: account for modulo...

	// font height over 8 bit
	// take character "0" ASCII 48 as example
	charPerBitmapRow = fontMapWidth_ / fontWidth_; // 256/8 =32 char per row
	charColPositionOnBitmap = character % charPerBitmapRow; // =16
	charRowPositionOnBitmap = character / charPerBitmapRow; // =1
	charBitmapStartPosition =
		(charRowPositionOnBitmap * fontMapWidth_ * (fontHeight_ / BITS_PER_ROW)) +
		(charColPositionOnBitmap * fontWidth_);

	// each row on LCD is 8 bit height
	for(uint8_t row = 0; row < rowsToDraw; row++)
	{
		for(uint8_t i = 0; i < fontWidth_; i++)
		{
			uint8_t temp = *(fonts_.at(fontType_) + FONT_HEADER_SIZE +
							 (charBitmapStartPosition + i + (row * fontMapWidth_)));

			for(uint8_t j = 0; j < LCD_PAGE_HEIGHT; j++)
			{
				auto check = temp & 0x1;
				if(check == 0)
				{
					auto set_color = c == color::white ? color::black : color::white;
					pixel(x + i, y + j + (row * BITS_PER_ROW), set_color, m);
				}
				else
				{
					pixel(x + i, y + j + (row * BITS_PER_ROW), c, m);
				}
				temp >>= 1;
			}
		}
	}
}

// TODO - New routine to take font of any height, at the moment limited to font height in
// multiple of 8 pixels. Also fix 5x7
void ssd1306::drawChar(coord_t x, coord_t y, uint8_t character, color c, mode m) noexcept
{
	// Check that we have a bitmap for the required c
	assert((character >= fontStartChar_) && (character < fontStartChar_ + fontTotalChar_ - 1));

	uint8_t tempC = character - fontStartChar_;

	// each row (in datasheet is call page) is 8 bits high, 16 bit high character will have 2 rows
	// to be drawn
	if((fontHeight_ / BITS_PER_ROW) <= 1)
	{
		drawCharSingleRow(x, y, tempC, c, m);
	}
	else
	{
		drawCharMultiRow(x, y, tempC, c, m);
	}
}

void ssd1306::drawBitmap(uint8_t* bitmap) noexcept
{
	memcpy(screen_buffer_, bitmap, SCREEN_BUFFER_SIZE);
}

uint8_t ssd1306::screenWidth() const noexcept
{
	return SCREEN_WIDTH;
}

uint8_t ssd1306::screenHeight() const noexcept
{
	return SCREEN_HEIGHT;
}

// Refer to http://learn.microview.io/intro/general-overview-of-microview.html for explanation of
// the rows.
void ssd1306::scrollRight(coord_t start, coord_t stop) noexcept
{
	assert(stop < start);

	scrollStop(); // need to disable scrolling before starting to avoid memory corrupt

	command(RIGHT_HORIZONTAL_SCROLL);
	command(0x00);
	command(start);
	// NOLINTNEXTLINE
	command(0x7); // scroll speed frames , TODO
	command(stop);
	command(0x00);
	command(0xFF); // NOLINT
	command(ACTIVATE_SCROLL);
}

// TODO
void ssd1306::scrollLeft(coord_t start, coord_t stop) noexcept
{
	(void)start;
	(void)stop;
	assert(0 && "Not Implemented");
}

// TODO
void ssd1306::scrollVertRight(coord_t start, coord_t stop) noexcept
{
	(void)start;
	(void)stop;
	assert(0 && "Not Implemented");
}

// TODO
void ssd1306::scrollVertLeft(coord_t start, coord_t stop) noexcept
{
	(void)start;
	(void)stop;
	assert(0 && "Not Implemented");
}

void ssd1306::scrollStop() noexcept
{
	command(DEACTIVATE_SCROLL);
}

void ssd1306::flipVertical(bool flip) noexcept
{
	command(flip ? COM_SCAN_INC : COM_SCAN_DEC);
}

void ssd1306::flipHorizontal(bool flip) noexcept
{
	command(flip ? (SEG_REMAP | 0x0) : (SEG_REMAP | 0x1));
}

void ssd1306::display() noexcept
{
	embvm::i2c::op_t t;
	t.op = embvm::i2c::operation::write;
	t.address = i2c_addr_;
	t.tx_size = sizeof(display_buffer_);
	t.tx_buffer = display_buffer_;
	i2c_.transfer(t);
}

uint8_t ssd1306::fontType(uint8_t type) noexcept
{
	assert(type < font_count_);

	fontType_ = type;
	fontWidth_ = *(fonts_.at(type) + 0);
	fontHeight_ = *(fonts_.at(type) + 1);
	fontStartChar_ = *(fonts_.at(type) + 2);
	fontTotalChar_ = *(fonts_.at(type) + 3);
	fontMapWidth_ = *(fonts_.at(type) + 4) + *(fonts_.at(type) + 5); // NOLINT

	return type;
}

uint8_t ssd1306::totalFonts() noexcept
{
	return font_count_;
}
