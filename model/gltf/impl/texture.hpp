#pragma once

#include "asset.hpp"
#include "common/util/error.hpp"
#include "model/texture.hpp"

#include <fastgltf/types.hpp>

namespace model::gltf::impl
{
	[[nodiscard]]
	std::expected<Texture::SourceType, Error> parse_image_data_source(
		Asset& asset,
		const std::monostate&
	) noexcept;

	[[nodiscard]]
	std::expected<Texture::SourceType, Error> parse_image_data_source(
		Asset& asset,
		const fastgltf::sources::BufferView&
	) noexcept;

	[[nodiscard]]
	std::expected<Texture::SourceType, Error> parse_image_data_source(
		Asset& asset,
		const fastgltf::sources::URI&
	) noexcept;

	[[nodiscard]]
	std::expected<Texture::SourceType, Error> parse_image_data_source(
		Asset& asset,
		const fastgltf::sources::Array&
	) noexcept;

	[[nodiscard]]
	std::expected<Texture::SourceType, Error> parse_image_data_source(
		Asset& asset,
		const fastgltf::sources::Vector&
	) noexcept;

	[[nodiscard]]
	std::expected<Texture::SourceType, Error> parse_image_data_source(
		Asset& asset,
		const fastgltf::sources::CustomBuffer&
	) noexcept;

	[[nodiscard]]
	std::expected<Texture::SourceType, Error> parse_image_data_source(
		Asset& asset,
		const fastgltf::sources::ByteView&
	) noexcept;

	[[nodiscard]]
	std::expected<Texture::SourceType, Error> parse_image_data_source(
		Asset& asset,
		const fastgltf::sources::Fallback&
	) noexcept;

	[[nodiscard]]
	std::expected<Texture::SourceType, Error> parse_image(
		Asset& asset,
		const fastgltf::Image& image
	) noexcept;

	[[nodiscard]]
	SampleMode parse_sampler(const fastgltf::Sampler& sampler) noexcept;

	[[nodiscard]]
	std::expected<Texture, Error> parse_texture(Asset& asset, const fastgltf::Texture& texture) noexcept;

	[[nodiscard]]
	std::expected<std::vector<Texture>, Error> parse_textures(Asset& asset) noexcept;
}
