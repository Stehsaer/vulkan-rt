#pragma once

#include "common/util/async.hpp"
#include "common/util/error.hpp"
#include "model/material.hpp"
#include "render/model/texture.hpp"
#include "vulkan/interface/common-context.hpp"
#include "vulkan/util/static-resource-creator.hpp"

#include <coro/coro.hpp>
#include <coro/thread_pool.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace render
{
	///
	/// @brief GPU texture list class, managing all textures used in `model::MaterialList`
	///
	class TextureList
	{
	  public:

		///
		/// @brief Load options for texture loading
		///
		///
		struct LoadOption
		{
			Texture::ColorLoadStrategy color_load_strategy = Texture::ColorLoadStrategy::BalancedBC;
			Texture::NormalLoadStrategy normal_load_strategy = Texture::NormalLoadStrategy::AllBC5;
			vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eSampled;

			// Set to `true` to make loading fail if any texture fails to load, `false` to ignore failed loads
			// and use error hint textures
			bool exit_on_failed_load = false;

			// Max size of pending upload data allowed during loading
			size_t max_pending_data_size = 16 * 1048576;
		};

		///
		/// @brief Create a texture list asynchronously using coroutine
		///
		/// @param thread_pool Thread pool for asynchronous loading
		/// @param context Vulkan device context
		/// @param material_list Material list to load textures
		/// @param load_option Options for texture loading
		/// @param progress Progress reporter for texture loading progress, will be incremented by 1 for each
		/// loaded texture
		/// @return Created `TextureList` on success, or an `Error` on failure
		///
		[[nodiscard]]
		static coro::task<std::expected<TextureList, Error>> create(
			coro::thread_pool& thread_pool,
			util::Progress progress,
			const vulkan::DeviceContext& context,
			const model::MaterialList& material_list,
			LoadOption load_option
		) noexcept;

		///
		/// @brief Result of querying a texture by index
		///
		///
		struct TextureResult
		{
			Texture::Ref texture;
			model::SampleMode sample_mode;
		};

		///
		/// @brief Get color texture by index
		/// @note
		/// - If the index is `std::nullopt`, the fallback texture will be returned
		/// - If the index is out of range, or the texture at the index is not available, the error hint
		/// texture will be returned
		///
		/// @param index Texture index
		/// @return A pair of the texture reference and its sample mode
		///
		[[nodiscard]]
		TextureResult get_color_texture(std::optional<uint32_t> index) const noexcept;

		///
		/// @brief Get normal texture by index
		/// @note
		/// - If the index is `std::nullopt`, the fallback texture will be returned
		/// - If the index is out of range, or the texture at the index is not available, the error hint
		/// texture will be returned
		///
		/// @param index Texture index
		/// @return A pair of the texture reference and its sample mode
		///
		[[nodiscard]]
		TextureResult get_normal_texture(std::optional<uint32_t> index) const noexcept;

	  private:

		struct TextureTuple
		{
			std::optional<Texture> color;
			std::optional<Texture> normal;
			model::SampleMode sample_mode;
		};

		std::unique_ptr<std::vector<TextureTuple>> textures;
		std::unique_ptr<Texture> color_fallback, normal_fallback, error_hint_texture;

		static constexpr model::SampleMode FALLBACK_SAMPLE_MODE = {};
		static constexpr model::SampleMode ERROR_HINT_SAMPLE_MODE = {
			.min_filter = model::Filter::Nearest,
			.mag_filter = model::Filter::Nearest,
			.mipmap_filter = model::Filter::Nearest,
			.max_mipmap_level = 0
		};

		TextureList(
			std::vector<TextureTuple> textures,
			Texture color_fallback,
			Texture normal_fallback,
			Texture error_hint_texture
		) :
			textures(std::make_unique<std::vector<TextureTuple>>(std::move(textures))),
			color_fallback(std::make_unique<Texture>(std::move(color_fallback))),
			normal_fallback(std::make_unique<Texture>(std::move(normal_fallback))),
			error_hint_texture(std::make_unique<Texture>(std::move(error_hint_texture)))
		{}

		// Create texture tuple for a single texture, with no progress reporting
		[[nodiscard]]
		static coro::task<std::expected<TextureTuple, Error>> create_texture_tuple(
			coro::thread_pool& thread_pool,
			vulkan::StaticResourceCreator& resource_creator,
			const model::Texture& texture,
			model::TextureUsage texture_usage,
			LoadOption load_option,
			const util::Progress& progress
		) noexcept;

	  public:

		TextureList(const TextureList&) = delete;
		TextureList(TextureList&&) = default;
		TextureList& operator=(const TextureList&) = delete;
		TextureList& operator=(TextureList&&) = default;
	};
}
