#include <map>

#include <sysx/socket.hh>
#include <sysx/cmdline.hh>
#include <sysx/network.hh>

#include <factory/factory.hh>
#include <factory/server/cpu_server.hh>
#include <factory/server/timer_server.hh>
#include <factory/server/nic_server.hh>

#include "distance.hh"
#include "cache_guard.hh"
#include "hierarchy.hh"
#include "springy_graph_generator.hh"
#include "hierarchy_with_graph.hh"
#include "test.hh"

// disable logs
namespace stdx {

	template<>
	struct disable_log_category<sysx::buffer_category>:
	public std::true_type {};

	template<>
	struct disable_log_category<factory::components::kernel_category>:
	public std::true_type {};

	template<>
	struct disable_log_category<factory::components::server_category>:
	public std::true_type {};

}

namespace factory {
	inline namespace this_config {

		struct config {
			typedef components::Managed_object<components::Server<config>> server;
			typedef components::Principal<config> kernel;
			typedef components::CPU_server<config> local_server;
			typedef components::NIC_server<config, sysx::socket> remote_server;
			typedef components::Timer_server<config> timer_server;
			typedef components::No_server<config> app_server;
			typedef components::No_server<config> principal_server;
			typedef components::No_server<config> external_server;
			typedef components::Basic_factory<config> factory;
		};

		typedef config::kernel Kernel;
		typedef config::server Server;
	}
}

using namespace factory;
using namespace factory::this_config;

#include "apps/autoreg/autoreg_app.hh"
#include "ping_pong.hh"
#include "delayed_shutdown.hh"
#include "master_discoverer.hh"

template<class Address>
struct Main: public Kernel {

	typedef Address addr_type;
	typedef typename sysx::ipaddr_traits<addr_type> traits_type;
	typedef Negotiator<addr_type> negotiator_type;
	typedef discovery::Hierarchy_with_graph<discovery::Hierarchy<addr_type>> hierarchy_type;
	typedef discovery::Distance_in_tree<addr_type> distance_type;
	typedef Master_discoverer<addr_type, hierarchy_type, distance_type> discoverer_type;
	typedef stdx::log<Main> this_log;

	Main(Server& this_server, int argc, char* argv[]):
	_network(),
	_port(),
	_numpings(),
	_cmdline(argc, argv, {
		sysx::cmd::ignore_first_arg(),
		sysx::cmd::ignore_arg("--num-peers"),
		sysx::cmd::ignore_arg("--role"),
		sysx::cmd::make_option({"--network"}, _network),
		sysx::cmd::make_option({"--port"}, _port),
		sysx::cmd::make_option({"--ping"}, _numpings)
	})
	{}

	void
	act(Server& this_server) {
		parse_cmdline_args(this_server);
		this_server.factory()->types().register_type(negotiator_type::static_type());
		this_server.factory()->types().register_type(Ping::static_type());
		this_server.factory()->types().register_type(autoreg::Generator1<float,autoreg::Uniform_grid>::static_type());
		this_server.factory()->types().register_type(autoreg::Wave_surface_generator<float,autoreg::Uniform_grid>::static_type());
		this_server.factory()->types().register_type(autoreg::Autoreg_model<float>::static_type());
		this_server.factory()->types().register_type(Secret_agent::static_type());
		if (this_server.factory()->exit_code()) {
			commit(this_server.local_server());
		} else {
			const sysx::ipv4_addr netmask = sysx::ipaddr_traits<sysx::ipv4_addr>::loopback_mask();
			const sysx::endpoint bind_addr(_network.address(), _port);
			this_server.remote_server()->bind(bind_addr, netmask);
			const auto default_delay = (_network.address() == sysx::ipv4_addr{127,0,0,1}) ? 0 : 2;
			const auto start_delay = sysx::this_process::getenv("START_DELAY", default_delay);
			discoverer_type* master = new discoverer_type(_network, _port);
			master->id(sysx::to_host_format(_network.address().rep()));
			this_server.factory()->instances().register_instance(master);
			master->after(std::chrono::seconds(start_delay));
//			master->at(Kernel::Time_point(std::chrono::seconds(start_time)));
			this_server.timer_server()->send(master);

//			if (_network.address() == traits_type::localhost()) {
//				schedule_pingpong_after(std::chrono::seconds(0), this_server);
//			}

			if (_network.address() == sysx::ipv4_addr{127,0,0,1}) {
				schedule_autoreg_app(this_server);
			}

			schedule_shutdown_after(std::chrono::seconds(60), master, this_server);
		}
	}

private:

	template<class Time>
	void
	schedule_pingpong_after(Time delay, Server& this_server) {
		Ping_pong* p = new Ping_pong(_numpings);
		p->after(delay);
		this_server.timer_server()->send(p);
	}

	template<class Time>
	void
	schedule_shutdown_after(Time delay, discoverer_type* master, Server& this_server) {
		Delayed_shutdown<addr_type>* shutdowner = new Delayed_shutdown<addr_type>(master->hierarchy());
		shutdowner->after(delay);
		shutdowner->parent(this);
		this_server.timer_server()->send(shutdowner);
	}

	void
	schedule_autoreg_app(Server& this_server) {
		Autoreg_app* app = new Autoreg_app;
		app->after(std::chrono::seconds(5));
		this_server.timer_server()->send(app);
	}

	void
	parse_cmdline_args(Server& this_server) {
		try {
			_cmdline.parse();
			if (!_network) {
				throw sysx::invalid_cmdline_argument("--network");
			}
		} catch (sysx::invalid_cmdline_argument& err) {
			std::cerr << err.what() << ": " << err.arg() << std::endl;
			this_server.factory()->set_exit_code(1);
		}
	}

	sysx::network<sysx::ipv4_addr> _network;
	sysx::port_type _port;
	uint32_t _numpings;
	sysx::cmdline _cmdline;

};

template<class Address>
struct Hosts: public std::vector<Address> {

	typedef Address addr_type;

	friend std::istream&
	operator>>(std::istream& cmdline, Hosts& rhs) {
		std::string filename;
		cmdline >> filename;
		std::clog << "reading hosts from " << filename << std::endl;
		std::ifstream in(filename);
		rhs.clear();
		std::copy(
			std::istream_iterator<addr_type>(in),
			std::istream_iterator<addr_type>(),
			std::back_inserter(rhs)
		);
		return cmdline;
	}

};

int main(int argc, char* argv[]) {

	const std::string role_master = "master";
	const std::string role_slave = "slave";
	sysx::port_type discovery_port = 10000;
	uint32_t num_pings = 10;
	const uint32_t do_not_kill = std::numeric_limits<uint32_t>::max();
	uint32_t kill_slave_after = do_not_kill;
	uint32_t kill_master_after = do_not_kill;
	Hosts<sysx::ipv4_addr> hosts2;

	typedef stdx::log<decltype(main)> this_log;
	int retval = 0;
	sysx::network<sysx::ipv4_addr> network;
	uint32_t npeers = 0;
	std::string role;
	try {
		sysx::cmdline cmd(argc, argv, {
			sysx::cmd::ignore_first_arg(),
			sysx::cmd::make_option({"--hosts"}, hosts2),
			sysx::cmd::make_option({"--network"}, network),
			sysx::cmd::make_option({"--num-peers"}, npeers),
			sysx::cmd::make_option({"--role"}, role),
			sysx::cmd::make_option({"--port"}, discovery_port),
			sysx::cmd::make_option({"--ping"}, num_pings),
			sysx::cmd::make_option({"--kill-slave-after"}, kill_slave_after),
			sysx::cmd::make_option({"--kill-master-after"}, kill_master_after)
		});
		cmd.parse();
		if (role != role_master and role != role_slave) {
			throw sysx::invalid_cmdline_argument("--role");
		}
		if (!network and hosts2.empty()) {
			throw sysx::invalid_cmdline_argument("--network,--hosts");
		}
	} catch (sysx::invalid_cmdline_argument& err) {
		std::cerr << err.what() << ": " << err.arg() << std::endl;
		return 1;
	}

	if (role == role_master) {

		this_log() << "Network = " << network << std::endl;
		this_log() << "Num peers = " << npeers << std::endl;
		this_log() << "Role = " << role << std::endl;
		this_log() << "start,mid = " << *network.begin() << ',' << *network.middle() << std::endl;

		std::vector<sysx::endpoint> hosts;
		if (network) {
			std::transform(
				network.begin(),
				network.begin() + npeers,
				std::back_inserter(hosts),
				[discovery_port] (const sysx::ipv4_addr& addr) {
					return sysx::endpoint(addr, discovery_port);
				}
			);
		} else {
			std::transform(
				hosts2.begin(),
				hosts2.end(),
				std::back_inserter(hosts),
				[discovery_port] (const sysx::ipv4_addr& addr) {
					return sysx::endpoint(addr, discovery_port);
				}
			);
		}
		springy::Springy_graph graph;
		graph.add_nodes(hosts.begin(), hosts.end());

		sysx::process_group procs;
		for (sysx::endpoint endpoint : hosts) {
			char workdir[PATH_MAX];
			::getcwd(workdir, PATH_MAX);
			procs.emplace([endpoint, &argv, npeers, &network, discovery_port, num_pings, workdir] () {
				return sysx::this_process::execute(
					"/usr/bin/ssh", endpoint.addr4(), "cd", workdir, ";", "exec",
					argv[0],
					"--network", sysx::network<sysx::ipv4_addr>(endpoint.addr4(), network.netmask()),
					"--port", discovery_port,
					"--role", "slave",
					"--num-peers", 0,
					"--ping", num_pings
				);
			});
		}

		this_log() << "Forked " << procs << std::endl;
		if (kill_master_after != do_not_kill) {
			using namespace std::chrono;
			std::this_thread::sleep_for(seconds(kill_master_after));
			sysx::process& master = procs.front();
			this_log() << "Killing master process " << master.id() << std::endl;
			master.kill();
		}
		if (kill_slave_after != do_not_kill) {
			using namespace std::chrono;
			std::this_thread::sleep_for(seconds(kill_slave_after));
			sysx::process_group::iterator first = procs.begin();
			// skip master
			++first;
			this_log() << "Killing slave process " << first->id() << std::endl;
			first->kill();
		}
		retval = procs.wait();

	} else {
		using namespace factory;
		retval = factory_main<Main<sysx::ipv4_addr>,config>(argc, argv);
	}

	return retval;
}
