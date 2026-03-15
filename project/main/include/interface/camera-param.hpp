#pragma once

#include "vulkan/alloc.hpp"
#include <glm/glm.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace interface
{
	struct CameraParam
	{
		glm::mat4 view_projection;
		glm::vec3 position;

		struct Layout
		{
			vk::raii::DescriptorSetLayout layout;

			[[nodiscard]]
			static std::expected<Layout, Error> create(const vk::raii::Device& device) noexcept;
		};

		struct Resource
		{
			vulkan::alloc::Buffer staging_buffer;
			vulkan::alloc::Buffer buffer;

			[[nodiscard]]
			static std::expected<Resource, Error> create(
				const vk::raii::Device& device,
				const vulkan::Allocator& allocator
			) noexcept;

			[[nodiscard]]
			std::expected<void, Error> update(const CameraParam& param) const noexcept;

			void upload(const vk::raii::CommandBuffer& command_buffer) const noexcept;

			[[nodiscard]]
			vk::BufferMemoryBarrier2 barrier_after_upload() const noexcept;
		};

		struct DescriptorSet
		{
			vk::DescriptorSet descriptor_set;

			void bind_resource(const vk::raii::Device& device, const Resource& resource) const noexcept;
		};

		class DescriptorPool
		{
			vk::raii::DescriptorPool pool;
			std::vector<vk::raii::DescriptorSet> sets;

			explicit DescriptorPool(
				vk::raii::DescriptorPool pool,
				std::vector<vk::raii::DescriptorSet> sets
			) :
				pool(std::move(pool)),
				sets(std::move(sets))
			{}

		  public:

			[[nodiscard]]
			static std::expected<DescriptorPool, Error> create(
				const vk::raii::Device& device,
				const Layout& layout,
				uint32_t set_count
			) noexcept;

			[[nodiscard]]
			std::vector<DescriptorSet> get_descriptor_sets() const;

			DescriptorPool(const DescriptorPool&) = delete;
			DescriptorPool(DescriptorPool&&) = default;
			DescriptorPool& operator=(const DescriptorPool&) = delete;
			DescriptorPool& operator=(DescriptorPool&&) = default;
		};
	};
}
