#include "render/resource/indirect.hpp"
#include <ranges>

namespace render
{
	std::expected<void, Error> IndirectResource::resize(
		const vulkan::DeviceContext& context,
		render::PerRenderState<size_t> drawcall_counts
	) noexcept
	{
		for (const auto& [buffer, size] :
			 std::views::zip(indirect_drawcall_buffers.all(), drawcall_counts.all()))
		{
			if (const auto result = buffer.resize(context, size); !result)
				return result.error().forward("Resize indirect buffer failed");
		}

		return {};
	}
}
