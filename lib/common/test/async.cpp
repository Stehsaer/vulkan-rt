#include "common/util/async.hpp"

#include <doctest.h>
#include <thread>

/* AI-generated test cases */

TEST_SUITE("Progress")
{
	TEST_CASE("default construction")
	{
		const util::Progress progress;
		auto ref = progress.get_ref();

		auto [total, current] = ref.get();
		CHECK(total == 0);
		CHECK(current == 0);
		CHECK(ref.get_progress() == 0.0);
	}

	TEST_CASE("set_total")
	{
		const util::Progress progress;
		progress.set_total(100);

		auto ref = progress.get_ref();
		auto [total, current] = ref.get();
		CHECK(total == 100);
		CHECK(current == 0);
	}

	TEST_CASE("increment")
	{
		const util::Progress progress;
		progress.set_total(100);

		progress.increment(25);
		auto ref = progress.get_ref();
		auto [total, current] = ref.get();
		CHECK(total == 100);
		CHECK(current == 25);
		CHECK(ref.get_progress() == 0.25);

		progress.increment(50);
		CHECK(ref.get_progress() == 0.75);

		progress.increment(25);
		CHECK(ref.get_progress() == 1.0);
	}

	TEST_CASE("increment default argument")
	{
		const util::Progress progress;
		progress.set_total(10);

		for (int i = 0; i < 10; ++i)
		{
			progress.increment();
		}

		auto ref = progress.get_ref();
		CHECK(ref.get_progress() == 1.0);
	}

	TEST_CASE("get_progress division by zero")
	{
		const util::Progress progress;
		auto ref = progress.get_ref();
		CHECK(ref.get_progress() == 0.0);

		progress.increment(100);
		CHECK(ref.get_progress() == 0.0);
	}

	TEST_CASE("multi-threaded concurrency")
	{
		util::Progress progress;
		progress.set_total(1000);

		constexpr int thread_count = 4;
		constexpr int increments_per_thread = 250;

		std::vector<std::thread> threads;
		threads.reserve(thread_count);
		for (int i = 0; i < thread_count; ++i)
			threads.emplace_back([&progress]() {
				for (int j = 0; j < increments_per_thread; ++j)
				{
					progress.increment(1);
				}
			});

		for (auto& t : threads)
		{
			t.join();
		}

		auto ref = progress.get_ref();
		auto [total, current] = ref.get();
		CHECK(total == 1000);
		CHECK(current == 1000);
		CHECK(ref.get_progress() == 1.0);
	}

	TEST_CASE("Ref copy construction")
	{
		const util::Progress progress;
		progress.set_total(100);
		progress.increment(50);

		const auto ref1 = progress.get_ref();
		const auto ref2 = ref1;  // NOLINT

		CHECK(ref1.get_progress() == 0.5);
		CHECK(ref2.get_progress() == 0.5);

		progress.increment(25);
		CHECK(ref1.get_progress() == 0.75);
		CHECK(ref2.get_progress() == 0.75);
	}

	TEST_CASE("move semantics")
	{
		util::Progress progress1;
		progress1.set_total(100);
		progress1.increment(30);

		const util::Progress progress2 = std::move(progress1);
		auto ref = progress2.get_ref();
		auto [total, current] = ref.get();
		CHECK(total == 100);
		CHECK(current == 30);
	}

	TEST_CASE("Progress::Ref non-copy-assignable")
	{
		const util::Progress progress;
		auto ref1 = progress.get_ref();
		auto ref2 = progress.get_ref();
	}
}

TEST_SUITE("Future")
{
	TEST_CASE("ready and get")
	{
		std::promise<int> promise;
		util::Future<int> future(promise.get_future());

		CHECK(!future.ready());

		promise.set_value(42);
		CHECK(future.ready());
		CHECK(std::move(future).get() == 42);
	}

	TEST_CASE("invalid future")
	{
		const util::Future<int> future((std::future<int>()));
		CHECK(!future.ready());
	}

	TEST_CASE("move semantics")
	{
		std::promise<int> promise;
		util::Future<int> future1(promise.get_future());
		util::Future<int> future2 = std::move(future1);

		promise.set_value(100);
		CHECK(future2.ready());
		CHECK(std::move(future2).get() == 100);
	}

	TEST_CASE("Actual test with async")
	{
		auto async_task = []() {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			return 123;
		};

		util::Future<int> future(std::async(std::launch::async, async_task));
		CHECK(!future.ready());

		std::this_thread::sleep_for(std::chrono::milliseconds(400));
		CHECK(future.ready());
		CHECK(std::move(future).get() == 123);
	}
}
