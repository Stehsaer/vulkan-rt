#include "common/util/async.hpp"

namespace util
{
	Progress::Ref::Ref(std::shared_ptr<const Value> progress) :
		progress(std::move(progress)),
		total_ref(this->progress->total),
		current_ref(this->progress->current),
		progress_ref(*this->progress)
	{}

	Progress::Value Progress::Ref::get() const noexcept
	{
		return progress_ref.load();
	}

	double Progress::Ref::get_progress() const noexcept
	{
		const auto [total, current] = get();
		return total == 0 ? 0.0 : static_cast<double>(current) / static_cast<double>(total);
	}

	Progress::Progress() :
		progress(std::make_shared<Value>(0, 0)),
		total(progress->total),
		current(progress->current)
	{}

	void Progress::set_total(size_t val) const noexcept
	{
		total.store(val);
	}

	void Progress::increment(size_t val) const noexcept
	{
		current.fetch_add(val);
	}

	Progress::Ref Progress::get_ref() const noexcept
	{
		return Ref(progress);
	}
}
