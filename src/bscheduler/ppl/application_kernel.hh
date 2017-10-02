#ifndef BSCHEDULER_PPL_APPLICATION_KERNEL_HH
#define BSCHEDULER_PPL_APPLICATION_KERNEL_HH

#include <cstdint>
#include <bscheduler/config.hh>
#include <string>
#include <vector>

namespace bsc {

	class Application_kernel: public BSCHEDULER_KERNEL_TYPE {

	public:
		typedef BSCHEDULER_KERNEL_TYPE kernel_type;
		typedef std::vector<std::string> container_type;

	private:
		container_type _args, _env;
		std::string _error;
		application_type _application = 0;

	public:

		inline
		Application_kernel(
			const container_type& args,
			const container_type& env
		):
		_args(args),
		_env(env)
		{}

		Application_kernel() = default;
		virtual ~Application_kernel() = default;

		void
		act() override {
			this->return_to_parent();
		}

		inline const container_type&
		arguments() const noexcept {
			return this->_args;
		}

		inline const container_type&
		environment() const noexcept {
			return this->_env;
		}

		inline void
		set_error(const std::string& rhs) {
			this->_error = rhs;
		}

		const std::string
		error() const noexcept {
			return this->_error;
		}

		application_type
		application() const noexcept {
			return this->_application;
		}

		inline void
		application(application_type rhs) noexcept {
			this->_application = rhs;
		}

		void
		write(sys::pstream& out) override;

		void
		read(sys::pstream& in) override;

	};

}

#endif // vim:filetype=cpp
