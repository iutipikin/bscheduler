#ifndef FACTORY_PPL_SOCKET_PIPELINE_HH
#define FACTORY_PPL_SOCKET_PIPELINE_HH

#include <tuple>
#include <unordered_map>
#include <vector>
#include <cassert>

#include <unistdx/base/log_message>
#include <unistdx/it/field_iterator>
#include <unistdx/it/queue_popper>
#include <unistdx/net/endpoint>
#include <unistdx/net/ifaddr>

#include <factory/kernel/kernel_instance_registry.hh>
#include <factory/kernel/kstream.hh>
#include <factory/ppl/basic_socket_pipeline.hh>
#include <factory/ppl/local_server.hh>
#include <factory/ppl/remote_client.hh>

namespace factory {

	template<class T, class Socket, class Router>
	struct socket_pipeline:
		public basic_socket_pipeline<T,remote_client<T,Socket,Router>> {

		typedef Socket socket_type;
		typedef Router router_type;
		typedef sys::ipv4_addr addr_type;
		typedef sys::ifaddr<addr_type> ifaddr_type;
		typedef remote_client<T,Socket,Router> remote_client_type;
		typedef local_server<addr_type,socket_type> server_type;
		typedef basic_socket_pipeline<T,remote_client_type> base_pipeline;

		using typename base_pipeline::kernel_type;
		using typename base_pipeline::mutex_type;
		using typename base_pipeline::lock_type;
		using typename base_pipeline::sem_type;
		using typename base_pipeline::kernel_pool;
		using typename base_pipeline::event_handler_type;
		using typename base_pipeline::event_handler_ptr;
		using typename base_pipeline::duration;

		using base_pipeline::poller;
		using base_pipeline::send;

	private:
		typedef sys::ipaddr_traits<addr_type> traits_type;
		typedef std::vector<server_type> server_container_type;
		typedef typename server_container_type::iterator server_iterator;
		typedef typename server_container_type::const_iterator
			server_const_iterator;
		typedef std::unordered_map<sys::endpoint,event_handler_ptr>
			client_map_type;
		typedef typename client_map_type::iterator client_iterator;
		typedef ifaddr_type::rep_type rep_type;
		typedef mobile_kernel::id_type id_type;
		typedef sys::field_iterator<server_const_iterator,0> ifaddr_iterator;

	private:
		server_container_type _servers;
		client_map_type _clients;
		client_iterator _iterator;
		sys::port_type _port = 33333;
		std::chrono::milliseconds _socket_timeout = std::chrono::seconds(7);
		id_type _counter = 0;
		bool _uselocalhost = true;

	public:

		socket_pipeline() {
			using namespace std::chrono;
			this->set_start_timeout(seconds(7));
		}

		~socket_pipeline() = default;
		socket_pipeline(const socket_pipeline&) = delete;
		socket_pipeline(socket_pipeline&&) = delete;
		socket_pipeline& operator=(const socket_pipeline&) = delete;
		socket_pipeline& operator=(socket_pipeline&&) = delete;

		void
		remove_server(sys::fd_type fd) override {
			server_iterator result = this->find_server(fd);
			if (result != this->_servers.end()) {
				this->remove_server(result);
			}
		}

		void
		remove_client(event_handler_ptr ptr) override {
			client_iterator result = _clients.find(ptr->vaddr());
			if (result != this->_clients.end()) {
				this->remove_client(result);
			}
		}

		void accept_connection(sys::poll_event& ev) override {
			sys::endpoint addr;
			socket_type sock;
			server_iterator result = this->find_server(ev.fd());
			if (result == this->_servers.end()) {
				throw std::invalid_argument("no matching server found");
			}
			result->socket().accept(sock, addr);
			sys::endpoint vaddr = virtual_addr(addr);
			auto res = _clients.find(vaddr);
			if (res == _clients.end()) {
				#ifndef NDEBUG
				event_handler_ptr ptr =
				#endif
				add_connected_pipeline(std::move(sock), vaddr, sys::poll_event::In);
				#ifndef NDEBUG
				sys::log_message(this->_name, "accept _", *ptr);
				#endif
			} else {
				/*
				event_handler_ptr s = res->second;
				const sys::port_type local_port = s->socket().bind_addr().port();
				if (!(addr.port() < local_port)) {
					#ifndef NDEBUG
					sys::log_message(this->_name,  "replace _", s);
					#endif
					poller().disable(s->socket().fd());
					s->socket(std::move(sock));
					remove_client(res);
					_clients.emplace(vaddr, std::move(s));
					poller().emplace(sys::poll_event{s->socket().fd(), sys::poll_event::Inout, sys::poll_event::Inout}, s);
				}
				*/
			}
		}

		void
		add_client(const sys::endpoint& addr) {
			lock_type lock(this->_mutex);
			this->add_client(addr, sys::poll_event::In);
		}

		void
		add_server(const ifaddr_type& rhs) {
			this->add_server(
				sys::endpoint(rhs.address(), this->_port),
				rhs.netmask()
			);
		}

		void
		add_server(const sys::endpoint& rhs, addr_type netmask) {
			lock_type lock(this->_mutex);
			ifaddr_type ifaddr(traits_type::address(rhs), netmask);
			if (this->find_server(ifaddr) == this->_servers.end()) {
				this->_servers.emplace_back(ifaddr, traits_type::port(rhs));
				server_type& srv = this->_servers.back();
				srv.socket().set_user_timeout(this->_socket_timeout);
				sys::fd_type fd = srv.socket().get_fd();
				this->poller().insert_special(
					sys::poll_event{fd, sys::poll_event::In}
				);
				if (!this->is_stopped()) {
					this->_semaphore.notify_one();
				}
			}
		}

		void
		remove_server(const ifaddr_type& ifaddr) {
			lock_type lock(this->_mutex);
			server_iterator result = this->find_server(ifaddr);
			if (result != this->_servers.end()) {
				this->remove_server(result);
			}
		}

		void
		forward(const kernel_header& hdr, sys::pstream& istr) {
			lock_type lock(this->_mutex);
			this->log("fwd _", hdr);
			assert(hdr.is_foreign());
			if (hdr.to()) {
				event_handler_ptr ptr =
					this->find_or_create_peer(hdr.to(), sys::poll_event::Inout);
				ptr->forward(hdr, istr);
			} else {
				this->find_next_client();
				if (this->end_reached()) {
					this->reset_iterator();
					this->_mutex.unlock();
					router_type::forward_child(hdr, istr);
				} else {
					this->_iterator->second->forward(hdr, istr);
					++this->_iterator;
				}
			}
		}

		inline void
		set_port(sys::port_type rhs) noexcept {
			this->_port = rhs;
		}

		inline server_const_iterator
		servers_begin() const noexcept {
			return this->_servers.begin();
		}

		inline server_const_iterator
		servers_end() const noexcept {
			return this->_servers.end();
		}

		inline void
		use_localhost(bool b) noexcept {
			this->_uselocalhost = b;
		}

	private:

		void
		remove_client(client_iterator result) {
			#ifndef NDEBUG
			const char* reason =
				result->second->is_starting() ? "timed out" : "";
			sys::log_message(
				this->_name,
				"remove client _ (_)",
				result->first,
				reason
			);
			#endif
			if (result == this->_iterator) {
				++this->_iterator;
			}
			result->second->setstate(pipeline_state::stopped);
			this->_clients.erase(result);
		}

		void
		remove_server(server_iterator result) {
			#ifndef NDEBUG
			sys::log_message(this->_name, "remove server _", result->ifaddr());
			#endif
			this->_servers.erase(result);
		}

		server_iterator
		find_server(const ifaddr_type& ifaddr) {
			typedef typename server_container_type::value_type value_type;
			return std::find_if(
				this->_servers.begin(),
				this->_servers.end(),
				[&ifaddr] (const value_type& rhs) {
					return rhs.ifaddr() == ifaddr;
				}
			);
		}

		server_iterator
		find_server(sys::fd_type fd) {
			typedef typename server_container_type::value_type value_type;
			return std::find_if(
				this->_servers.begin(),
				this->_servers.end(),
				[fd] (const value_type& rhs) {
					return rhs.socket().get_fd() == fd;
				}
			);
		}

		server_iterator
		find_server(const sys::endpoint& dest) {
			typedef typename server_container_type::value_type value_type;
			return std::find_if(
				this->_servers.begin(),
				this->_servers.end(),
				[&dest] (const value_type& rhs) {
					return rhs.ifaddr().contains(dest.addr4());
				}
			);
		}

		void
		set_kernel_id(kernel_type* kernel, server_type& srv) {
			if (!kernel->has_id()) {
				kernel->set_id(srv.generate_id());
			}
		}

		void
		set_kernel_id(kernel_type* kernel, id_type& counter) {
			if (!kernel->has_id()) {
				kernel->set_id(++counter);
			}
		}

		void
		ensure_identity(kernel_type* kernel, const sys::endpoint& dest) {
			if (dest.family() == sys::family_type::unix) {
				this->set_kernel_id(kernel, this->_counter);
				this->set_kernel_id(kernel->parent(), this->_counter);
			} else {
				if (this->_servers.empty()) {
					kernel->return_to_parent(
						exit_code::no_upstream_servers_available
					);
				} else {
					server_iterator result = this->find_server(dest);
					if (result == this->_servers.end()) {
						result = this->_servers.begin();
					}
					this->set_kernel_id(kernel, *result);
					this->set_kernel_id(kernel->parent(), *result);
				}
			}
		}

		/// round robin over upstream hosts
		void
		find_next_client() {
			if (this->_clients.empty()) {
				return;
			}
			// skip stopped hosts
			client_iterator old_iterator = this->_iterator;
			do {
				if (this->_iterator == this->_clients.end()) {
					this->_iterator = this->_clients.begin();
				} else {
					++this->_iterator;
				}
				if (this->_iterator != this->_clients.end() &&
					this->_iterator->second->is_running()) {
					break;
				}
			} while (old_iterator != this->_iterator);
		}

		inline bool
		end_reached() const noexcept {
			return this->_iterator == this->_clients.end();
		}

		inline void
		reset_iterator() noexcept {
			this->_iterator = this->_clients.begin();
		}

		std::pair<client_iterator,bool>
		emplace_pipeline(const sys::endpoint& vaddr, event_handler_ptr&& s) {
			const bool save = !this->end_reached();
			sys::endpoint e;
			if (save) {
				e = this->_iterator->first;
			}
			auto result = this->_clients.emplace(vaddr, std::move(s));
			if (save) {
				this->_iterator = this->_clients.find(e);
			} else {
				this->_iterator = result.first;
			}
			return result;
		}

		inline sys::endpoint
		virtual_addr(const sys::endpoint& addr) const {
			return addr.family() == sys::family_type::unix
				? addr
				: sys::endpoint(addr, this->_port);
		}

		void
		process_kernels() override {
			lock_type lock(this->_mutex);
			std::for_each(
				sys::queue_popper(this->_kernels),
				sys::queue_popper_end(this->_kernels),
				[this] (kernel_type* rhs) { process_kernel(rhs); }
			);
		}

		void
		process_kernel(kernel_type* k) {
			// short circuit local server
			// TODO logic?
			if (k->to()) {
				server_iterator result = this->find_server(k->to());
				if (result != this->_servers.end()) {
					router_type::send_local(k);
				}
			}
			if (k->moves_everywhere()) {
				for (auto& pair : _clients) {
					pair.second->send(k);
				}
				// delete broadcast kernel
				delete k;
			} else if (k->moves_upstream() && k->to() == sys::endpoint()) {
				bool success = false;
				this->find_next_client();
				if (this->_uselocalhost) {
					if (this->end_reached()) {
						this->reset_iterator();
						// include localhost in round-robin
						// (short-circuit kernels when no upstream servers
						// are available)
						router_type::send_local(k);
					} else {
						success = true;
					}
				} else {
					if (this->_clients.empty()) {
						k->return_to_parent(exit_code::no_upstream_servers_available);
						router_type::send_local(k);
					} else if (this->end_reached()) {
						this->reset_iterator();
						success = true;
					} else {
						success = true;
					}
				}
				if (success) {
					ensure_identity(k, this->_iterator->second->vaddr());
					this->_iterator->second->send(k);
					++this->_iterator;
				}
			} else if (k->moves_downstream() and not k->from()) {
				// kernel @k was sent to local node
				// because no upstream servers had
				// been available
				router_type::send_local(k);
			} else {
				// create endpoint if necessary, and send kernel
				if (not k->to()) {
					k->to(k->from());
				}
				if (k->moves_somewhere()) {
					ensure_identity(k, k->to());
				}
				this->find_or_create_peer(k->to(), sys::poll_event::Inout)->send(k);
			}
		}

		event_handler_ptr
		find_or_create_peer(const sys::endpoint& addr, sys::poll_event::legacy_event ev) {
			event_handler_ptr ret;
			auto result = _clients.find(addr);
			if (result == _clients.end()) {
				ret = this->add_client(addr, ev);
			} else {
				ret = result->second;
			}
			return ret;
		}

		event_handler_ptr
		add_client(
			const sys::endpoint& addr,
			sys::poll_event::legacy_event events
		) {
			if (addr.family() == sys::family_type::unix) {
				socket_type s(sys::family_type::unix);
				s.setopt(socket_type::pass_credentials);
				s.connect(addr);
				return this->add_connected_pipeline(std::move(s), addr, events);
			} else {
				server_iterator result = this->find_server(addr);
				if (result == this->_servers.end()) {
					throw std::invalid_argument("no matching server found");
				}
				// bind to server address with ephemeral port
				sys::endpoint srv_addr(result->endpoint(), 0);
				return this->add_connected_pipeline(
					socket_type(srv_addr, addr),
					addr,
					events
				);
			}
		}

		event_handler_ptr
		add_connected_pipeline(socket_type&& sock, sys::endpoint vaddr,
			sys::poll_event::legacy_event events,
			sys::poll_event::legacy_event revents=0)
		{
			sys::fd_type fd = sock.fd();
			if (vaddr.family() != sys::family_type::unix) {
				sock.set_user_timeout(this->_socket_timeout);
			}
			event_handler_ptr s =
				std::make_shared<event_handler_type>(
					std::move(sock),
					vaddr
				);
			s->setstate(pipeline_state::starting);
			// s.setparent(this);
			auto result = emplace_pipeline(vaddr, std::move(s));
			this->poller().emplace(
				sys::poll_event{fd, events, revents},
				result.first->second
			);
			#ifndef NDEBUG
			sys::log_message(this->_name, "add _", *result.first->second);
			#endif
			if (!this->is_stopped()) {
				this->_semaphore.notify_one();
			}
			return result.first->second;
		}

	};

}

#endif // vim:filetype=cpp