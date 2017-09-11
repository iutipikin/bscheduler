#ifndef FACTORY_KERNEL_KSTREAM_HH
#define FACTORY_KERNEL_KSTREAM_HH

#include <cassert>

#include <unistdx/base/log_message>
#include <unistdx/net/endpoint>
#include <unistdx/net/pstream>

#include <factory/base/error.hh>
#include <factory/kernel/kernel_error.hh>
#include <factory/kernel/kernel_header.hh>
#include <factory/kernel/kernel_type_registry.hh>
#include <factory/kernel/kernelbuf.hh>
#include <factory/ppl/kernel_proto_flag.hh>

namespace factory {

	namespace bits {

		template <class Router>
		struct no_forward: public Router {
			void
			operator()(const kernel_header&, sys::pstream&) {
				assert("no forwarding");
			}
		};

		template <class Router>
		struct forward_to_child: public Router {
			void
			operator()(const kernel_header& hdr, sys::pstream& istr) {
				this->forward_child(hdr, istr);
			}
		};

		template <class Router>
		struct forward_to_parent: public Router {
			void
			operator()(const kernel_header& hdr, sys::pstream& istr) {
				this->forward_parent(hdr, istr);
			}
		};

		template <class T>
		struct no_router {

			void
			send_local(T* rhs) {}

			void
			send_remote(T*) {}

			void
			forward(const kernel_header&, sys::pstream&) {}

			void
			forward_child(const kernel_header&, sys::pstream&) {}

			void
			forward_parent(const kernel_header&, sys::pstream&) {}

		};
	}

	template<class T>
	struct kstream: public sys::pstream {

		typedef T kernel_type;

		using sys::pstream::operator<<;
		using sys::pstream::operator>>;

		kstream() = default;
		inline explicit
		kstream(sys::packetbuf* buf): sys::pstream(buf) {}
		kstream(kstream&&) = default;

		inline kstream&
		operator<<(kernel_type* kernel) {
			return operator<<(*kernel);
		}

		kstream&
		operator<<(kernel_type& kernel) {
			this->write_kernel(kernel);
			if (kernel.carries_parent()) {
				// embed parent into the packet
				kernel_type* parent = kernel.parent();
				if (!parent) {
					throw std::invalid_argument("parent is null");
				}
				this->write_kernel(*parent);
			}
			return *this;
		}

		kstream&
		operator>>(kernel_type*& kernel) {
			kernel = this->read_kernel();
			if (kernel->carries_parent()) {
				kernel_type* parent = this->read_kernel();
				kernel->parent(parent);
			}
			return *this;
		}

	private:

		inline void
		write_kernel(kernel_type& kernel) {
			auto type = types.find(typeid(kernel));
			if (type == types.end()) {
				throw std::invalid_argument("kernel type is null");
			}
			*this << type->id();
			kernel.write(*this);
		}

		inline kernel_type*
		read_kernel() {
			return static_cast<kernel_type*>(types.read_object(*this));
		}

	};

}

#endif // vim:filetype=cpp