#include "master_discoverer.hh"

#include <array>
#include <ostream>

#include <unistdx/base/log_message>

namespace {

	std::array<const char*,4> all_results{
		"add", "remove", "reject", "retain"
	};

}

std::ostream&
factory::operator<<(std::ostream& out, probe_result rhs) {
	const size_t i = static_cast<size_t>(rhs);
	const char* s = i >= 0 && i <= all_results.size()
		? all_results[i] : "<unknown>";
	return out << s;
}

void
factory::master_discoverer::on_start() {
	this->probe_next_node();
}

void
factory::master_discoverer::on_kernel(factory::api::Kernel* k) {
	if (typeid(*k) == typeid(discovery_timer)) {
		// start probing only if it has not been started already
		if (this->state() == state_type::waiting) {
			this->probe_next_node();
		}
	} else if (typeid(*k) == typeid(probe)) {
		this->update_subordinates(dynamic_cast<probe*>(k));
	} else if (typeid(*k) == typeid(prober)) {
		this->update_principal(dynamic_cast<prober*>(k));
	} else if (typeid(*k) == typeid(socket_pipeline_kernel)) {
		this->on_event(dynamic_cast<socket_pipeline_kernel*>(k));
	}
}

void
factory::master_discoverer::probe_next_node() {
	this->setstate(state_type::probing);
	if (this->_iterator == this->_end) {
		sys::log_message(
			"discoverer",
			"_: all addresses have been probed",
			this->ifaddr()
		);
		this->send_timer();
	} else {
		addr_type addr = *this->_iterator;
		sys::endpoint new_principal(addr, this->port());
		sys::log_message("discoverer", "_: probe _", this->ifaddr(), addr);
		prober* p = new prober(
			this->ifaddr(),
			this->_hierarchy.principal(),
			new_principal
		);
		factory::api::upstream(this, p);
		++this->_iterator;
	}
}

void
factory::master_discoverer::send_timer() {
	this->setstate(state_type::waiting);
	using namespace std::chrono;
	if (!this->_timer) {
		this->_timer = new discovery_timer;
	}
	this->_timer->after(this->_interval);
	factory::api::send<>(this->_timer, this);
}

void
factory::master_discoverer::update_subordinates(probe* p) {
	const sys::endpoint src = p->from();
	probe_result result = this->process_probe(p);
	sys::log_message(
		"discoverer",
		"_: _ subordinate _",
		this->ifaddr(),
		result,
		src
	);
	if (result == probe_result::add_subordinate) {
		this->_hierarchy.add_subordinate(src);
	} else if (result == probe_result::remove_subordinate) {
		this->_hierarchy.remove_subordinate(src);
	}
	p->setf(kernel_flag::do_not_delete);
	factory::api::commit<factory::api::Remote>(p);
}

factory::probe_result
factory::master_discoverer::process_probe(probe* p) {
	probe_result result = probe_result::retain;
	if (p->from() == this->_hierarchy.principal()) {
		// principal tries to become subordinate
		// which is prohibited
		p->result(exit_code::error);
		result = probe_result::reject_subordinate;
	} else {
		if (p->old_principal() != p->new_principal()) {
			if (p->new_principal() == this->_hierarchy.endpoint()) {
				result = probe_result::add_subordinate;
			} else if (p->old_principal() == this->_hierarchy.endpoint()) {
				result = probe_result::remove_subordinate;
			}
		}
		p->result(exit_code::success);
	}
	return result;
}

void
factory::master_discoverer::update_principal(prober* p) {
	if (p->result() != exit_code::success) {
		sys::log_message(
			"discoverer",
			"_: prober returned from _: _",
			this->ifaddr(),
			p->new_principal(),
			p->result()
		);
		this->probe_next_node();
	} else {
		const sys::endpoint& oldp = p->old_principal();
		const sys::endpoint& newp = p->new_principal();
		if (oldp) {
			::factory::factory.nic().stop_client(oldp);
		}
		sys::log_message(
			"discoverer",
			"_: set principal to _",
			this->ifaddr(),
			newp
		);
		this->_hierarchy.set_principal(newp);
		// try to find better principal after a period of time
		this->send_timer();
	}
}

void
factory::master_discoverer::on_event(socket_pipeline_kernel* ev) {
	if (ev->endpoint() == this->_hierarchy.principal()) {
		sys::log_message(
			"discoverer",
			"_: unset principal _",
			this->ifaddr(),
			ev->endpoint()
		);
		this->_hierarchy.unset_principal();
		this->probe_next_node();
	}
}
