#pragma once

#include <iostream>  // IWYU pragma: keep
#include <print>     // IWYU pragma: keep

#define EXPECT_FAIL(arg)                                                                                     \
	if (!(arg).has_value())                                                                                  \
	{                                                                                                        \
		std::println(std::cout, "Expected Error: {0:msg}: {0:detail}", (arg).error().root());                \
	}                                                                                                        \
	REQUIRE_FALSE((arg).has_value());

#define EXPECT_SUCCESS(arg)                                                                                  \
	if (!(arg).has_value())                                                                                  \
	{                                                                                                        \
		std::println(std::cout, "Unexpected Error: {0:msg}: {0:detail}", (arg).error().root());              \
	}                                                                                                        \
	REQUIRE((arg).has_value());

#define CHECK_VEC4_EQ(val, x_comp, y_comp, z_comp, w_comp)                                                   \
	CHECK_EQ((val).x, x_comp);                                                                               \
	CHECK_EQ((val).y, y_comp);                                                                               \
	CHECK_EQ((val).z, z_comp);                                                                               \
	CHECK_EQ((val).w, w_comp);

#define CHECK_VEC4_EQ_ALT(val1, val2)                                                                        \
	CHECK_EQ((val1).x, (val2).x);                                                                            \
	CHECK_EQ((val1).y, (val2).y);                                                                            \
	CHECK_EQ((val1).z, (val2).z);                                                                            \
	CHECK_EQ((val1).w, (val2).w);

#define CHECK_VEC3_EQ(val, x_comp, y_comp, z_comp)                                                           \
	CHECK_EQ((val).x, x_comp);                                                                               \
	CHECK_EQ((val).y, y_comp);                                                                               \
	CHECK_EQ((val).z, z_comp);

#define CHECK_VEC3_EQ_ALT(val1, val2)                                                                        \
	CHECK_EQ((val1).x, (val2).x);                                                                            \
	CHECK_EQ((val1).y, (val2).y);                                                                            \
	CHECK_EQ((val1).z, (val2).z);

#define CHECK_VEC2_EQ(val, x_comp, y_comp)                                                                   \
	CHECK_EQ((val).x, x_comp);                                                                               \
	CHECK_EQ((val).y, y_comp);

#define CHECK_VEC2_EQ_ALT(val1, val2)                                                                        \
	CHECK_EQ((val1).x, (val2).x);                                                                            \
	CHECK_EQ((val1).y, (val2).y);

#define CHECK_VEC1_EQ CHECK_EQ

#define REQUIRE_VEC4_EQ(val, x_comp, y_comp, z_comp, w_comp)                                                 \
	REQUIRE_EQ((val).x, x_comp);                                                                             \
	REQUIRE_EQ((val).y, y_comp);                                                                             \
	REQUIRE_EQ((val).z, z_comp);                                                                             \
	REQUIRE_EQ((val).w, w_comp);

#define REQUIRE_VEC4_EQ_ALT(val1, val2)                                                                      \
	REQUIRE_EQ((val1).x, (val2).x);                                                                          \
	REQUIRE_EQ((val1).y, (val2).y);                                                                          \
	REQUIRE_EQ((val1).z, (val2).z);                                                                          \
	REQUIRE_EQ((val1).w, (val2).w);

#define REQUIRE_VEC3_EQ(val, x_comp, y_comp, z_comp)                                                         \
	REQUIRE_EQ((val).x, x_comp);                                                                             \
	REQUIRE_EQ((val).y, y_comp);                                                                             \
	REQUIRE_EQ((val).z, z_comp);

#define REQUIRE_VEC3_EQ_ALT(val1, val2)                                                                      \
	REQUIRE_EQ((val1).x, (val2).x);                                                                          \
	REQUIRE_EQ((val1).y, (val2).y);                                                                          \
	REQUIRE_EQ((val1).z, (val2).z);

#define REQUIRE_VEC2_EQ(val, x_comp, y_comp)                                                                 \
	REQUIRE_EQ((val).x, x_comp);                                                                             \
	REQUIRE_EQ((val).y, y_comp);

#define REQUIRE_VEC2_EQ_ALT(val1, val2)                                                                      \
	REQUIRE_EQ((val1).x, (val2).x);                                                                          \
	REQUIRE_EQ((val1).y, (val2).y);

#define REQUIRE_VEC1_EQ REQUIRE_EQ
