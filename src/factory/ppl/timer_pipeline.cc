#include "timer_pipeline.hh"
#include "config.hh"

template <class T>
void
factory::timer_pipeline<T>::do_run() {
	while (!this->is_stopped()) {
		lock_type lock(this->_mutex);
		this->wait_until_kernel_arrives(lock);
		if (!this->is_stopped()) {
			kernel_type* kernel = traits_type::front(this->_kernels);
			if (!this->wait_until_kernel_is_ready(lock, kernel)) {
				traits_type::pop(this->_kernels);
				lock.unlock();
				::factory::act(kernel);
			}
		}
	}
}

template class factory::timer_pipeline<FACTORY_KERNEL_TYPE>;