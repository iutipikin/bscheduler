#ifndef BSCHEDULER_API_HH
#define BSCHEDULER_API_HH

#include <bscheduler/config.hh>
#include <bscheduler/ppl/basic_factory.hh>

namespace bsc {

	typedef BSCHEDULER_KERNEL_TYPE kernel;

	enum Target {
		Local,
		Remote,
		Child
	};

	template <Target t=Target::Local>
	inline void
	send(kernel* k) {
		factory.send(k);
	}

	template <>
	inline void
	send<Local>(kernel* k) {
		factory.send(k);
	}

	template <>
	inline void
	send<Remote>(kernel* k) {
		factory.send_remote(k);
	}

	template<Target target=Target::Local>
	void
	upstream(kernel* lhs, kernel* rhs) {
		rhs->parent(lhs);
		send<target>(rhs);
	}

	template<Target target=Target::Local>
	void
	commit(kernel* rhs, exit_code ret) {
		if (!rhs->parent()) {
			delete rhs;
			bsc::graceful_shutdown(static_cast<int>(ret));
		} else {
			rhs->return_to_parent(ret);
			send<target>(rhs);
		}
	}

	template<Target target=Target::Local>
	void
	commit(kernel* rhs) {
		exit_code ret = rhs->return_code();
		commit<target>(
			rhs,
			ret == exit_code::undefined ? exit_code::success :
			ret
		);
	}

	template<Target target=Target::Local>
	void
	send(kernel* lhs, kernel* rhs) {
		lhs->principal(rhs);
		send<target>(lhs);
	}

	struct factory_guard {

		inline
		factory_guard() {
			::bsc::factory.start();
		}

		inline
		~factory_guard() {
			::bsc::factory.stop();
			::bsc::factory.wait();
		}

	};

	template<class Pipeline>
	void
	upstream(Pipeline& ppl, kernel* lhs, kernel* rhs) {
		rhs->parent(lhs);
		ppl.send(rhs);
	}

}

#endif // vim:filetype=cpp
