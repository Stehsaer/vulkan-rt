#include "common/util/async.hpp"
#include <atomic>

namespace util
{
	ProgressRef::ProgressRef(std::shared_ptr<const ProgressValue> progress) :
		progress(std::move(progress)),
		total_ref(this->progress->total),
		current_ref(this->progress->current)
	{}

	ProgressValue ProgressRef::get() const noexcept
	{
		return {
			.total = total_ref.load(std::memory_order_relaxed),
			.current = current_ref.load(std::memory_order_relaxed),
		};
	}

	std::optional<double> ProgressRef::get_progress() const noexcept
	{
		const auto [total, current] = get();

		if (total == 0)
			return std::nullopt;
		else
			return static_cast<double>(current) / static_cast<double>(total);
	}

	Progress::Progress() :
		progress(std::make_shared<ProgressValue>(0, 0)),
		total(progress->total),
		current(progress->current)
	{}

	void Progress::set_total(size_t val) const noexcept
	{
		total.store(val, std::memory_order_relaxed);
	}

	void Progress::increment(size_t val) const noexcept
	{
		current.fetch_add(val, std::memory_order_relaxed);
	}

	ProgressRef Progress::get_ref() const noexcept
	{
		return ProgressRef(progress);
	}
}
