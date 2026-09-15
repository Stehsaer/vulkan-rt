#pragma once

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <future>
#include <memory>
#include <optional>

namespace util
{
	///
	/// @brief Value representing the progress, consisting of total and current counts
	///
	///
	struct alignas(64) ProgressValue
	{
		size_t total;
		size_t current;
	};

	class ProgressRef;

	///
	/// @brief Progress producer
	/// @note It's not recommended to use this as observer. Use `ProgressRef` instead
	///
	class Progress
	{
	  public:

	  private:

		std::shared_ptr<ProgressValue> progress;
		std::atomic_ref<size_t> total;
		std::atomic_ref<size_t> current;

	  public:

		///
		/// @brief Initialize a new progress. Total and current counts are initialized to 0.
		///
		///
		explicit Progress();

		///
		/// @brief Set total count for the progress.
		///
		/// @param val The total count to be set
		///
		void set_total(size_t val) const noexcept;

		///
		/// @brief Increment the current count by 1.
		///
		/// @param val The value to increment the current count by (default is 1)
		///
		void increment(size_t val = 1) const noexcept;

		///
		/// @brief Get an observer to the current progress
		///
		/// @return An observer that observes the current progress
		///
		[[nodiscard]]
		ProgressRef get_ref() const noexcept;

		Progress(const Progress&) = delete;
		Progress(Progress&&) = default;
		Progress& operator=(const Progress&) = delete;
		Progress& operator=(Progress&&) = delete;
	};

	///
	/// @brief Progress observer
	///
	///
	class ProgressRef
	{
		std::shared_ptr<const ProgressValue> progress;
		std::atomic_ref<const size_t> total_ref;
		std::atomic_ref<const size_t> current_ref;

		friend Progress;

		explicit ProgressRef(std::shared_ptr<const ProgressValue> progress);

	  public:

		///
		/// @brief Get the current progress as a pair of (total, current).
		///
		/// @return A pair of (total, current) representing the progress.
		///
		[[nodiscard]]
		ProgressValue get() const noexcept;

		///
		/// @brief Get the current progress as a double in the range [0.0, 1.0].
		///
		/// @retval std::nullopt Indeterminate progress
		/// @retval double Progress value between `0.0` and `1.0`
		///
		[[nodiscard]]
		std::optional<double> get_progress() const noexcept;

		ProgressRef(const ProgressRef&) = default;
		ProgressRef(ProgressRef&&) = default;
		ProgressRef& operator=(const ProgressRef&) = delete;
		ProgressRef& operator=(ProgressRef&&) = delete;
	};

	///
	/// @brief A simple wrapper around std::future for easy checking of readiness and retrieval of results.
	///
	/// @tparam T Type of the result
	///
	template <typename T>
	class Future
	{
		std::future<T> future;

	  public:

		///
		/// @brief Construct a new Future object from a std::future.
		///
		/// @param future The std::future to wrap
		///
		explicit Future(std::future<T> future) :
			future(std::move(future))
		{}

		///
		/// @brief Block until the future is ready and retrieve the result.
		/// @note Check @p ready to avoid blocking
		/// @return The result of the future.
		///
		[[nodiscard]]
		T get() && noexcept
		{
			return std::move(future).get();
		}

		///
		/// @brief Check if the future is ready without blocking.
		///
		/// @return `true` if the future is ready, `false` otherwise.
		///
		[[nodiscard]]
		bool ready() const noexcept
		{
			return future.valid() && future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
		}

		Future(const Future&) = delete;
		Future(Future&&) = default;
		Future& operator=(const Future&) = delete;
		Future& operator=(Future&&) = default;
	};
}
