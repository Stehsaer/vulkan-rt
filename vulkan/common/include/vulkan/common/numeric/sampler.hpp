#pragma once

#include <libassert/assert.hpp>
#include <optional>
#include <vulkan/vulkan.hpp>

namespace vulkan
{
	///
	/// @brief Sampler filter mode, covers min/mag and mipmap
	/// @details
	/// Use the following pattern to form a `SamplerOption`, which can be used as `vk::SamplerCreateInfo`:
	/// `SamplerFilter::xxx + vk::SamplerMipmapMode::yyy`
	///
	/// @note This utility assumes that min and mag uses the same settings. If not, construct
	/// `vk::SamplerCreateInfo` manually instead.
	///
	enum class SamplerFilter
	{
		Nearest,              // Nearest min/mag, nearest mipmap
		Linear,               // Linear min/mag, linear mipmap
		NearestMipmapLinear,  // Nearest min/mag, linear mipmap
		LinearMipmapNearest   // Linear min/mag, nearest mipmap
	};

	struct SamplerUnnormalizedTag
	{};

	///
	/// @brief A tag for unnormalized coordinate
	///	@details Attach after a `SamplerOption` to turn on unnormalized coordinate
	///
	static constexpr SamplerUnnormalizedTag SamplerUnnormalized;

	///
	/// @brief Enable sampler anisotropy
	/// @details Attach after a `SamplerOption` to turn on anisotropy and assign a max level setting
	///
	struct SamplerAnisotropy
	{
		float max_anistropy;

		///
		/// @brief Enable sampler anisotropy on a sampler
		///
		/// @param max_anistropy Maximum anisotropy level
		///
		explicit SamplerAnisotropy(float max_anistropy) :
			max_anistropy(max_anistropy)
		{}

		SamplerAnisotropy(const SamplerAnisotropy&) = default;
		SamplerAnisotropy(SamplerAnisotropy&&) = default;
		SamplerAnisotropy& operator=(const SamplerAnisotropy&) = default;
		SamplerAnisotropy& operator=(SamplerAnisotropy&&) = default;
	};

	///
	/// @brief Holds simplified information needed to construct a sampler
	/// @details
	/// Construct this option by "add"ing `SamplerFilter` and `vk::SamplerAddressMode`. Append options such as
	/// unnormalized coordinates by appending corresponding option structs.
	///
	/// @note Can be used as a drop-in replacement for existing `vk::SamplerCreateInfo`.
	///
	struct SamplerOption
	{
		SamplerFilter filter;
		vk::SamplerAddressMode mode;
		std::optional<float> anisotropy = std::nullopt;
		bool unnormalized = false;

		constexpr SamplerOption operator+(SamplerUnnormalizedTag) const noexcept
		{
			return {
				.filter = filter,
				.mode = mode,
				.anisotropy = anisotropy,
				.unnormalized = true,
			};
		}

		constexpr SamplerOption operator+(SamplerAnisotropy anisotropy) const noexcept
		{
			return {
				.filter = filter,
				.mode = mode,
				.anisotropy = anisotropy.max_anistropy,
				.unnormalized = unnormalized,
			};
		}

		constexpr operator vk::SamplerCreateInfo() const noexcept
		{
			vk::SamplerCreateInfo info;

			switch (filter)
			{
			case SamplerFilter::Nearest:
			case SamplerFilter::NearestMipmapLinear:
				info.minFilter = info.magFilter = vk::Filter::eNearest;
				break;

			case SamplerFilter::Linear:
			case SamplerFilter::LinearMipmapNearest:
				info.minFilter = info.magFilter = vk::Filter::eLinear;
				break;

			default:
				UNREACHABLE();
			}

			switch (filter)
			{
			case SamplerFilter::Nearest:
			case SamplerFilter::LinearMipmapNearest:
				info.mipmapMode = vk::SamplerMipmapMode::eNearest;
				break;
			case SamplerFilter::Linear:
			case SamplerFilter::NearestMipmapLinear:
				info.mipmapMode = vk::SamplerMipmapMode::eLinear;
				break;
			};

			info.addressModeU = info.addressModeV = info.addressModeW = mode;
			if (anisotropy)
			{
				info.anisotropyEnable = vk::True;
				info.maxAnisotropy = *anisotropy;
			}

			info.unnormalizedCoordinates = unnormalized ? vk::True : vk::False;

			return info;
		}
	};

	constexpr SamplerOption operator+(SamplerFilter filter, vk::SamplerAddressMode mode) noexcept
	{
		return SamplerOption{.filter = filter, .mode = mode};
	}
}
