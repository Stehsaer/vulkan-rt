#pragma once

#include <cstdint>
#include <glm/glm.hpp>

#include "common/util/error.hpp"
#include "texture.hpp"

namespace model
{
	enum class AlphaMode : uint8_t
	{
		Opaque,
		Mask,
		Blend
	};

	///
	/// @brief Tracks how a texture is used in the material
	/// @note Multiple instances of `vk::Image` can be created with different formats based on different
	/// usages
	///
	struct TextureUsage
	{
		bool srgb = false;    // Used as sRGB texture
		bool linear = false;  // Used as linear texture
		bool normal = false;  // Used as normal map
	};

	///
	/// @brief Texture set for the material, contains optional references to various texture types
	/// @note Implementations need not verify the validity of the texture references, as they will be verified
	/// together when creating material set.
	///
	struct TextureSet
	{
		///
		/// @brief Albedo texture
		/// @details
		/// - sRGB usage
		/// - Uses RGB channels for albedo color, A channel for alpha
		/// - Fallback: (R=1, G=1, B=1, A=1)
		///
		std::optional<uint32_t> albedo = std::nullopt;

		///
		/// @brief Emissive texture
		/// @details
		/// - sRGB usage
		/// - Uses RGB channels for (base) emissive color, A channel ignored
		/// - Fallback: (R=1, G=1, B=1, A=1)
		///
		std::optional<uint32_t> emissive = std::nullopt;

		///
		/// @brief Roughness-metallic texture
		/// @details
		/// - Linear usage
		/// - Metallic maps to B channel, roughness maps to G channel, A channel
		/// ignored
		/// - Fallback: (R=1, G=1, B=1, A=1)
		///
		std::optional<uint32_t> roughness_metallic = std::nullopt;

		///
		/// @brief Normal texture
		/// @details
		/// - Normal map usage
		/// - Uses RG channels for normal map's XY axis, BA channel ignored
		/// - Fallback: (R=0.5, G=0.5, B=1, A=1)
		///
		std::optional<uint32_t> normal = std::nullopt;

		///
		/// @brief Get general fallback texture for missing textures (excluding normal)
		/// @note Contains a 4x4 white pixel (R=1, G=1, B=1, A=1)
		/// @return Texture object with fallback data
		///
		[[nodiscard]]
		static image::Image<
			image::Format::Unorm8,
			image::Layout::RGBA
		> get_general_fallback_texture() noexcept;

		///
		/// @brief Get fallback texture for missing normal maps
		/// @note Contains a 4x4 pixel with (R=0.5, G=0.5, B=1, A=1)
		/// @return Texture object with fallback data
		///
		[[nodiscard]]
		static image::Image<
			image::Format::Unorm8,
			image::Layout::RGBA
		> get_normal_map_fallback_texture() noexcept;
	};

	///
	/// @brief Material class, represents a generic PBR material
	/// @note This is mostly a slightly modified version of glTF 2.0's PBR material model. Alpha texture is
	/// separated from albedo texture.
	///
	struct Material
	{
		// Render parameters for the material
		struct Param
		{
			glm::vec4 base_color_factor = glm::vec4(1.0f);
			glm::vec3 emissive_factor = glm::vec3(0.0f);
			float alpha_cutoff = 0.5f;
			float metallic_factor = 0.0f;
			float roughness_factor = 0.3f;
			glm::vec2 normal_scale = glm::vec2(1.0f);
		};

		// Rendering mode for the material
		struct Mode
		{
			AlphaMode alpha_mode = AlphaMode::Opaque;
			bool double_sided = false;

			[[nodiscard]]
			std::strong_ordering operator<=>(const Mode&) const noexcept = default;

			[[nodiscard]]
			bool operator==(const Mode&) const noexcept = default;
		};

		Param param = Param();
		Mode mode = Mode();
		TextureSet texture_set = TextureSet();
	};

	///
	/// @brief Material set class, contains a set of materials and their textures
	/// @note Use `operator->` to access the materials and textures after creation.
	///
	class MaterialList
	{
	  public:

		static constexpr Material DEFAULT_MATERIAL = Material();

		std::vector<std::pair<Texture, TextureUsage>> textures;
		std::vector<Material> materials;

		///
		/// @brief Create a material set from a list of textures and materials
		/// @details Validates that all texture references in the materials are within bounds of the provided
		/// texture list. Also tracks texture usage for each texture.
		/// @note The function takes ownership of the input `textures` and `materials` vectors as the input
		/// types imply.
		///
		/// @param textures List of textures. The function takes ownership away from the input `textures`
		/// @param materials List of materials. The function takes ownership away from the input `materials`
		/// @return Created `MaterialList` if successful, or an `Error` if validation fails
		///
		[[nodiscard]]
		static std::expected<MaterialList, Error> create(
			std::vector<Texture> textures,
			std::vector<Material> materials
		) noexcept;

	  private:

		explicit MaterialList(
			std::vector<std::pair<Texture, TextureUsage>> textures,
			std::vector<Material> materials
		) noexcept :
			textures(std::move(textures)),
			materials(std::move(materials))
		{}

	  public:

		MaterialList(const MaterialList&) = delete;
		MaterialList(MaterialList&&) = default;
		MaterialList& operator=(const MaterialList&) = delete;
		MaterialList& operator=(MaterialList&&) = default;
	};
}
