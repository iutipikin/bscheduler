#ifndef FACTORY_KERNEL_ACT_HH
#define FACTORY_KERNEL_ACT_HH

#include <factory/kernel/kernel.hh>
#include <factory/ppl/basic_pipeline.hh>

namespace factory {

	inline void
	act(Kernel* kernel) {
		bool del = false;
		if (kernel->result() == exit_code::undefined) {
			if (kernel->principal()) {
				kernel->principal()->react(kernel);
				if (!kernel->isset(kernel_flag::do_not_delete)) {
					del = true;
				} else {
					kernel->unsetf(kernel_flag::do_not_delete);
				}
			} else {
				kernel->act();
			}
		} else {
			if (!kernel->principal()) {
				del = !kernel->parent();
			} else {
				del = *kernel->principal() == *kernel->parent();
				if (kernel->result() == exit_code::success) {
					kernel->principal()->react(kernel);
				} else {
					kernel->principal()->error(kernel);
				}
			}
		}
		if (del) {
			delete kernel;
		}
	}

}

#endif // vim:filetype=cpp
