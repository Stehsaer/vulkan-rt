#pragma once

#include "common/util/construct.hpp"
#include "common/util/error.hpp"
#include "common/util/pmr-unique-ptr.hpp"
#include "common/util/tuple.hpp"
#include "vulkan/interface/attachment.hpp"
#include "vulkan/interface/context.hpp"
#include "vulkan/numeric/pool-size.hpp"

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <memory_resource>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace vulkan::trivset
{
	namespace detail
	{
		// Checks if expressions are constexpr. Intentionally ill-formed if arguments aren't constexpr.
		template <typename... T>
		consteval bool is_constexpr(T...)
		{
			return true;
		}

		template <typename T>
		concept WriteInfo = std::invocable<T>
			&& std::convertible_to<std::invoke_result_t<T>, vk::WriteDescriptorSet>
			&& std::convertible_to<decltype(T::ALLOC_SIZE), size_t>;

		struct WriteTarget
		{
			vk::DescriptorSet set;
			uint32_t binding;
		};

		class ImageWriteInfo : public WriteTarget
		{
		  public:

			static constexpr auto ALLOC_SIZE = sizeof(vk::DescriptorImageInfo);

			explicit ImageWriteInfo(
				WriteTarget write_target,
				std::pmr::memory_resource& resource,
				vk::DescriptorType type,
				const vk::DescriptorImageInfo& info
			) noexcept;

			[[nodiscard]]
			vk::WriteDescriptorSet operator()() const noexcept;

		  private:

			vk::DescriptorType type;
			util::PmrUniquePtr<vk::DescriptorImageInfo> info;

		  public:

			ImageWriteInfo(const ImageWriteInfo&) = delete;
			ImageWriteInfo(ImageWriteInfo&&) = default;
			ImageWriteInfo& operator=(const ImageWriteInfo&) = delete;
			ImageWriteInfo& operator=(ImageWriteInfo&&) = default;
		};

		class BufferWriteInfo : public WriteTarget
		{
		  public:

			static constexpr auto ALLOC_SIZE = sizeof(vk::DescriptorBufferInfo);

			explicit BufferWriteInfo(
				WriteTarget write_target,
				std::pmr::memory_resource& resource,
				vk::DescriptorType type,
				const vk::DescriptorBufferInfo& info
			) noexcept;

			[[nodiscard]]
			vk::WriteDescriptorSet operator()() const noexcept;

		  private:

			vk::DescriptorType type;
			util::PmrUniquePtr<vk::DescriptorBufferInfo> info;

		  public:

			BufferWriteInfo(const BufferWriteInfo&) = delete;
			BufferWriteInfo(BufferWriteInfo&&) = default;
			BufferWriteInfo& operator=(const BufferWriteInfo&) = delete;
			BufferWriteInfo& operator=(BufferWriteInfo&&) = default;
		};

		class AccelerationStructureWriteInfo : public WriteTarget
		{
		  public:

			static constexpr auto ALLOC_SIZE =
				sizeof(vk::AccelerationStructureKHR) + sizeof(vk::WriteDescriptorSetAccelerationStructureKHR);

			explicit AccelerationStructureWriteInfo(
				WriteTarget write_target,
				std::pmr::memory_resource& resource,
				vk::AccelerationStructureKHR accel_struct
			) noexcept;

			[[nodiscard]]
			vk::WriteDescriptorSet operator()() const noexcept;

		  private:

			util::PmrUniquePtr<vk::AccelerationStructureKHR> accel_struct;
			util::PmrUniquePtr<vk::WriteDescriptorSetAccelerationStructureKHR> write_info;

		  public:

			AccelerationStructureWriteInfo(const AccelerationStructureWriteInfo&) = delete;
			AccelerationStructureWriteInfo(AccelerationStructureWriteInfo&&) = default;
			AccelerationStructureWriteInfo& operator=(const AccelerationStructureWriteInfo&) = delete;
			AccelerationStructureWriteInfo& operator=(AccelerationStructureWriteInfo&&) = default;
		};

		static_assert(WriteInfo<ImageWriteInfo>);
		static_assert(WriteInfo<BufferWriteInfo>);
		static_assert(WriteInfo<AccelerationStructureWriteInfo>);

		template <typename T>
		concept Slot =
			requires() {
				{ std::decay_t<T>::TYPE } -> std::convertible_to<vk::DescriptorType>;
				requires is_constexpr(std::decay_t<T>::TYPE);
			}
			&& requires(
				const T& slot,
				std::pmr::memory_resource& resource,
				const detail::WriteTarget write_target
			) {
				   { slot.write_info_fn(resource, write_target) } -> WriteInfo;
			   };

		template <Slot T>
		using WriteInfoOfSlot = std::remove_cvref_t<decltype(std::declval<T>().write_info_fn(
			std::declval<std::pmr::memory_resource&>(),
			detail::WriteTarget()
		))>;

		template <typename Base, auto MemPtr>
		concept SlotPtr = std::is_member_object_pointer_v<decltype(MemPtr)> && requires(const Base& base) {
			{ base.*MemPtr } -> Slot;
		};

		template <typename Base>
		concept TupleOfSlotPtr = []<std::size_t... Is>(std::index_sequence<Is...>) {
			return (SlotPtr<Base, std::get<Is>(Base::SLOT_LIST)> && ...);
		}(std::make_index_sequence<std::tuple_size_v<decltype(Base::SLOT_LIST)>>());

		template <typename T>
		concept LayoutStruct = requires {
			{ std::decay_t<T>::STAGE } -> std::convertible_to<vk::ShaderStageFlags>;
			requires TupleOfSlotPtr<T>;
			requires is_constexpr(std::decay_t<T>::SLOT_LIST, std::decay_t<T>::STAGE);
		};

		template <LayoutStruct Base, auto MemPtr>
		consteval vk::DescriptorSetLayoutBinding get_binding_info(
			uint32_t binding,
			vk::ShaderStageFlags stages
		) noexcept
		{
			using SlotType = std::decay_t<std::invoke_result_t<decltype(MemPtr), Base>>;

			return vk::DescriptorSetLayoutBinding{
				.binding = binding,
				.descriptorType = SlotType::TYPE,
				.descriptorCount = 1,
				.stageFlags = stages,
				.pImmutableSamplers = nullptr,
			};
		}

		template <LayoutStruct Base>
		consteval std::array<vk::DescriptorSetLayoutBinding, std::tuple_size_v<decltype(Base::SLOT_LIST)>>
		get_binding_info_list() noexcept
		{
			return []<std::size_t... Is>(std::index_sequence<Is...>) {
				return std::to_array(
					{get_binding_info<Base, std::get<Is>(Base::SLOT_LIST)>(Is, Base::STAGE)...}
				);
			}(std::make_integer_sequence<size_t, std::tuple_size_v<decltype(Base::SLOT_LIST)>>());
		}

		template <LayoutStruct T>
		constexpr size_t AllocSizeOf = util::fold_left_tuple(
			util::map_tuple(
				T::SLOT_LIST,
				[]<typename MemPtr>(MemPtr) {
					return std::max(
						std::alignment_of_v<std::max_align_t>,
						WriteInfoOfSlot<std::invoke_result_t<MemPtr, T>>::ALLOC_SIZE
					);
				}
			),
			0zu,
			std::plus()
		);
	}

	namespace slot
	{
		///
		/// @brief Combined image sampler slot
		/// @note Operator `+` can be used create a combined image sampler, for example, `image + sampler`
		///
		class CombinedImageSampler
		{
		  public:

			static constexpr auto TYPE = vk::DescriptorType::eCombinedImageSampler;

			///
			/// @brief Create a combined image sampler slot
			/// @note Operator `+` can be used create a combined image sampler
			///
			/// @param image Image view object
			/// @param sampler Sampler object
			/// @param layout_general Set to `true` if used under `General` image layout
			///
			CombinedImageSampler(vk::ImageView image, vk::Sampler sampler, bool layout_general = false) :
				image(image),
				sampler(sampler),
				layout_general(layout_general)
			{}

			///
			/// @brief Create a combined image sampler slot
			/// @note Operator `+` can be used create a combined image sampler
			///
			/// @param attachment Attachment view object
			/// @param sampler Sampler object
			/// @param layout_general Set to `true` if used under `General` image layout
			///
			CombinedImageSampler(
				const vulkan::AttachmentView& attachment,
				vk::Sampler sampler,
				bool layout_general = false
			) :
				image(attachment.view),
				sampler(sampler),
				layout_general(layout_general)
			{}

			///
			/// @brief Get write information
			///
			/// @param resource Memory resource
			/// @param info Target binding and set
			/// @return Write info
			///
			[[nodiscard]]
			detail::ImageWriteInfo write_info_fn(
				std::pmr::memory_resource& resource,
				const detail::WriteTarget& info
			) const noexcept;

		  private:

			vk::ImageView image;
			vk::Sampler sampler;
			bool layout_general;

		  public:

			CombinedImageSampler(const CombinedImageSampler&) = default;
			CombinedImageSampler(CombinedImageSampler&&) = default;
			CombinedImageSampler& operator=(const CombinedImageSampler&) = default;
			CombinedImageSampler& operator=(CombinedImageSampler&&) = default;
		};

		///
		/// @brief Sampled image slot
		///
		class SampledImage
		{
		  public:

			static constexpr auto TYPE = vk::DescriptorType::eSampledImage;

			///
			/// @brief Create a sampled image slot
			///
			/// @param image Image object
			/// @param layout_general Set to `true` if used under `General` image layout
			///
			SampledImage(vk::ImageView image, bool layout_general = false) :
				image(image),
				layout_general(layout_general)
			{}

			///
			/// @brief Create a sampled image slot
			///
			/// @param image Attachment view object
			/// @param layout_general Set to `true` if used under `General` image layout
			///
			SampledImage(const vulkan::AttachmentView& attachment, bool layout_general = false) :
				image(attachment.view),
				layout_general(layout_general)
			{}

			///
			/// @brief Get write information
			///
			/// @param resource Memory resource
			/// @param info Target binding and set
			/// @return Write info
			///
			[[nodiscard]]
			detail::ImageWriteInfo write_info_fn(
				std::pmr::memory_resource& resource,
				const detail::WriteTarget& info
			) const noexcept;

		  private:

			vk::ImageView image;
			bool layout_general;

		  public:

			SampledImage(const SampledImage&) = default;
			SampledImage(SampledImage&&) = default;
			SampledImage& operator=(const SampledImage&) = default;
			SampledImage& operator=(SampledImage&&) = default;
		};

		class Sampler
		{
		  public:

			static constexpr auto TYPE = vk::DescriptorType::eSampler;

			///
			/// @brief Create a sampler image slot
			///
			/// @param sampler Sampler object
			///
			Sampler(vk::Sampler sampler) :
				sampler(sampler)
			{}

			///
			/// @brief Get write information
			///
			/// @param resource Memory resource
			/// @param info Target binding and set
			/// @return Write info
			///
			[[nodiscard]]
			detail::ImageWriteInfo write_info_fn(
				std::pmr::memory_resource& resource,
				const detail::WriteTarget& info
			) const noexcept;

		  private:

			vk::Sampler sampler;

		  public:

			Sampler(const Sampler&) = default;
			Sampler(Sampler&&) = default;
			Sampler& operator=(const Sampler&) = default;
			Sampler& operator=(Sampler&&) = default;
		};

		///
		/// @brief Storage image slot
		/// @note Layout is assumed to be `General`
		///
		class StorageImage
		{
		  public:

			static constexpr auto TYPE = vk::DescriptorType::eStorageImage;

			///
			/// @brief Create a storage image slot
			///
			/// @param image Image view object
			///
			StorageImage(vk::ImageView image) :
				image(image)
			{}

			///
			/// @brief Create a storage image slot
			///
			/// @param image Image view object
			///
			StorageImage(const vulkan::AttachmentView& attachment) :
				image(attachment.view)
			{}

			///
			/// @brief Get write information
			///
			/// @param resource Memory resource
			/// @param info Target binding and set
			/// @return Write info
			///
			[[nodiscard]]
			detail::ImageWriteInfo write_info_fn(
				std::pmr::memory_resource& resource,
				const detail::WriteTarget& info
			) const noexcept;

		  private:

			vk::ImageView image;

		  public:

			StorageImage(const StorageImage&) = default;
			StorageImage(StorageImage&&) = default;
			StorageImage& operator=(const StorageImage&) = default;
			StorageImage& operator=(StorageImage&&) = default;
		};

		///
		/// @brief Uniform buffer slot
		///
		class UniformBuffer
		{
		  public:

			static constexpr auto TYPE = vk::DescriptorType::eUniformBuffer;

			///
			/// @brief Create a uniform buffer slot
			///
			/// @tparam T Type of the buffer, must be convertible to `vk::Buffer`
			/// @param buffer Buffer object
			/// @param offset Offset, defaults to `0`
			/// @param size Size, defaults to whole size
			///
			template <std::convertible_to<vk::Buffer> T>
			UniformBuffer(T buffer, vk::DeviceSize offset = 0, vk::DeviceSize size = vk::WholeSize) :
				buffer(static_cast<vk::Buffer>(buffer)),
				offset(offset),
				size(size)
			{}

			///
			/// @brief Get write information
			///
			/// @param resource Memory resource
			/// @param info Target binding and set
			/// @return Write info
			///
			[[nodiscard]]
			detail::BufferWriteInfo write_info_fn(
				std::pmr::memory_resource& resource,
				const detail::WriteTarget& info
			) const noexcept;

		  private:

			vk::Buffer buffer;
			vk::DeviceSize offset = 0;
			vk::DeviceSize size = vk::WholeSize;

		  public:

			UniformBuffer(const UniformBuffer&) = default;
			UniformBuffer(UniformBuffer&&) = default;
			UniformBuffer& operator=(const UniformBuffer&) = default;
			UniformBuffer& operator=(UniformBuffer&&) = default;
		};

		///
		/// @brief Storage buffer slot
		///
		class StorageBuffer
		{
		  public:

			static constexpr auto TYPE = vk::DescriptorType::eStorageBuffer;

			///
			/// @brief Create a storage buffer slot
			///
			/// @tparam T Type of the buffer, must be convertible to `vk::Buffer`
			/// @param buffer Buffer object
			/// @param offset Offset, defaults to `0`
			/// @param size Size, defaults to whole size
			///
			template <std::convertible_to<vk::Buffer> T>
			StorageBuffer(T buffer, vk::DeviceSize offset = 0, vk::DeviceSize size = vk::WholeSize) :
				buffer(static_cast<vk::Buffer>(buffer)),
				offset(offset),
				size(size)
			{}

			///
			/// @brief Get write information
			///
			/// @param resource Memory resource
			/// @param info Target binding and set
			/// @return Write info
			///
			[[nodiscard]]
			detail::BufferWriteInfo write_info_fn(
				std::pmr::memory_resource& resource,
				const detail::WriteTarget& info
			) const noexcept;

		  private:

			vk::Buffer buffer;
			vk::DeviceSize offset = 0;
			vk::DeviceSize size = vk::WholeSize;

		  public:

			StorageBuffer(const StorageBuffer&) = default;
			StorageBuffer(StorageBuffer&&) = default;
			StorageBuffer& operator=(const StorageBuffer&) = default;
			StorageBuffer& operator=(StorageBuffer&&) = default;
		};

		///
		/// @brief Acceleration structure slot
		///
		class AccelerationStructure
		{
		  public:

			static constexpr auto TYPE = vk::DescriptorType::eAccelerationStructureKHR;

			///
			/// @brief Create an acceleration structure slot
			///
			/// @param accel_struct Acceleration structure
			///
			AccelerationStructure(vk::AccelerationStructureKHR accel_struct) :
				accel_struct(accel_struct)
			{}

			///
			/// @brief Get write information
			///
			/// @param resource Memory resource
			/// @param info Target binding and set
			/// @return Write info
			///
			[[nodiscard]]
			detail::AccelerationStructureWriteInfo write_info_fn(
				std::pmr::memory_resource& resource,
				const detail::WriteTarget& info
			) const noexcept;

		  private:

			vk::AccelerationStructureKHR accel_struct;

		  public:

			AccelerationStructure(const AccelerationStructure&) = default;
			AccelerationStructure(AccelerationStructure&&) = default;
			AccelerationStructure& operator=(const AccelerationStructure&) = default;
			AccelerationStructure& operator=(AccelerationStructure&&) = default;
		};

		static_assert(detail::Slot<CombinedImageSampler>);
		static_assert(detail::Slot<SampledImage>);
		static_assert(detail::Slot<Sampler>);
		static_assert(detail::Slot<StorageImage>);
		static_assert(detail::Slot<UniformBuffer>);
		static_assert(detail::Slot<StorageBuffer>);
		static_assert(detail::Slot<AccelerationStructure>);
	}

	///
	/// @brief Combine an image and sampler as combined image sampler slot
	///
	/// @param image Image object
	/// @param sampler Sampler object
	/// @return Combined image sampler slot
	///
	inline slot::CombinedImageSampler operator+(vk::ImageView image, vk::Sampler sampler) noexcept
	{
		return {image, sampler};
	}

	///
	/// @brief Combine an attachment and sampler as combined image sampler slot
	///
	/// @param attachment Attachment object
	/// @param sampler Sampler object
	/// @return Combined image sampler slot
	///
	inline slot::CombinedImageSampler operator+(
		const vulkan::AttachmentView& attachment,
		vk::Sampler sampler
	) noexcept
	{
		return {attachment, sampler};
	}

	///
	/// @brief Base class of layout, provides aliases for slots
	/// @note Not required for use in sets
	///
	struct LayoutBase
	{
		using CombinedImageSampler = slot::CombinedImageSampler;
		using SampledImage = slot::SampledImage;
		using Sampler = slot::Sampler;
		using StorageImage = slot::StorageImage;
		using UniformBuffer = slot::UniformBuffer;
		using StorageBuffer = slot::StorageBuffer;
		using AccelerationStructure = slot::AccelerationStructure;
	};

	///
	/// @brief Mandates requirements for a struct/class to be accepted in layout
	///
	template <typename T>
	concept LayoutStruct = detail::LayoutStruct<T>;

	template <LayoutStruct T>
	class Layout;

	///
	/// @brief Trivial descriptor set. Can be created from `Layout<T>`
	///
	/// @tparam T Type of the layout struct
	///
	template <LayoutStruct T>
	class Set
	{
	  public:

		///
		/// @brief Update the set with given info
		///
		/// @param context Vulkan context
		/// @param info Update information of type `T`
		///
		void update(const vulkan::Context& context, const T& info) const noexcept;

		operator vk::DescriptorSet() const noexcept { return set; }
		vk::DescriptorSet operator*() const noexcept { return set; }

	  private:

		std::shared_ptr<vk::raii::DescriptorPool> pool;
		vk::raii::DescriptorSet set;

		explicit Set(std::shared_ptr<vk::raii::DescriptorPool> pool, vk::raii::DescriptorSet set) :
			pool(std::move(pool)),
			set(std::move(set))
		{}

		friend Layout<T>;

	  public:

		Set(const Set&) = delete;
		Set(Set&&) = default;
		Set& operator=(const Set&) = delete;
		Set& operator=(Set&&) = default;
	};

	///
	/// @brief Trivial descriptor set layout
	///
	/// @tparam T Type of the layout struct
	///
	template <LayoutStruct T>
	class Layout
	{
	  public:

		///
		/// @brief Create a layout object
		///
		/// @param context Vulkan context
		/// @return Created layout or error
		///
		[[nodiscard]]
		static std::expected<Layout, Error> create(const vulkan::Context& context) noexcept;

		///
		/// @brief Create descriptor sets from this layout
		///
		/// @param context Vulkan context
		/// @param count Number of sets to create
		/// @return List of created sets or error
		///
		[[nodiscard]]
		std::expected<std::vector<Set<T>>, Error> create_sets(
			const vulkan::Context& context,
			uint32_t count
		) const noexcept;

		operator vk::DescriptorSetLayout() const noexcept { return layout; }
		vk::DescriptorSetLayout operator*() const noexcept { return layout; }

	  private:

		vk::raii::DescriptorSetLayout layout;

		explicit Layout(vk::raii::DescriptorSetLayout layout) :
			layout(std::move(layout))
		{}

	  public:

		Layout(const Layout&) = delete;
		Layout(Layout&&) = default;
		Layout& operator=(const Layout&) = delete;
		Layout& operator=(Layout&&) = default;
	};

	template <LayoutStruct T>
	std::expected<Layout<T>, Error> Layout<T>::create(const vulkan::Context& context) noexcept
	{
		constexpr auto bindings = detail::get_binding_info_list<T>();
		const auto create_info = vk::DescriptorSetLayoutCreateInfo().setBindings(bindings);

		auto layout_result = context.device.createDescriptorSetLayout(create_info);
		if (!layout_result) return Error::from(layout_result);
		return Layout(std::move(*layout_result));
	}

	template <LayoutStruct T>
	std::expected<std::vector<Set<T>>, Error> Layout<T>::create_sets(
		const vulkan::Context& context,
		uint32_t count
	) const noexcept
	{
		constexpr auto bindings = detail::get_binding_info_list<T>();
		const auto pool_sizes = vulkan::calc_pool_sizes(bindings, count);

		auto descriptor_pool_result = context.device.createDescriptorPool(
			vk::DescriptorPoolCreateInfo()
				.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
				.setMaxSets(count)
				.setPoolSizes(pool_sizes)
		);
		if (!descriptor_pool_result) return Error::from(descriptor_pool_result);
		auto descriptor_pool = std::make_shared<vk::raii::DescriptorPool>(std::move(*descriptor_pool_result));

		const auto layouts = std::vector(count, *layout);
		const auto set_alloc_info =
			vk::DescriptorSetAllocateInfo().setDescriptorPool(*descriptor_pool).setSetLayouts(layouts);
		auto sets_result = context.device.allocateDescriptorSets(set_alloc_info);
		if (!sets_result) return Error::from(sets_result);
		auto sets = std::move(*sets_result);

		return std::views::zip_transform(
				   CTOR_LAMBDA(Set<T>),
				   std::views::repeat(descriptor_pool),
				   std::views::as_rvalue(sets)
			   )
			| std::ranges::to<std::vector>();
	}

	template <LayoutStruct T>
	void Set<T>::update(const vulkan::Context& context, const T& info) const noexcept
	{
		// TODO:
		//   Use update descriptor template
		//   https://docs.vulkan.org/refpages/latest/refpages/source/vkUpdateDescriptorSetWithTemplate.html

		alignas(std::max_align_t) std::array<std::byte, detail::AllocSizeOf<T> * 2> scratch_buffer;
		std::pmr::monotonic_buffer_resource scratch_resource(
			scratch_buffer.data(),
			detail::AllocSizeOf<T> * 2
		);

		// Note:
		// `write_info_objs` must stay alive until update. Update info references data from it.
		const auto write_info_objs =
			[this, &info, &scratch_resource]<size_t... Is>(std::integer_sequence<size_t, Is...>) {
				return std::make_tuple(
					(info.*(std::get<Is>(T::SLOT_LIST)))
						.write_info_fn(scratch_resource, detail::WriteTarget{.set = set, .binding = Is})...
				);
			}(std::make_integer_sequence<size_t, std::tuple_size_v<decltype(T::SLOT_LIST)>>());

		const auto write_infos = std::apply(
			[](const auto&... write_info_obj) { return std::to_array({std::invoke(write_info_obj)...}); },
			write_info_objs
		);

		context.device.updateDescriptorSets(write_infos, {});
	}
}
