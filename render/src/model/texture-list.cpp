#include "render/model/texture-list.hpp"

#include <ranges>

namespace render
{
	static image::Image<image::Format::Unorm8, image::Layout::RGBA> get_error_hint_image() noexcept
	{
		auto image = image::Image<image::Format::Unorm8, image::Layout::RGBA>({16, 16});

		for (const auto [y, x] :
			 std::views::cartesian_product(std::views::iota(0u, 16u), std::views::iota(0u, 16u)))
		{
			const bool is_purple = (x + y) % 2 == 0;
			image[x, y] = is_purple ? glm::u8vec4(255, 0, 255, 255) : glm::u8vec4(0, 0, 0, 255);
		}

		return image;
	}

	// (color fallback, normal fallback, error hint texture)
	static std::expected<std::tuple<Texture, Texture, Texture>, Error> load_fallback_textures(
		vulkan::StaticResourceCreator& resource_creator,
		TextureList::LoadOption load_option
	) noexcept
	{
		auto color_fallback_result = Texture::load_color_texture(
			resource_creator,
			model::Texture{.source = model::TextureSet::get_general_fallback_texture()},
			Texture::ColorLoadStrategy::AllBC7,
			load_option.usage
		);
		if (!color_fallback_result)
			return color_fallback_result.error().forward("Load fallback color texture failed");
		auto color_fallback = std::move(*color_fallback_result);

		auto normal_fallback_result = Texture::load_normal_texture(
			resource_creator,
			model::Texture{.source = model::TextureSet::get_normal_map_fallback_texture()},
			Texture::NormalLoadStrategy::AllBC5,
			load_option.usage
		);
		if (!normal_fallback_result)
			return normal_fallback_result.error().forward("Load fallback normal texture failed");
		auto normal_fallback = std::move(*normal_fallback_result);

		auto error_hint_texture_result = Texture::load_color_texture(
			resource_creator,
			model::Texture{.source = get_error_hint_image()},
			Texture::ColorLoadStrategy::Raw,
			load_option.usage
		);
		if (!error_hint_texture_result)
			return error_hint_texture_result.error().forward("Load error hint texture failed");
		auto error_hint_texture = std::move(*error_hint_texture_result);

		return std::make_tuple(
			std::move(color_fallback),
			std::move(normal_fallback),
			std::move(error_hint_texture)
		);
	}

	coro::task<std::expected<TextureList::TextureTuple, Error>> TextureList::create_texture_tuple(
		coro::thread_pool& thread_pool,
		vulkan::StaticResourceCreator& resource_creator,
		const model::Texture& texture,
		model::TextureUsage texture_usage,
		LoadOption load_option,
		const util::Progress& progress
	) noexcept
	{
		co_await thread_pool.schedule();

		std::optional<Texture> color_texture;
		std::optional<Texture> normal_texture;

		if (texture_usage.linear || texture_usage.srgb)
		{
			auto color_texture_result = Texture::load_color_texture(
				resource_creator,
				texture,
				load_option.color_load_strategy,
				load_option.usage
			);

			if (!color_texture_result)
			{
				if (load_option.exit_on_failed_load)
					co_return color_texture_result.error().forward("Load color texture failed");
			}
			else
				color_texture = std::move(*color_texture_result);
		}

		if (texture_usage.normal)
		{
			auto normal_texture_result = Texture::load_normal_texture(
				resource_creator,
				texture,
				load_option.normal_load_strategy,
				load_option.usage
			);

			if (!normal_texture_result)
			{
				if (load_option.exit_on_failed_load)
					co_return normal_texture_result.error().forward("Load normal texture failed");
			}
			else
				normal_texture = std::move(*normal_texture_result);
		}

		if (const auto upload_result =
				resource_creator.execute_uploads_with_size_thres(load_option.max_pending_data_size);
			!upload_result)
			co_return upload_result.error().forward("Execute upload tasks failed");

		progress.increment();

		co_return TextureTuple{
			.color = std::move(color_texture),
			.normal = std::move(normal_texture),
			.sample_mode = texture.sample_mode
		};
	}

	coro::task<std::expected<TextureList, Error>> TextureList::create(
		coro::thread_pool& thread_pool,
		util::Progress progress,
		const vulkan::DeviceContext& context,
		const model::MaterialList& material_list,
		LoadOption load_option
	) noexcept
	{
		auto resource_creator = vulkan::StaticResourceCreator(context);

		/* Load and upload default textures */

		auto fallback_result = load_fallback_textures(resource_creator, load_option);
		if (!fallback_result) co_return fallback_result.error().forward("Load fallback textures failed");
		auto [color_fallback, normal_fallback, error_hint_texture] = std::move(*fallback_result);

		/* Load material textures */

		const auto task_func =
			[&progress, &thread_pool, &resource_creator, &load_option](const auto& texture_info) {
				return create_texture_tuple(
					thread_pool,
					resource_creator,
					texture_info.first,
					texture_info.second,
					load_option,
					progress
				);
			};

		progress.set_total(material_list.textures.size());
		auto tasks =
			material_list.textures | std::views::transform(task_func) | std::ranges::to<std::vector>();

		auto texture_results = co_await coro::when_all(std::move(tasks))
			| std::views::transform([](auto&& result) { return std::move(result.return_value()); })
			| Error::collect();
		if (!texture_results) co_return texture_results.error().forward("Load textures failed");
		auto textures = std::move(*texture_results);

		/* Last upload */

		if (const auto upload_result = resource_creator.execute_uploads(); !upload_result)
			co_return upload_result.error().forward("Execute upload tasks failed");

		co_return TextureList(
			std::move(textures),
			std::move(color_fallback),
			std::move(normal_fallback),
			std::move(error_hint_texture)
		);
	}

	TextureList::TextureResult TextureList::get_color_texture(std::optional<uint32_t> index) const noexcept
	{
		if (!index.has_value())
			return {.texture = color_fallback->ref(), .sample_mode = FALLBACK_SAMPLE_MODE};

		if (*index >= textures->size())
			return {.texture = error_hint_texture->ref(), .sample_mode = ERROR_HINT_SAMPLE_MODE};

		if (const auto& texture_tuple = textures->at(*index); !texture_tuple.color.has_value())
			return {.texture = error_hint_texture->ref(), .sample_mode = ERROR_HINT_SAMPLE_MODE};
		else
			return {.texture = texture_tuple.color->ref(), .sample_mode = texture_tuple.sample_mode};
	}

	TextureList::TextureResult TextureList::get_normal_texture(std::optional<uint32_t> index) const noexcept
	{
		if (!index.has_value())
			return {.texture = normal_fallback->ref(), .sample_mode = FALLBACK_SAMPLE_MODE};

		if (*index >= textures->size())
			return {.texture = error_hint_texture->ref(), .sample_mode = ERROR_HINT_SAMPLE_MODE};

		if (const auto& texture_tuple = textures->at(*index); !texture_tuple.normal.has_value())
			return {.texture = error_hint_texture->ref(), .sample_mode = ERROR_HINT_SAMPLE_MODE};
		else
			return {.texture = texture_tuple.normal->ref(), .sample_mode = texture_tuple.sample_mode};
	}
}
