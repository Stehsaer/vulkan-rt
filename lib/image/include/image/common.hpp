#pragma once

#include <cstddef>
#include <glm/glm.hpp>
#include <vector>

namespace image
{
	template <typename T>
	constexpr bool indexable_pixel_type_flag = true;

	template <typename T>
	concept IndexablePixel = indexable_pixel_type_flag<T>;

	template <typename T>
	struct Container
	{
		glm::u32vec2 size;
		std::vector<T> data;

		[[nodiscard]]
		auto& operator[](this auto& self, size_t x, size_t y)
			requires(IndexablePixel<T>)
		{
			assert(x < self.size.x && y < self.size.y);
			return self.data[y * self.size.x + x];
		}

		[[nodiscard]]
		auto& operator[](this auto& self, glm::u32vec2 coord)
			requires(IndexablePixel<T>)
		{
			assert(coord.x < self.size.x && coord.y < self.size.y);
			return self.data[coord.y * self.size.x + coord.x];
		}
	};
}