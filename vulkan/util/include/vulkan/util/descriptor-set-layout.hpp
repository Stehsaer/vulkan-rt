#pragma once

#include "common/util/error.hpp"
#include "vulkan/interface/context.hpp"

#include <array>
#include <concepts>
#include <cstdint>
#include <expected>
#include <tuple>
#include <utility>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_raii.hpp>

namespace vulkan
{
	///
	/// @brief Base class of mono descriptor set slot, providing internal type-related infra
	///
	class MonoDescriptorSetSlotBase
	{
		enum class TypeEnum
		{
			Unknown,
			Buffer,
			Image,
			AccelerationStructure
		};

		static consteval TypeEnum get_type(vk::DescriptorType type) noexcept
		{
			switch (type)
			{
			case vk::DescriptorType::eStorageBuffer:
			case vk::DescriptorType::eUniformBuffer:
				return TypeEnum::Buffer;

			case vk::DescriptorType::eAccelerationStructureKHR:
				return TypeEnum::AccelerationStructure;

			case vk::DescriptorType::eStorageImage:
			case vk::DescriptorType::eSampledImage:
			case vk::DescriptorType::eCombinedImageSampler:
				return TypeEnum::Image;

			default:
				return TypeEnum::Unknown;
			}
		}

		template <TypeEnum E>
			requires(E != TypeEnum::Unknown)
		struct InfoTypeStruct;

	  protected:

		template <vk::DescriptorType E>
		using InfoType = InfoTypeStruct<get_type(E)>::Type;
	};

	template <>
	struct MonoDescriptorSetSlotBase::InfoTypeStruct<MonoDescriptorSetSlotBase::TypeEnum::Buffer>
	{
		using Type = vk::DescriptorBufferInfo;
	};

	template <>
	struct MonoDescriptorSetSlotBase::InfoTypeStruct<MonoDescriptorSetSlotBase::TypeEnum::Image>
	{
		using Type = vk::DescriptorImageInfo;
	};

	template <>
	struct MonoDescriptorSetSlotBase::InfoTypeStruct<
		MonoDescriptorSetSlotBase::TypeEnum::AccelerationStructure
	>
	{
		using Type = vk::WriteDescriptorSetAccelerationStructureKHR;
	};

	///
	/// @brief Base class of mono descriptor set, provides slot-agnostic internal functions
	///
	class MonoDescriptorSetLayoutBase
	{
	  protected:

		static vk::WriteDescriptorSet write_descriptor_set(
			vk::DescriptorSet set,
			uint32_t binding,
			vk::DescriptorType type,
			const vk::DescriptorBufferInfo& info
		) noexcept;

		static vk::WriteDescriptorSet write_descriptor_set(
			vk::DescriptorSet set,
			uint32_t binding,
			vk::DescriptorType type,
			const vk::DescriptorImageInfo& info
		) noexcept;

		static vk::WriteDescriptorSet write_descriptor_set(
			vk::DescriptorSet set,
			uint32_t binding,
			vk::DescriptorType type,
			const vk::WriteDescriptorSetAccelerationStructureKHR& info
		) noexcept;
	};

	///
	/// @brief A descriptor slot description, where the slot only holds exactly one descriptor
	///
	/// @tparam Type Type of the descriptor slot
	/// @tparam Stage Stages of the descriptor slot
	///
	template <vk::DescriptorType Type, vk::ShaderStageFlagBits... Stage>
	struct MonoDescriptorSetSlot : public MonoDescriptorSetSlotBase
	{
		///
		/// @brief Type of the descriptor info
		///
		using InfoType = MonoDescriptorSetSlotBase::InfoType<Type>;

		static constexpr vk::DescriptorType DescriptorType = Type;
		static constexpr vk::ShaderStageFlags StageFlags = (Stage | ...);
	};

	///
	/// @brief "Mono" descriptor set layout where each slot exactly holds 1 descriptor
	/// @details
	/// - The layout is described by the slots specified in the template parameter
	/// - Each slot corresponds to exactly one binding index, asssigned sequencially starting from 0.
	///
	/// @tparam Slot List of slots
	///
	template <std::derived_from<MonoDescriptorSetSlotBase>... Slot>
		requires(sizeof...(Slot) != 0)
	class MonoDescriptorSetLayout : public MonoDescriptorSetLayoutBase
	{
	  public:

		///
		/// @brief Get vulkan descriptor set layout binding information
		/// @return Array of layout binding structs
		///
		static consteval std::array<vk::DescriptorSetLayoutBinding, sizeof...(Slot)> get_bindings() noexcept
		{
			std::array<vk::DescriptorSetLayoutBinding, sizeof...(Slot)> bindings;

			uint32_t binding = 0;
			((bindings[binding] =
				  vk::DescriptorSetLayoutBinding{
					  .binding = binding,
					  .descriptorType = Slot::DescriptorType,
					  .descriptorCount = 1,
					  .stageFlags = Slot::StageFlags
				  },
			  binding++),
			 ...);

			return bindings;
		}

		///
		/// @brief Create a vulkan descriptor set layout instance from the current layout
		///
		/// @param context Vulkan context
		/// @return Created descriptor set layout instance or error
		///
		static std::expected<vk::raii::DescriptorSetLayout, Error> create_descriptor_set_layout(
			const vulkan::Context& context
		) noexcept
		{
			static constexpr auto BINDINGS = get_bindings();

			auto layout_result = context.device.createDescriptorSetLayout(
				vk::DescriptorSetLayoutCreateInfo().setBindings(BINDINGS)
			);
			if (!layout_result) return Error::from(layout_result);

			return std::move(*layout_result);
		}

		///
		/// @brief Get write info structs using descirptor info structs, where the order is deduced from
		/// current layout.
		///
		/// @param set Target descriptor set
		/// @param info Descriptor info structs
		/// @return Array of write info structs
		///
		static std::array<vk::WriteDescriptorSet, sizeof...(Slot)> get_write_infos(
			vk::DescriptorSet set,
			const Slot::InfoType&... info
		) noexcept
		{
			std::array<vk::WriteDescriptorSet, sizeof...(Slot)> result;

			uint32_t binding = 0;
			((result[binding] = write_descriptor_set(set, binding, Slot::DescriptorType, info), binding++),
			 ...);

			return result;
		}

		///
		/// @brief Get write info structs using descirptor info structs, where the order is deduced from
		/// current layout.
		///
		/// @param set Target descriptor set
		/// @param info Tuple of descriptor info structs
		/// @return Array of write info structs
		///
		static std::array<vk::WriteDescriptorSet, sizeof...(Slot)> get_write_infos(
			vk::DescriptorSet set,
			std::tuple<const typename Slot::InfoType&...> info
		) noexcept
		{
			return std::apply([set](const auto&... info) { return get_write_infos(set, info...); }, info);
		}
	};
}
