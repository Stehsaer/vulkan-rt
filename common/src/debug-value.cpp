#include "common/container/debug-value.hpp"
#include "common/container/debug-trace.hpp"

#include <cppcodec/base64_rfc4648.hpp>
#include <format>
#include <libassert/assert.hpp>
#include <string>

namespace debug::value
{
	std::string ByteStream::stringify() const noexcept
	{
		return cppcodec::base64_rfc4648::encode(
			reinterpret_cast<const char*>(binary_data.data()),
			binary_data.size()
		);
	}

	Trace Error::convert() const noexcept
	{
		return {
			"message"_key = String(err->message),
			"detail"_key = err->detail.transform([](auto str) { return String(std::move(str)); }),
			"trace"_key = err->trace
		};
	}

	[[nodiscard]]
	std::string String::stringify() const noexcept
	{
		const auto str = std::format("{:?}", value);
		ASSERT(str.length() >= 2);
		return str.substr(1, str.length() - 2);
	}
}
