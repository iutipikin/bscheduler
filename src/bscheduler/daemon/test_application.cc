#include <bscheduler/api.hh>
#include <bscheduler/base/error_handler.hh>

#include "test_application.hh"

std::string failure = "no";

inline bool
test_without_failures() {
	return failure.empty() || failure == "no-failure";
}

inline bool
test_slave_failure() {
	return failure == "slave-failure";
}

inline bool
test_master_failure() {
	return failure == "master-failure";
}

class slave_kernel: public bsc::kernel {

private:
	uint32_t _number = 0;
	uint32_t _nslaves = 0;

public:

	slave_kernel() = default;

	explicit
	slave_kernel(uint32_t number, uint32_t nslaves):
	_number(number),
	_nslaves(nslaves)
	{}

	void
	act() override {
		if (!test_without_failures()) {
			using namespace bsc::this_application;
			if ((test_master_failure() && is_master()) ||
				(test_slave_failure() && is_slave())) {
				using namespace sys;
				send(signal::kill, this_process::parent_id());
				sys::argstream args;
				args.append("false");
				this_process::execute_command(args);
			}
		}
		sys::log_message("slave", "act [_/_]", this->_number, this->_nslaves);
		bsc::commit<bsc::Remote>(this);
	}

	void
	write(sys::pstream& out) const override {
		bsc::kernel::write(out);
		out << this->_number << this->_nslaves;
	}

	void
	read(sys::pstream& in) override {
		bsc::kernel::read(in);
		in >> this->_number >> this->_nslaves;
	}

	inline uint32_t
	number() const noexcept {
		return this->_number;
	}

};

class master_kernel: public bsc::kernel {

private:
	uint32_t _nkernels = BSCHEDULER_TEST_NUM_KERNELS;
	uint32_t _nreturned = 0;

public:

	master_kernel() = default;

	~master_kernel() = default;

	void
	act() override {
		sys::log_message("master", "start");
		for (uint32_t i=0; i<this->_nkernels; ++i) {
			slave_kernel* slave = new slave_kernel(i+1, this->_nkernels);
			bsc::upstream<bsc::Remote>(this, slave);
		}
	}

	void
	react(bsc::kernel* child) {
		slave_kernel* k = dynamic_cast<slave_kernel*>(child);
		if (k->from()) {
			sys::log_message(
				"master",
				"react [_/_] from _",
				k->number(),
				this->_nkernels,
				k->from()
			);
		} else {
			sys::log_message(
				"master",
				"react [_/_]",
				k->number(),
				this->_nkernels
			);
		}
		if (++this->_nreturned == this->_nkernels) {
			sys::log_message("master", "finish");
			if (test_without_failures()) {
				bsc::commit(this);
			} else {
				bsc::commit<bsc::Remote>(this);
			}
		}
	}

	void
	write(sys::pstream& out) const override {
		bsc::kernel::write(out);
	}

	void
	read(sys::pstream& in) override {
		bsc::kernel::read(in);
	}

};

class grand_master_kernel: public bsc::kernel {

public:

	grand_master_kernel() = default;

	~grand_master_kernel() = default;

	void
	act() override {
		sys::log_message("grand", "start");
		master_kernel* master = new master_kernel;
		master->setf(bsc::kernel_flag::carries_parent);
		bsc::upstream<bsc::Remote>(this, master);
	}

	void
	react(bsc::kernel* child) {
		sys::log_message("grand", "finish");
		bsc::commit(this);
	}

	void
	write(sys::pstream& out) const override {
		bsc::kernel::write(out);
	}

	void
	read(sys::pstream& in) override {
		bsc::kernel::read(in);
	}

};

int
main(int argc, char* argv[]) {
	if (argc >= 2) {
		failure = argv[1];
	}
	using namespace bsc;
	install_error_handler();
	register_type<slave_kernel>();
	register_type<master_kernel>();
	register_type<grand_master_kernel>();
	factory_guard g;
	if (this_application::is_master()) {
		if (test_master_failure()) {
			send(new grand_master_kernel);
		} else {
			send(new master_kernel);
		}
	}
	return wait_and_return();
}
