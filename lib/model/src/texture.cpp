#include "model/texture.hpp"
#include "common/file.hpp"

#include <mio/mmap.hpp>
#include <variant>

namespace model
{
	namespace
	{
		struct VariableFormatVisitor
		{
			using ReturnType = std::expected<Texture::ImageVariant, Error>;

			static ReturnType decode(std::span<const std::byte> encoded_data) noexcept
			{
				if (encoded_data.empty()) return Error("Texture source is empty");

				if (image::encoded_data_is_16bit(encoded_data))
				{
					auto result =
						image::Image<image::Format::Unorm16, image::Layout::RGBA>::decode(encoded_data);
					if (!result) return result.error().forward("Failed to decode texture from encoded data");
					return std::move(*result);
				}
				else
				{
					auto result =
						image::Image<image::Format::Unorm8, image::Layout::RGBA>::decode(encoded_data);
					if (!result) return result.error().forward("Failed to decode texture from encoded data");
					return std::move(*result);
				}
			}

			ReturnType operator()(const std::filesystem::path& path) const noexcept
			{
				if (!std::filesystem::exists(path))
					return Error(std::format("Texture file '{}' does not exist", path.string()));

				try
				{
					auto mapped_file = mio::basic_mmap_source<std::byte>(path.string());
					return decode(std::span(mapped_file.begin(), mapped_file.size()));
				}
				catch (const std::system_error& e)
				{
					return Error("Mapping texture file failed", std::format("what(): {}", e.what()));
				}
			}

			ReturnType operator()(const std::vector<std::byte>& encoded_data) const noexcept
			{
				return decode(encoded_data);
			}

			ReturnType operator()(
				const image::Image<image::Format::Unorm8, image::Layout::RGBA>& raw_image
			) const noexcept
			{
				return raw_image;
			}

			ReturnType operator()(
				const image::Image<image::Format::Unorm16, image::Layout::RGBA>& raw_image
			) const noexcept
			{
				return raw_image;
			}
		};

		template <image::Format T>
		struct FixedFormatVisitor
		{
			static std::expected<image::Image<T, image::Layout::RGBA>, Error> decode(
				std::span<const std::byte> encoded_data
			) noexcept
			{
				if (encoded_data.empty()) return Error("Texture source is empty");

				auto result = image::Image<T, image::Layout::RGBA>::decode(encoded_data);
				if (!result) return result.error().forward("Failed to decode texture from encoded data");
				return std::move(*result);
			}

			std::expected<image::Image<T, image::Layout::RGBA>, Error> operator()(
				const std::filesystem::path& path
			) const noexcept
			{
				if (!std::filesystem::exists(path))
					return Error("Texture file does not exist", std::format("Path: {}", path.string()));

				try
				{
					auto mapped_file = mio::basic_mmap_source<std::byte>(path.string());
					return decode(std::span(mapped_file.begin(), mapped_file.size()));
				}
				catch (const std::system_error& e)
				{
					return Error("Mapping texture file failed", std::format("what(): {}", e.what()));
				}
			}

			std::expected<image::Image<T, image::Layout::RGBA>, Error> operator()(
				const std::vector<std::byte>& encoded_data
			) const noexcept
			{
				return decode(encoded_data);
			}

			std::expected<image::Image<T, image::Layout::RGBA>, Error> operator()(
				const image::Image<image::Format::Unorm8, image::Layout::RGBA>& raw_image
			) const noexcept
			{
				if constexpr (T == image::Format::Unorm8)
				{
					return raw_image;
				}
				else
				{
					const auto unorm8_to_unorm16 = [](glm::u8vec4 pix) {
						return glm::u16vec4(pix) * glm::u16vec4(0x0101);
					};

					return raw_image.map(unorm8_to_unorm16);
				}
			}

			std::expected<image::Image<T, image::Layout::RGBA>, Error> operator()(
				const image::Image<image::Format::Unorm16, image::Layout::RGBA>& raw_image
			) const noexcept
			{
				if constexpr (T == image::Format::Unorm16)
				{
					return raw_image;
				}
				else
				{
					const auto unorm16_to_unorm8 = [](glm::u16vec4 pix) {
						return glm::u8vec4(pix >> glm::u16vec4(8));
					};

					return raw_image.map(unorm16_to_unorm8);
				}
			}
		};
	}

	std::expected<Texture::ImageVariant, Error> Texture::load() const noexcept
	{
		const auto post_process = [this](ImageVariant result) -> ImageVariant {
			if (std::holds_alternative<image::Image<image::Format::Unorm8, image::Layout::RGBA>>(result))
			{
				auto image =
					std::get<image::Image<image::Format::Unorm8, image::Layout::RGBA>>(std::move(result));

				if (flip_x) image = image.flip_x();
				if (flip_y) image = image.flip_y();

				return image;
			}
			else
			{
				auto image =
					std::get<image::Image<image::Format::Unorm16, image::Layout::RGBA>>(std::move(result));

				if (flip_x) image = image.flip_x();
				if (flip_y) image = image.flip_y();

				return image;
			}
		};

		return std::visit(VariableFormatVisitor(), source).transform(post_process);
	}

	std::expected<image::Image<image::Format::Unorm8, image::Layout::RGBA>, Error>
	Texture::load_8bit() const noexcept
	{
		return std::visit(FixedFormatVisitor<image::Format::Unorm8>(), source).transform([this](auto image) {
			if (flip_x) image = image.flip_x();
			if (flip_y) image = image.flip_y();
			return image;
		});
	}

	std::expected<image::Image<image::Format::Unorm16, image::Layout::RGBA>, Error>
	Texture::load_16bit() const noexcept
	{
		return std::visit(FixedFormatVisitor<image::Format::Unorm16>(), source).transform([this](auto image) {
			if (flip_x) image = image.flip_x();
			if (flip_y) image = image.flip_y();
			return image;
		});
	}
}
