#ifndef FACTORY_PPL_SOCKET_PIPELINE_HH
#define FACTORY_PPL_SOCKET_PIPELINE_HH

#include <unordered_map>
#include <vector>

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
	class socket_pipeline:
		public basic_socket_pipeline<T,remote_client<T,Socket,Router>> {

	public:
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

	private:
		typedef sys::ipaddr_traits<addr_type> traits_type;
		typedef std::vector<server_type> server_container_type;
		typedef typename server_container_type::iterator server_iterator;
		typedef typename server_container_type::const_iterator
			server_const_iterator;
		typedef std::unordered_map<sys::endpoint,event_handler_ptr>
			client_container_type;
		typedef typename client_container_type::iterator client_iterator;
		typedef ifaddr_type::rep_type rep_type;
		typedef mobile_kernel::id_type id_type;
		typedef sys::field_iterator<server_const_iterator,0> ifaddr_iterator;

	private:
		server_container_type _servers;
		client_container_type _clients;
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
		add_client(const sys::endpoint& addr) {
			lock_type lock(this->_mutex);
			this->add_client(addr, sys::poll_event::In);
		}

		void
		stop_client(const sys::endpoint& addr);

		void
		add_server(const ifaddr_type& rhs) {
			this->add_server(
				sys::endpoint(rhs.address(), this->_port),
				rhs.netmask()
			);
		}

		void
		add_server(const sys::endpoint& rhs, addr_type netmask);

		void
		remove_server(const ifaddr_type& ifaddr);

		void
		forward(const kernel_header& hdr, sys::pstream& istr);

		inline void
		set_port(sys::port_type rhs) noexcept {
			this->_port = rhs;
		}

		inline sys::port_type
		port() const noexcept {
			return this->_port;
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
		remove_server(sys::fd_type fd) override;

		void
		remove_client(event_handler_ptr ptr) override;

		void
		accept_connection(sys::poll_event& ev) override;

		void
		remove_client(client_iterator result);

		void
		remove_server(server_iterator result);

		server_iterator
		find_server(const ifaddr_type& ifaddr);

		server_iterator
		find_server(sys::fd_type fd);

		server_iterator
		find_server(const sys::endpoint& dest);

		void
		ensure_identity(kernel_type* kernel, const sys::endpoint& dest);

		/// round robin over upstream hosts
		void
		find_next_client();

		inline bool
		end_reached() const noexcept {
			return this->_iterator == this->_clients.end();
		}

		inline void
		reset_iterator() noexcept {
			this->_iterator = this->_clients.begin();
		}

		std::pair<client_iterator,bool>
		emplace_pipeline(const sys::endpoint& vaddr, event_handler_ptr&& s);

		inline sys::endpoint
		virtual_addr(const sys::endpoint& addr) const {
			return addr.family() == sys::family_type::unix
				? addr
				: sys::endpoint(addr, this->_port);
		}

		void
		process_kernels() override;

		void
		process_kernel(kernel_type* k);

		event_handler_ptr
		find_or_create_peer(
			const sys::endpoint& addr,
			sys::poll_event::legacy_event ev
		);

		event_handler_ptr
		add_client(
			const sys::endpoint& addr,
			sys::poll_event::legacy_event events
		);

		event_handler_ptr
		add_connected_pipeline(
			socket_type&& sock,
			sys::endpoint vaddr,
			sys::poll_event::legacy_event events,
			sys::poll_event::legacy_event revents=0
		);

	};

}

#endif // vim:filetype=cpp