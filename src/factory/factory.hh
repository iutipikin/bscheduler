#ifndef FACTORY_FACTORY_HH
#define FACTORY_FACTORY_HH

#if __cplusplus < 201103L
	#error Factory requires C++11 compiler.
#endif

#include <sys/process.hh>
#include <sys/log.hh>
#include <sys/packetstream.hh>

#include <factory/bits/terminate_handler.hh>
#include <factory/kernel.hh>

#include <factory/server/cpu_server.hh>
#include <factory/server/io_server.hh>
#include <factory/server/timer_server.hh>
#include <factory/server/nic_server.hh>
#include <factory/server_guard.hh>
#include <factory/reflection.hh>
#include <factory/algorithm.hh>

namespace factory {

	Server_guard<components::CPU_server<Kernel>> local_server;
	Server_guard<components::Timer_server<Kernel>> timer_server;
	Server_guard<components::IO_server<Kernel>> io_server;

	/*
		[this] (app_type app, stream_type&) {
		 	this->app_server()->forward(app, _vaddr, _packetbuf);
		}
	*/

	/// Server API
	inline void compute(Kernel* rhs) { local_server.send(rhs); }
//	inline void spill(Kernel* rhs) { remote_server.send(rhs); }
	inline void input(Kernel* rhs) { io_server.send(rhs); }
	inline void output(Kernel* rhs) { io_server.send(rhs); }

	inline void schedule(Kernel* rhs) { timer_server.send(rhs); }

	inline void
	schedule_after(Kernel::Duration dur, Kernel* rhs) {
		rhs->after(dur);
		timer_server.send(rhs);
	}

	inline void yield(Kernel* rhs) { compute(rhs); }

	/*
	inline void collect(Kernel* rhs) {
		rhs->return_to_parent();
		spill(rhs);
	}
	*/

	inline void commit(Kernel* rhs, Result ret) {
		if (!rhs->parent()) {
			factory::return_value.set_value(static_cast<int>(rhs->result()));
		} else {
			rhs->return_to_parent(ret);
			compute(rhs);
		}
	}

	inline void commit(Kernel* rhs) {
		Result ret = rhs->result();
		commit(rhs, ret == Result::undefined ? Result::success : ret);
	}

	/*
	/// @deprecated
	template<class S>
	void
	commit(S& srv, Result res = Result::success) {
		this->principal(_parent);
		this->result(res);
		srv->send(this);
	}
	*/

}

#endif // FACTORY_FACTORY_HH
