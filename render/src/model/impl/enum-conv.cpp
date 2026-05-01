#include "model/enum-conv.hpp"

namespace render::impl
{
	vk::Filter as_filter(model::Filter filter) noexcept
	{
		switch (filter)
		{
		case model::Filter::Nearest:
			return vk::Filter::eNearest;
		case model::Filter::Linear:
			return vk::Filter::eLinear;
		default:
			UNREACHABLE("Invalid filter mode", filter);
		}
	}

	vk::SamplerMipmapMode as_mipmap_mode(model::Filter filter) noexcept
	{
		switch (filter)
		{
		case model::Filter::Nearest:
			return vk::SamplerMipmapMode::eNearest;
		case model::Filter::Linear:
			return vk::SamplerMipmapMode::eLinear;
		default:
			UNREACHABLE("Invalid filter mode", filter);
		}
	}

	vk::SamplerAddressMode as_address_mode(model::Wrap wrap) noexcept
	{
		switch (wrap)
		{
		case model::Wrap::Repeat:
			return vk::SamplerAddressMode::eRepeat;
		case model::Wrap::MirroredRepeat:
			return vk::SamplerAddressMode::eMirroredRepeat;
		case model::Wrap::ClampToEdge:
			return vk::SamplerAddressMode::eClampToEdge;
		default:
			UNREACHABLE("Invalid wrap mode", wrap);
		}
	}
}
