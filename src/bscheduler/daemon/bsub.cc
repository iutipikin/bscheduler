#include <bscheduler/base/error_handler.hh>
#include <bscheduler/ppl/application_kernel.hh>
#include <bscheduler/api.hh>
#include <iostream>
#include <string>
#include <vector>

#include "bscheduler_socket.hh"

using bsc::Application_kernel;
using namespace bsc;

Application_kernel*
new_application_kernel(int argc, char* argv[]) {
	typedef Application_kernel::container_type container_type;
	container_type args, env;
	for (int i=1; i<argc; ++i) {
		args.emplace_back(argv[i]);
	}
	for (char** first=environ; *first; ++first) {
		env.emplace_back(*first);
	}
	Application_kernel* k = new Application_kernel(args, env);
	k->to(sys::socket_address(BSCHEDULER_UNIX_DOMAIN_SOCKET));
	return k;
}

class Main: public kernel {

private:
	int _argc;
	char** _argv;

public:
	Main(int argc, char** argv):
	_argc(argc),
	_argv(argv)
	{}

	void
	act() override {
		upstream<Remote>(
			this,
			new_application_kernel(this->_argc, this->_argv)
		);
	}

	void
	react(kernel* child) {
		Application_kernel* app = dynamic_cast<Application_kernel*>(child);
		if (app->return_code() != bsc::exit_code::success) {
			std::string message = app->error();
			if (message.empty()) {
				message = to_string(app->return_code());
			}
			std::string app_id =
				app->application() == 0
				? "application"
				: std::to_string(app->application());
			sys::log_message(
				"bsub",
				"failed to submit _: _",
				app_id,
				message
			);
		} else {
			sys::log_message("bsub", "submitted _", app->application());
		}
		commit<Local>(this, app->return_code());
	}

};

int main(int argc, char* argv[]) {
	if (argc <= 1) {
		std::cerr << "Please, specify at least one argument." << std::endl;
		return 1;
	}
	bsc::install_error_handler();
	bsc::types.register_type<Application_kernel>();
	factory_guard g;
	bsc::factory.parent().use_localhost(false);
	try {
		bsc::send(new Main(argc, argv));
	} catch (const std::exception& err) {
		sys::log_message(
			"bsub",
			"failed to connect to daemon process: _",
			err.what()
		);
		bsc::graceful_shutdown(1);
	}
	return bsc::wait_and_return();
}
