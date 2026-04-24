#pragma once

#include <atomic>
#include <cassert>
#include <future>
#include <memory>

namespace util
{
	///
	/// @brief Progress class to track the progress of asynchronous operations
	///
	///
	class Progress
	{
	  public:

		///
		/// @brief Value representing the progress, consisting of total and current counts
		///
		///
		struct Value
		{
			size_t total;
			size_t current;
		};

	  private:

		std::shared_ptr<Value> progress;
		std::atomic_ref<size_t> total;
		std::atomic_ref<size_t> current;

	  public:

		///
		/// @brief Reference to the progress
		///
		///
		class Ref
		{
			std::shared_ptr<const Value> progress;
			std::atomic_ref<const size_t> total_ref;
			std::atomic_ref<const size_t> current_ref;
			std::atomic_ref<const Value> progress_ref;

			friend Progress;

			explicit Ref(std::shared_ptr<const Value> progress);

		  public:

			///
			/// @brief Get the current progress as a pair of (total, current).
			///
			/// @return A pair of (total, current) representing the progress.
			///
			[[nodiscard]]
			Value get() const noexcept;

			///
			/// @brief Get the current progress as a double in the range [0.0, 1.0].
			///
			/// @return A double representing the progress, where 0.0 means no progress and 1.0 means
			/// complete.
			///
			[[nodiscard]]
			double get_progress() const noexcept;

			Ref(const Ref&) = default;
			Ref(Ref&&) = default;
			Ref& operator=(const Ref&) = delete;
			Ref& operator=(Ref&&) = delete;
		};

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
		/// @brief Get a reference to current progress
		///
		/// @return A reference object that can be used to query the current progress.
		///
		[[nodiscard]]
		Ref get_ref() const noexcept;

		Progress(const Progress&) = delete;
		Progress(Progress&&) = default;
		Progress& operator=(const Progress&) = delete;
		Progress& operator=(Progress&&) = delete;
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
