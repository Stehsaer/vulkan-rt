#pragma once

#include "render/interface/primitive-drawcall.hpp"
#include "render/model/model.hpp"
#include "render/util/per-render-state.hpp"

#include <memory>
#include <memory_resource>

namespace logic
{
	///
	/// @brief Drawcall generator
	/// @details
	/// - Generate drawcalls for different render modes based on model instance and transform matrix
	/// - All return vectors uses polymorphic allocators
	///
	class DrawcallGenerator
	{
	  public:

		struct Result
		{
			render::PerRenderState<std::pmr::vector<render::PrimitiveDrawcall>> drawcalls;
			std::pmr::vector<glm::mat4> transforms;
		};

		///
		/// @brief Execute the computation (Pure CPU side)
		///
		/// @param model Model instance
		/// @param model_transform Transform of the root node
		/// @return Drawcall data
		///
		Result compute(const render::Model& model, glm::mat4 model_transform) noexcept;

		DrawcallGenerator() :
			memory_resource(std::make_unique<std::pmr::unsynchronized_pool_resource>())
		{}

	  private:

		std::unique_ptr<std::pmr::unsynchronized_pool_resource> memory_resource;

	  public:

		DrawcallGenerator(const DrawcallGenerator&) = delete;
		DrawcallGenerator(DrawcallGenerator&&) = default;
		DrawcallGenerator& operator=(const DrawcallGenerator&) = delete;
		DrawcallGenerator& operator=(DrawcallGenerator&&) = default;
	};
}
