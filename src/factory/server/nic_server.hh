#ifndef FACTORY_SERVER_NIC_SERVER_HH
#define FACTORY_SERVER_NIC_SERVER_HH

#include <map>
#include <type_traits>

#include <stdx/for_each.hh>
#include <stdx/field_iterator.hh>
#include <stdx/front_popper.hh>
#include <stdx/unlock_guard.hh>

#include <sysx/event.hh>
#include <sysx/socket.hh>
#include <sysx/packetstream.hh>

#include <factory/server/intro.hh>
#include <factory/server/proxy_server.hh>
#include <sysx/fildesbuf.hh>
#include <factory/kernelbuf.hh>
#include <factory/kernel_stream.hh>

namespace factory {

	namespace components {

		template<class T, class Socket, class Kernels=std::deque<typename Server<T>::kernel_type*>>
		struct Remote_Rserver: public Managed_object<Server<T>> {

			typedef Managed_object<Server<T>> base_server;
			using typename base_server::kernel_type;
			using typename base_server::factory_type;
			typedef char Ch;
			typedef basic_kernelbuf<sysx::basic_fildesbuf<Ch, std::char_traits<Ch>, sysx::socket>> Kernelbuf;
			typedef Kernel_stream<kernel_type> stream_type;
			typedef Server<T> server_type;
			typedef Socket socket_type;
			typedef Kernels pool_type;
			typedef typename kernel_type::app_type app_type;
			typedef stdx::log<Remote_Rserver> this_log;

			static_assert(
				std::is_move_constructible<stream_type>::value,
				"bad stream_type"
			);

			Remote_Rserver() = default;

			Remote_Rserver(socket_type&& sock, sysx::endpoint vaddr, factory_type* factory):
			_vaddr(vaddr),
			_packetbuf(),
			_stream(&_packetbuf),
			_sentupstream()
			{
				_stream.setapp(factory->app());
				_stream.settypes(&factory->types());
				_stream.setforward(
					[this] (app_type app, stream_type&) {
						this->app_server()->forward(app, _vaddr, _packetbuf);
					}
				);
				_packetbuf.setfd(std::move(sock));
			}

			Remote_Rserver(const Remote_Rserver&) = delete;
			Remote_Rserver& operator=(const Remote_Rserver&) = delete;

			Remote_Rserver(Remote_Rserver&& rhs):
			base_server(std::move(rhs)),
			_vaddr(rhs._vaddr),
			_packetbuf(std::move(rhs._packetbuf)),
			_stream(std::move(rhs._stream)),
			_sentupstream(std::move(rhs._sentupstream))
			{
				_stream.rdbuf(&_packetbuf);
			}

			virtual
			~Remote_Rserver() {
				this->recover_kernels();
				this->delete_kernels();
			}

			Category
			category() const noexcept override {
				return Category{
					"nic_rserver",
					[] () { return nullptr; }
				};
			}

			void
			recover_kernels() {

				// Here failed kernels are written to buffer,
				// from which they must be recovered with recover_kernels().
				sysx::poll_event ev{socket().fd(), sysx::poll_event::In};
				handle(ev);

				// recover kernels from upstream and downstream buffer
				do_recover_kernels(_sentupstream);
				if (socket().error()) {
					do_recover_kernels(_sentdownstream);
				}
			}

			void
			send(kernel_type* kernel) override {
				bool delete_kernel = false;
				if (kernel_goes_in_upstream_buffer(kernel)) {
					ensure_identity(kernel);
					_sentupstream.push_back(kernel);
				} else
				if (kernel_goes_in_downstream_buffer(kernel)) {
					std::clog << "Put kernel into downstream buffer" << std::endl;
					_sentdownstream.push_back(kernel);
				} else
				if (not kernel->moves_everywhere()) {
					delete_kernel = true;
				}
				_stream << kernel;
				/// The kernel is deleted if it goes downstream
				/// and does not carry its parent.
				if (delete_kernel) {
					delete kernel;
				}
			}

			void
			handle(sysx::poll_event& event) {
				// TODO: It is probably too slow to check error on every event.
				if (socket().error() != 0) {
					event.setrev(sysx::poll_event::Hup);
				}
				if (_packetbuf.dirty()) {
					event.setrev(sysx::poll_event::Out);
				}
				if (event.in()) {
					_stream.clear();
					_stream.fill();
					kernel_type* kernel = nullptr;
					while (_stream >> kernel) {
						receive_kernel(kernel);
					}
					this_log() << "finished receiving kernels: " << _stream.rdstate() << std::endl;
				}
				if (event.out() && !event.hup()) {
					_stream.flush();
				}
			}

			void
			prepare(sysx::poll_event& event) {
				if (_packetbuf.dirty()) {
					event.setev(sysx::poll_event::Out);
				} else {
					event.unsetev(sysx::poll_event::Out);
				}
			}

			const socket_type&
			socket() const {
				return _packetbuf.fd();
			}

			socket_type&
			socket() {
				return _packetbuf.fd();
			}

			void
			socket(sysx::socket&& rhs) {
				_packetbuf.pubfill();
				_packetbuf.setfd(socket_type(std::move(rhs)));
			}

			const sysx::endpoint& vaddr() const { return _vaddr; }
			void setvaddr(const sysx::endpoint& rhs) { _vaddr = rhs; }

			friend std::ostream&
			operator<<(std::ostream& out, const Remote_Rserver& rhs) {
				return out << stdx::make_fields(
					"vaddr", rhs.vaddr(),
					"socket", rhs.socket(),
					"kernels", rhs._sentupstream.size(),
					"stream", stdx::debug_stream(rhs._stream)
				);
			}

		private:

			void
			do_recover_kernels(pool_type& rhs) noexcept {
				using namespace std::placeholders;
				std::for_each(
					stdx::front_popper(rhs),
					stdx::front_popper_end(rhs),
					std::bind(&Remote_Rserver::recover_kernel, this, _1)
				);
			}

			void
			delete_kernels() {
				do_delete_kernels(_sentupstream);
				do_delete_kernels(_sentdownstream);
			}

			void
			do_delete_kernels(pool_type& rhs) noexcept {
				stdx::delete_each(
					stdx::front_popper(rhs),
					stdx::front_popper_end(rhs)
				);
			}

			void
			ensure_identity(kernel_type* kernel) {
				if (not kernel->identifiable()) {
					kernel->set_id(this->factory()->factory_generate_id());
				}
			}

			static bool
			kernel_goes_in_upstream_buffer(const kernel_type* rhs) noexcept {
				return rhs->moves_upstream() or rhs->moves_somewhere();
			}

			static bool
			kernel_goes_in_downstream_buffer(const kernel_type* rhs) noexcept {
				return rhs->moves_downstream() and rhs->carries_parent();
			}

			void
			receive_kernel(kernel_type* k) {
				bool ok = true;
				k->from(_vaddr);
				if (k->moves_downstream()) {
					this->clear_kernel_buffer(k);
				} else if (k->principal_id()) {
					kernel_type* p = this->factory()->instances().lookup(k->principal_id());
					if (p == nullptr) {
						k->result(Result::NO_PRINCIPAL_FOUND);
						ok = false;
					}
					k->principal(p);
				}
				this_log() << "recv kernel=" << *k << std::endl;
				if (!ok) {
					return_kernel(k);
				} else {
					this->root()->send(k);
				}
			}

			void return_kernel(kernel_type* k) {
				this_log() << "No principal found for " << *k << std::endl;
				k->principal(k->parent());
				this->send(k);
			}

			void recover_kernel(kernel_type* k) {
				if (k->moves_upstream()) {
					this->parent()->send(k);
				} else if (k->moves_somewhere()) {
					k->from(k->to());
					k->result(Result::ENDPOINT_NOT_CONNECTED);
					k->principal(k->parent());
					this->root()->send(k);
				} else if (k->moves_downstream() and k->carries_parent()) {
					std::clog << "Reviving parent kernel on a subordinate node" << std::endl;
					this->root()->send(k);
				} else {
					assert(false and "Bad kernel in sent buffer");
				}
			}

			void clear_kernel_buffer(kernel_type* k) {
				auto pos = std::find_if(
					_sentupstream.begin(),
					_sentupstream.end(),
					[k] (kernel_type* rhs) { return *rhs == *k; }
				);
				if (pos != _sentupstream.end()) {
					kernel_type* orig = *pos;
					k->parent(orig->parent());
					k->principal(k->parent());
					delete orig;
					_sentupstream.erase(pos);
				}
			}

			sysx::endpoint _vaddr;
			Kernelbuf _packetbuf;
			stream_type _stream;
			pool_type _sentupstream;
			pool_type _sentdownstream;
		};

		template<class T, class Socket>
		struct NIC_server: public Proxy_server<T,Remote_Rserver<T,Socket>> {

			typedef Socket socket_type;

			typedef Proxy_server<T,Remote_Rserver<T,Socket>> base_server;
			using typename base_server::kernel_type;
			using typename base_server::mutex_type;
			using typename base_server::lock_type;
			using typename base_server::sem_type;
			using typename base_server::kernel_pool;
			using typename base_server::server_type;

			using base_server::poller;

			typedef std::map<sysx::endpoint,server_type> upstream_type;
			typedef typename upstream_type::iterator iterator_type;
			typedef server_type* handler_type;
			typedef sysx::event_poller<handler_type> poller_type;
			typedef stdx::log<NIC_server> this_log;

			static_assert(
				std::is_move_constructible<server_type>::value,
				"bad server_type"
			);

			NIC_server(NIC_server&& rhs) noexcept:
			base_server(std::move(rhs)),
			_upstream(),
			_iterator(_upstream.end())
			{}

			NIC_server() = default;
			~NIC_server() = default;
			NIC_server(const NIC_server&) = delete;
			NIC_server& operator=(const NIC_server&) = delete;

			void remove_server(server_type* ptr) override {
				// TODO: occasional ``Bad file descriptor''
				this_log() << "Removing server " << *ptr << std::endl;
				auto result = _upstream.find(ptr->vaddr());
				if (result != _upstream.end()) {
					remove_valid_server(result);
				}
			}

			void accept_connection(sysx::poll_event&) override {
				sysx::endpoint addr;
				socket_type sock;
				_socket.accept(sock, addr);
				sysx::endpoint vaddr = virtual_addr(addr);
				auto res = _upstream.find(vaddr);
				if (res == _upstream.end()) {
					this->add_connected_server(std::move(sock), vaddr, sysx::poll_event::In);
					this_log() << "connected peer " << vaddr << std::endl;
				} else {
					server_type& s = res->second;
					const sysx::port_type
					local_port = s.socket().bind_addr().port();
					this_log()
						<< "ports: "
						<< addr.port() << ' '
						<< local_port
						<< std::endl;
					if (addr.port() < local_port) {
						this_log()
							<< "not replacing peer " << s
							<< std::endl;
					} else {
						this_log log;
						log << "replacing peer " << s;
						poller().disable(s.socket().fd());
						server_type new_s(std::move(s));
						new_s.setparent(this);
						new_s.socket(std::move(sock));
						remove_valid_server(res);
//						_upstream.erase(res);
						_upstream.emplace(vaddr, std::move(new_s));
//						_upstream.emplace(vaddr, std::move(*new_s));
						poller().emplace(
							sysx::poll_event{res->second.socket().fd(), sysx::poll_event::Inout, sysx::poll_event::Inout},
							handler_type(&res->second));
						log << " with " << res->second << std::endl;
					}
				}
			}

			void peer(sysx::endpoint addr) {
				this->connect_to_server(addr, sysx::poll_event::In);
			}

			void
			socket(const sysx::endpoint& addr) {
				_socket.bind(addr);
				_socket.listen();
				poller().insert_special(sysx::poll_event{_socket.fd(),
					sysx::poll_event::In});
				if (!this->stopped()) {
					this->_semaphore.notify_one();
				}
			}

			inline sysx::endpoint
			server_addr() const {
				return _socket.bind_addr();
			}

			Category
			category() const noexcept override {
				return Category{
					"nic_server",
					[] () { return new NIC_server; },
					{"endpoint"},
					[] (const void* obj, Category::key_type key) {
						const NIC_server* lhs = static_cast<const NIC_server*>(obj);
						if (key == "endpoint") {
							std::stringstream tmp;
							tmp << lhs->server_addr();
							return tmp.str();
						}
						return Category::value_type();
					}
				};
			}

		private:

			void
			remove_valid_server(iterator_type result) noexcept {
				if (result == _iterator) {
					advance_upstream_iterator();
				}
				result->second.stop();
				_upstream.erase(result);
			}

			void
			advance_upstream_iterator() noexcept {
				if (++_iterator == _upstream.end()) {
					_iterator = _upstream.begin();
				}
			}

			std::pair<iterator_type,bool>
			emplace_server(const sysx::endpoint& vaddr, server_type&& s) {
				auto result = _upstream.emplace(vaddr, std::move(s));
				if (_upstream.size() == 1) {
					_iterator = _upstream.begin();
				}
				return result;
			}

			inline sysx::endpoint
			virtual_addr(sysx::endpoint addr) const {
				return sysx::endpoint(addr, server_addr().port());
			}

			void
			process_kernels() override {
				stdx::front_pop_iterator<kernel_pool> it_end;
				lock_type lock(this->_mutex);
				stdx::for_each_thread_safe(lock,
					stdx::front_popper(this->_kernels), it_end,
					[this] (kernel_type* rhs) { process_kernel(rhs); }
				);
			}

			void
			process_kernel(kernel_type* k) {
				if (this->server_addr() && k->to() == this->server_addr()) {
					this->root()->send(k);
//					std::ostringstream msg;
//					msg << "Kernel is sent to local node. From="
//						<< this->server_addr() << ", to=" << k->to();
//					throw Error(msg.str(), __FILE__, __LINE__, __func__);
				}

				if (k->moves_everywhere()) {
					for (auto& pair : _upstream) {
						pair.second.send(k);
					}
					// delete broadcast kernel
					delete k;
				} else if (k->moves_upstream() && k->to() == sysx::endpoint()) {
					if (_upstream.empty()) {
						// short-circuit kernels when no upstream servers are available
						this->root()->send(k);
					} else {
						// skip stopped hosts
						iterator_type old_iterator = _iterator;
						if (_iterator->second.stopped()) {
							do {
								advance_upstream_iterator();
							} while (_iterator->second.stopped() and old_iterator != _iterator);
						}
						// round robin over upstream hosts
						_iterator->second.send(k);
						advance_upstream_iterator();
					}
				} else if (k->moves_downstream() and not k->from()) {
					// kernel @k was sent to local node
					// because no upstream servers had
					// been available
					this->root()->send(k);
				} else {
					// create endpoint if necessary, and send kernel
					if (!k->to()) {
						k->to(k->from());
					}
					this->find_or_create_peer(k->to(), sysx::poll_event::Inout)->send(k);
				}
			}

			server_type* find_or_create_peer(const sysx::endpoint& addr, sysx::poll_event::legacy_event ev) {
				server_type* ret;
				auto result = _upstream.find(addr);
				if (result == _upstream.end()) {
					ret = this->connect_to_server(addr, ev);
				} else {
					ret = &result->second;
				}
				return ret;
			}

			struct print_values {
				explicit print_values(const upstream_type& rhs): map(rhs) {}
				friend std::ostream& operator<<(std::ostream& out, const print_values& rhs) {
					out << '{';
					std::copy(stdx::field_iter<1>(rhs.map.begin()),
						stdx::field_iter<1>(rhs.map.end()),
						stdx::intersperse_iterator<server_type>(out, ","));
					out << '}';
					return out;
				}
			private:
				const upstream_type& map;
			};

			server_type* connect_to_server(sysx::endpoint addr, sysx::poll_event::legacy_event events) {
				// bind to server address with ephemeral port
				sysx::endpoint srv_addr(this->server_addr(), 0);
				return this->add_connected_server(socket_type(srv_addr, addr), addr, events);
			}

			server_type* add_connected_server(socket_type&& sock, sysx::endpoint vaddr,
				sysx::poll_event::legacy_event events,
				sysx::poll_event::legacy_event revents=0)
			{
				sysx::fd_type fd = sock.fd();
				server_type s(std::move(sock), vaddr, this->factory());
				s.setparent(this);
				auto result = emplace_server(vaddr, std::move(s));
				poller().emplace(
					sysx::poll_event{fd, events, revents},
					handler_type(&result.first->second));
				this_log() << "add server " << result.first->second << std::endl;
				return &result.first->second;
			}

			socket_type _socket;
			upstream_type _upstream;
			iterator_type _iterator;
		};

	}

}

namespace stdx {

	template<class T, class Socket>
	struct type_traits<factory::components::NIC_server<T,Socket>> {
		static constexpr const char*
		short_name() { return "nic_server"; }
		typedef factory::components::server_category category;
	};

	template<class T, class Socket, class Kernels>
	struct type_traits<factory::components::Remote_Rserver<T, Socket, Kernels>> {
		static constexpr const char*
		short_name() { return "nic_rserver"; }
		typedef factory::components::server_category category;
	};

	//template<>
	//struct disable_log_category<server_category>:
	//public std::integral_constant<bool, true> {};

}
#endif // FACTORY_SERVER_NIC_SERVER_HH
