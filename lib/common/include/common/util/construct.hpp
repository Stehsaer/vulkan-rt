#pragma once

#define CTOR_LAMBDA(type)                                                                                    \
	[](auto&&... args) {                                                                                     \
		return type(std::forward<decltype(args)>(args)...);                                                  \
	}
