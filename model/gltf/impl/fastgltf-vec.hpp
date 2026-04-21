#pragma once

#include <fastgltf/types.hpp>
#include <glm/glm.hpp>

namespace model::gltf::impl
{
	template <size_t N>
		requires(N >= 2 && N <= 4)
	[[nodiscard]]
	glm::vec<N, float> to_glm(fastgltf::math::vec<float, N> v)
	{
		if constexpr (N == 2)
			return glm::vec<N, float>(v[0], v[1]);
		else if constexpr (N == 3)
			return glm::vec<N, float>(v[0], v[1], v[2]);
		else if constexpr (N == 4)
			return glm::vec<N, float>(v[0], v[1], v[2], v[3]);
	}
}
