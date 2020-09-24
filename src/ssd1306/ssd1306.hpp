#ifndef SSD1306_HPP_
#define SSD1306_HPP_

#include <driver/basic_display.hpp>
#include <driver/i2c.hpp>
#include <etl/variant_pool.h>

namespace embdrv
{
inline constexpr uint8_t DEFAULT_SSD1306_I2C_ADDR = 0x3C;

/** Driver for the SSD1306 Display Driver
 *
 * This implementation only supports I2C, though the part supports SPI and Parallel modes
 * To implement that support, please use a state pattern and adjust the i2cWrite function
 * to be a more generic write. This single function can be swapped out for the particular
 * data mode.
 *
 * The screen size is hard-coded to be 64 x 48 with an offset of 32. Adjustments
 * are needed to make the screen size specification generic.
 *
 * @ingroup FrameworkDrivers
 */
class ssd1306 final : public embvm::basicDisplay
{
  public:
	/// Address is 0x3D if DC pin is set to 1
	explicit ssd1306(embvm::i2c::master& i2c, uint8_t i2c_addr = DEFAULT_SSD1306_I2C_ADDR)
		: embvm::basicDisplay("ssd1306"), i2c_(i2c), i2c_addr_(i2c_addr)
	{
		// Initialize the display buffer with byte 0x40, indicating that it is a
		// Data payload. This is used to transfer the whole screen buffer in a
		// Single transaction.
		display_buffer_[0] = 0x40; // NOLINT
	}

	void clear() noexcept final;
	void clearAndDisplay() noexcept;
	void invert(enum invert inv) noexcept final;
	void contrast(uint8_t contrast) noexcept final;
	void cursor(coord_t x, coord_t y) noexcept final;
	void pixel(coord_t x, coord_t y, color c, mode m) noexcept final;
	void line(coord_t x0, coord_t y0, coord_t x1, coord_t y1, color c, mode m) noexcept final;
	void rect(coord_t x, coord_t y, uint8_t width, uint8_t height, color c, mode m) noexcept final;
	void rectFill(coord_t x, coord_t y, uint8_t width, uint8_t height, color c,
				  mode m) noexcept final;
	void circle(coord_t x, coord_t y, uint8_t radius, color c, mode m) noexcept final;
	void circleFill(coord_t x, coord_t y, uint8_t radius, color c, mode m) noexcept final;
	void drawChar(coord_t x, coord_t y, uint8_t character, color c, mode m) noexcept final;
	void drawBitmap(uint8_t* bitmap) noexcept final;
	uint8_t screenWidth() const noexcept final;
	uint8_t screenHeight() const noexcept final;

	void scrollRight(coord_t start, coord_t stop) noexcept final;
	void scrollLeft(coord_t start, coord_t stop) noexcept final;
	void scrollVertRight(coord_t start, coord_t stop) noexcept final;
	void scrollVertLeft(coord_t start, coord_t stop) noexcept final;
	void scrollStop() noexcept final;

	void flipVertical(bool flip) noexcept final;
	void flipHorizontal(bool flip) noexcept final;

	void display() noexcept final;

	// TODO: refactor font functions out of this driver

	/// Set the font type
	/// @param type Index for the font to use
	/// @returns the currently selected font.
	uint8_t fontType(uint8_t type) noexcept;

	/// Get the current font type
	/// @returns the currently selected font.
	uint8_t fontType() const noexcept
	{
		return fontType_;
	}

	/// Get the font width
	/// @returns the width of the currently selected font in pixels
	uint8_t fontWidth() const noexcept
	{
		return fontWidth_;
	}

	/// Get the font height
	/// @returns the height of the currently selected font in pixels
	uint8_t fontHeight() const noexcept
	{
		return fontHeight_;
	}

	/// Get the total number of supported fonts
	/// @returns the number of supported fonts
	static uint8_t totalFonts() noexcept;

	/// Get the starting character for the font
	/// @returns the starting ASCII character for the currently selected font
	uint8_t fontStartChar() const noexcept
	{
		return fontStartChar_;
	}

	/// Get the total number of characters supported by the font
	/// @returns the number of characters supported by the currently selected font
	uint8_t fontTotalChar() const noexcept
	{
		return fontTotalChar_;
	}

	void putchar(uint8_t c) noexcept final;

  private:
	void start_() noexcept final;
	void stop_() noexcept final;

	/// Helper function which performs an I2C write
	/// @param buffer The transaction buffer.
	/// @param size The size of the write.
	/// @param cb The callback function to invoke when the write completes.
	void i2c_write(const uint8_t* buffer, uint8_t size,
				   const embvm::i2c::master::cb_t& cb) noexcept;

	// RAW LCD functions

	/// Clear the display and initialize buffer bytes with the target value
	/// @param c The value to initialize the bytes of the screen buffe rwith.
	void clear(uint8_t c) noexcept;

	/// Send a command to the display driver hardware
	/// @param c the command byte.
	void command(uint8_t c) noexcept;

	/// Send a command with one argument to the display driver hardware
	/// @param cmd the command byte.
	/// @param arg1 The argument to send with the command byte.
	void command(uint8_t cmd, uint8_t arg1) noexcept;

	/// Send a command with two arguments to the display driver hardware
	/// @param cmd the command byte.
	/// @param arg1 The first argument to send with the command byte.
	/// @param arg2 The second argumet to send with the command byte.
	void command(uint8_t cmd, uint8_t arg1, uint8_t arg2) noexcept;

	/// Send a data byte to the display driver hardware
	/// @param c The data byte to send to the display.
	void data(uint8_t c) noexcept;

	/// Set the column address
	/// @param add The address of the column.
	void setColumnAddress(uint8_t add) noexcept;

	/// Set the page address
	/// @param add The address of the page.
	void setPageAddress(uint8_t add) noexcept;

	void drawCharSingleRow(coord_t x, coord_t y, uint8_t character, color c, mode m) noexcept;
	void drawCharMultiRow(coord_t x, coord_t y, uint8_t character, color c, mode m) noexcept;

	/// Deleted copy constructor - make GCC happy since we have pointers to data members
	ssd1306(const ssd1306&) = delete;

	/// Deleted copy assignment operator - make GCC happy since we have pointers to data members
	const ssd1306& operator=(const ssd1306&) = delete;

	/// Deleted move constructor - make GCC happy since we have pointers to data members
	ssd1306(ssd1306&&) = delete;

	/// Deleted move assignment operator - make GCC happy since we have pointers to data members
	ssd1306& operator=(ssd1306&&) = delete;

  private:
	/// The width of the scren in pixels
	static constexpr uint8_t SCREEN_WIDTH = 64;

	/// The height of the screen in pixels
	static constexpr uint8_t SCREEN_HEIGHT = 48;

	/// The size of the screen buffer
	/// We divide by 8 because each byte controls the state of 8 pixels.
	static constexpr size_t SCREEN_BUFFER_SIZE = ((SCREEN_WIDTH * SCREEN_HEIGHT) / 8);

	/// The number of columns offset into the display where the active display area starts.
	static constexpr uint8_t COLUMN_OFFSET = 32;

	uint8_t fontWidth_ = 0, fontHeight_ = 0, fontType_ = 0, fontStartChar_ = 0, fontTotalChar_ = 0;
	uint16_t fontMapWidth_ = 0;

	/// X-axies position of the cursor.
	uint8_t cursorX_ = 0;

	/// Y-axis position of the cursor.
	uint8_t cursorY_ = 0;

	/// The i2c instance this display driver is attached to
	embvm::i2c::master& i2c_;

	/// The I2C address for this dispaly
	uint8_t i2c_addr_;

	/// Array of fonts supported by this driver
	static const std::array<const uint8_t*, 2> fonts_;

	/// Static memory pool which is used for display I2C transactions.
	etl::variant_pool<128, uint8_t, uint16_t, uint32_t> i2c_pool_{};

	/** \brief OLED screen buffer.
	 * Page buffer is required because in SPI and I2C mode, the host cannot read the SSD1306's GDRAM
	 * of the controller.  This page buffer serves as a scratch RAM for graphical functions.  All
	 * drawing function will first be drawn on this page buffer, only upon calling display()
	 * function will transfer the page buffer to the actual LCD controller's memory.
	 *
	 * The additional byte is to represent the data word byte, which allows us to send the entire
	 * buffer in one shot
	 */
	uint8_t display_buffer_[SCREEN_BUFFER_SIZE + 1] = {0};

	/// Pointer alias to the display_buffer_ which accounts for the single byte reserved for the
	/// DATA command value.
	uint8_t* const screen_buffer_ = &display_buffer_[1];
};

} // namespace embdrv

#endif // SSD1306_HPP_
