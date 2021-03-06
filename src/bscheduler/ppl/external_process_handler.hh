#ifndef BSCHEDULER_PPL_EXTERNAL_PROCESS_HANDLER_HH
#define BSCHEDULER_PPL_EXTERNAL_PROCESS_HANDLER_HH

#include <memory>

#include <unistdx/io/fildesbuf>
#include <unistdx/io/poller>
#include <unistdx/net/socket_address>
#include <unistdx/net/pstream>
#include <unistdx/net/socket>

#include <bscheduler/kernel/kstream.hh>
#include <bscheduler/ppl/application.hh>
#include <bscheduler/ppl/application_kernel.hh>
#include <bscheduler/ppl/pipeline_base.hh>

namespace bsc {

	template<class T, class Router>
	class external_process_handler: public pipeline_base {

	public:
		typedef T kernel_type;
		typedef Router router_type;

	private:
		typedef basic_kernelbuf<
			sys::basic_fildesbuf<char, std::char_traits<char>, sys::socket>>
			kernelbuf_type;
		typedef std::unique_ptr<kernelbuf_type> kernelbuf_ptr;
		typedef kstream<T> stream_type;
		typedef typename stream_type::ipacket_guard ipacket_guard;
		typedef sys::opacket_guard<stream_type> opacket_guard;

	private:
		sys::socket_address _endpoint;
		kernelbuf_ptr _buffer;
		stream_type _stream;

	public:

		inline
		external_process_handler(const sys::socket_address& e, sys::socket&& sock):
		_endpoint(e),
		_buffer(new kernelbuf_type),
		_stream(_buffer.get())
		{
			this->_buffer->setfd(std::move(sock));
		}

		inline const sys::socket_address&
		socket_address() const noexcept {
			return this->_endpoint;
		}

		inline sys::socket&
		socket() noexcept {
			return this->_buffer->fd();
		}

		void
		handle(sys::epoll_event& event);

	private:

		void
		receive_kernels(stream_type& stream) noexcept;

	};

}

#endif // vim:filetype=cpp
