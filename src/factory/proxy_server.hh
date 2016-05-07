#ifndef FACTORY_PROXY_SERVER_HH
#define FACTORY_PROXY_SERVER_HH

#include <stdx/algorithm.hh>
#include <stdx/iterator.hh>
#include <stdx/mutex.hh>

#include <sys/event.hh>
#include <sys/fildesbuf.hh>
#include <sys/packetstream.hh>

#include <factory/bits/server_category.hh>
#include <factory/kernelbuf.hh>
#include <factory/kernel_stream.hh>

namespace factory {

	template<class T, class Rserver,
	class Kernels=std::queue<T*>,
	class Threads=std::vector<std::thread>>
	using Proxy_server_base = Server_with_pool<T, Kernels, Threads,
		stdx::spin_mutex, stdx::simple_lock<stdx::spin_mutex>,
		sys::event_poller<Rserver*>>;

	template<class T, class Rserver>
	struct Proxy_server: public Proxy_server_base<T,Rserver> {

		typedef Rserver server_type;
		typedef stdx::log<Proxy_server> this_log;
		typedef server_type* handler_type;

		typedef Proxy_server_base<T,Rserver> base_server;
		using typename base_server::kernel_type;
		using typename base_server::mutex_type;
		using typename base_server::lock_type;
		using typename base_server::sem_type;
		using typename base_server::kernel_pool;

		Proxy_server(Proxy_server&& rhs) noexcept:
		base_server(std::move(rhs))
		{}

		Proxy_server():
		base_server(1u)
		{}

		~Proxy_server() = default;
		Proxy_server(const Proxy_server&) = delete;
		Proxy_server& operator=(const Proxy_server&) = delete;

	protected:

		sem_type&
		poller() {
			return this->_semaphore;
		}

		const sem_type&
		poller() const {
			return this->_semaphore;
		}

		void
		do_run() override {
			// start processing as early as possible
			poller().notify_one();
			lock_type lock(this->_mutex);
			while (!this->is_stopped()) {
				prepare_poll_events();
				poller().wait(lock);
				stdx::unlock_guard<lock_type> g(lock);
				process_kernels_if_any();
				accept_connections_if_any();
				handle_events();
				remove_servers_if_any();
			}
			// prevent double free or corruption
			poller().clear();
		}

		virtual void
		remove_server(server_type* ptr) = 0;

		virtual void
		process_kernels() = 0;

		virtual void
		accept_connection(sys::poll_event&) {}

		void
		run(Global_thread_context*) override {
			do_run();
			/**
			do nothing with the context, because we can not
			reliably garbage collect kernels sent from other nodes
			*/
		}

	private:

		void
		prepare_poll_events() {
			poller().for_each_ordinary_fd(
				[this] (sys::poll_event& ev, handler_type& h) {
					h->prepare(ev);
				}
			);
		}

		void
		remove_servers_if_any() {
			poller().for_each_ordinary_fd(
				[this] (sys::poll_event& ev, handler_type& h) {
					if (!ev) {
						this->remove_server(h);
					}
				}
			);
		}

		void
		process_kernels_if_any() {
			if (this->is_stopped()) {
				this->try_to_stop_gracefully();
			} else {
				poller().for_each_pipe_fd([this] (sys::poll_event& ev) {
					if (ev.in()) {
						this->process_kernels();
					}
				});
			}
		}

		void
		accept_connections_if_any() {
			poller().for_each_special_fd([this] (sys::poll_event& ev) {
				if (ev.in()) {
					this->accept_connection(ev);
				}
			});
		}

		void
		handle_events() {
			poller().for_each_ordinary_fd(
				[this] (sys::poll_event& ev, handler_type& h) {
					h->handle(ev);
				}
			);
		}

		void try_to_stop_gracefully() {
			this->process_kernels();
			// TODO check if this needed at all
			//this->flush_kernels();
			++_stop_iterations;
		}

		//void
		//flush_kernels() {
		//	typedef typename upstream_type::value_type pair_type;
		//	std::for_each(_upstream.begin(), _upstream.end(),
		//		[] (pair_type& rhs) {
		//			rhs.second.handle(sys::poll_event::Out);
		//		}
		//	);
		//}

	protected:

		int _stop_iterations = 0;
		static const int MAX_STOP_ITERATIONS = 13;
	};

}

#endif // FACTORY_PROXY_SERVER_HH
