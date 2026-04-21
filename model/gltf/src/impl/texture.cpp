#include "texture.hpp"
#include "model/texture.hpp"
#include <fastgltf/types.hpp>

namespace model::gltf::impl
{
	[[nodiscard]]
	std::expected<Texture::SourceType, Error> parse_image_data_source(Asset&, const std::monostate&) noexcept
	{
		return Error("Image has no source");
	}

	[[nodiscard]]
	std::expected<Texture::SourceType, Error> parse_image_data_source(
		Asset& asset,
		const fastgltf::sources::BufferView& buffer_view_source
	) noexcept
	{
		const auto& buffer_view = asset.bufferViews[buffer_view_source.bufferViewIndex];
		auto& buffer = asset.unified_buffers[buffer_view.bufferIndex];

		auto data_result = buffer.get_copy(buffer_view);
		if (!data_result) return data_result.error().forward("Acquire image data from buffer view failed");
		return std::move(*data_result);
	}

	[[nodiscard]]
	std::expected<Texture::SourceType, Error> parse_image_data_source(
		Asset& asset,
		const fastgltf::sources::URI& uri
	) noexcept
	{
		if (!uri.uri.isLocalPath()) return Error("Only local paths are supported as image sources");

		auto path = uri.uri.fspath();
		if (path.is_relative())
		{
			if (!asset.directory.has_value())
				return Error("Cannot resolve relative image path without search directory");
			path = *asset.directory / path;
		}

		return path;
	}

	[[nodiscard]]
	std::expected<Texture::SourceType, Error> parse_image_data_source(
		Asset&,
		const fastgltf::sources::Array& array
	) noexcept
	{
		return std::vector(std::from_range, array.bytes);
	}

	[[nodiscard]]
	std::expected<Texture::SourceType, Error> parse_image_data_source(
		Asset&,
		const fastgltf::sources::Vector& vector
	) noexcept
	{
		return std::vector(std::from_range, vector.bytes);
	}

	[[nodiscard]]
	std::expected<Texture::SourceType, Error> parse_image_data_source(
		Asset&,
		const fastgltf::sources::CustomBuffer&
	) noexcept
	{
		return Error("Custom buffers are not supported as image sources");
	}

	[[nodiscard]]
	std::expected<Texture::SourceType, Error> parse_image_data_source(
		Asset&,
		const fastgltf::sources::ByteView& view
	) noexcept
	{
		return std::vector(std::from_range, view.bytes);
	}

	[[nodiscard]]
	std::expected<Texture::SourceType, Error> parse_image_data_source(
		Asset&,
		const fastgltf::sources::Fallback&
	) noexcept
	{
		return Error("Fallback image sources are not supported");
	}

	std::expected<Texture::SourceType, Error> parse_image(Asset& asset, const fastgltf::Image& image) noexcept
	{
		return std::visit(
			[&asset](const auto& variant) { return parse_image_data_source(asset, variant); },
			image.data
		);
	}

	static Wrap parse_wrap_mode(fastgltf::Wrap wrap) noexcept
	{
		switch (wrap)
		{
		case fastgltf::Wrap::ClampToEdge:
			return Wrap::ClampToEdge;
		case fastgltf::Wrap::MirroredRepeat:
			return Wrap::MirroredRepeat;
		case fastgltf::Wrap::Repeat:
		default:
			return Wrap::Repeat;
		}
	}

	static Filter parse_filter_mode(fastgltf::Filter filter) noexcept
	{
		switch (filter)
		{
		case fastgltf::Filter::Linear:
			return Filter::Linear;
		case fastgltf::Filter::Nearest:
		default:
			return Filter::Nearest;
		}
	}

	SampleMode parse_sampler(const fastgltf::Sampler& sampler) noexcept
	{
		return {
			.min_filter = sampler.minFilter.transform(parse_filter_mode).value_or(Filter::Linear),
			.mag_filter = sampler.magFilter.transform(parse_filter_mode).value_or(Filter::Linear),
			.mipmap_filter = Filter::Linear,
			.wrap_u = parse_wrap_mode(sampler.wrapS),
			.wrap_v = parse_wrap_mode(sampler.wrapT),
		};
	}

	std::expected<Texture, Error> parse_texture(Asset& asset, const fastgltf::Texture& texture) noexcept
	{
		if (texture.samplerIndex.has_value() && texture.samplerIndex.value() >= asset.samplers.size())
			return Error("Sampler index out of bounds");

		if (!texture.imageIndex.has_value()) return Error("Texture missing image index");
		if (texture.imageIndex.value() >= asset.images.size()) return Error("Image index out of bounds");

		const auto sample_mode =
			texture.samplerIndex
				.transform([&asset](size_t index) { return parse_sampler(asset.samplers[index]); })
				.value_or(SampleMode());

		auto image_result = parse_image(asset, asset.images[texture.imageIndex.value()]);
		if (!image_result) return image_result.error().forward("Parse texture image failed");

		return Texture{
			.source = std::move(*image_result),
			.sample_mode = sample_mode,
		};
	}

	std::expected<std::vector<Texture>, Error> parse_textures(Asset& asset) noexcept
	{
		return asset.textures
			| std::views::transform([&asset](const fastgltf::Texture& texture) {
				   return parse_texture(asset, texture);
			   })
			| Error::collect();
	}
}
