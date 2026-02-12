#pragma once

#include "common/util/error.hpp"
#include "vulkan/alloc.hpp"
#include "vulkan/context.hpp"

#include <glm/glm.hpp>

struct Vertex
{
	glm::vec3 position;
	glm::vec2 uv;
	glm::vec3 normal;
};

struct Model
{
	std::vector<uint32_t> indices;
	std::vector<Vertex> vertices;

	static std::expected<Model, Error> load_from_file(const std::string_view& path) noexcept;
};

struct ModelBuffer
{
	vulkan::alloc::Buffer vertex_buffer, index_buffer;
	uint32_t vertex_count;

	static ModelBuffer create(const vulkan::Context& context, const Model& model);
};