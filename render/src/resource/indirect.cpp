#include "render/resource/indirect.hpp"
#include "common/util/error.hpp"
#include "render/util/per-render-state.hpp"
#include "vulkan/interface/context.hpp"

#include <cstddef>
#include <expected>
#include <ranges>

namespace render
{
	std::expected<void, Error> IndirectResource::resize(
		const vulkan::Context& context,
		render::PerRenderState<size_t> drawcall_counts
	) noexcept
	{
		for (
			const auto& [buffer, size] :
			std::views::zip(indirect_drawcall_buffers.all(), drawcall_counts.all())
		)
		{
			if (const auto result = buffer.resize(context, size); !result)
				return result.error().forward("Resize indirect buffer failed");
		}

		return {};
	}
}
