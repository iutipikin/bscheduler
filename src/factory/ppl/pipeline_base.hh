#ifndef FACTORY_PPL_PIPELINE_BASE_HH
#define FACTORY_PPL_PIPELINE_BASE_HH

#include <chrono>

namespace factory {

	enum struct pipeline_state {
		initial,
		starting,
		started,
		stopping,
		stopped
	};

	class pipeline_base {

	public:
		typedef std::chrono::system_clock clock_type;
		typedef clock_type::time_point time_point;
		typedef clock_type::duration duration;

	protected:
		volatile pipeline_state _state = pipeline_state::initial;
		time_point _start;

	public:
		pipeline_base() = default;
		virtual ~pipeline_base() = default;
		pipeline_base(pipeline_base&&) = default;
		pipeline_base(const pipeline_base&) = delete;
		pipeline_base& operator=(pipeline_base&) = delete;

		inline void
		setstate(pipeline_state rhs) noexcept {
			this->_state = rhs;
			if (rhs == pipeline_state::starting) {
				this->_start = clock_type::now();
			}
		}

		inline pipeline_state
		state() const noexcept {
			return this->_state;
		}

		inline bool
		is_starting() const noexcept {
			return this->_state == pipeline_state::starting;
		}

		inline bool
		is_started() const noexcept {
			return this->_state == pipeline_state::started;
		}

		inline bool
		is_stopping() const noexcept {
			return this->_state == pipeline_state::stopping;
		}

		inline bool
		is_stopped() const noexcept {
			return this->_state == pipeline_state::stopped;
		}

		inline time_point
		start_time_point() const noexcept {
			return this->_start;
		}

	};

}

#endif // vim:filetype=cpp
