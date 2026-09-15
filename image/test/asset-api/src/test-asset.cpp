#include "test-asset.hpp"

extern "C"
{
	extern const std::byte _binary_load8_png_start;
	extern const std::byte _binary_load8_png_end;

	extern const std::byte _binary_load8_jpg_start;
	extern const std::byte _binary_load8_jpg_end;

	extern const std::byte _binary_load16_png_start;
	extern const std::byte _binary_load16_png_end;

	extern const std::byte _binary_checker_png_start;
	extern const std::byte _binary_checker_png_end;

	extern const std::byte _binary_complex_jpg_start;
	extern const std::byte _binary_complex_jpg_end;
}

extern const std::span<const std::byte>
	load8_png_data = {&_binary_load8_png_start, &_binary_load8_png_end},
	load8_jpg_data = {&_binary_load8_jpg_start, &_binary_load8_jpg_end},
	load16_png_data = {&_binary_load16_png_start, &_binary_load16_png_end},
	checker_image_data = {&_binary_checker_png_start, &_binary_checker_png_end},
	complex_image_data = {&_binary_complex_jpg_start, &_binary_complex_jpg_end};
