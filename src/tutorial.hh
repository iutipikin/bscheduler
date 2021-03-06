/**
\page tutorial Tutorial

Every Bscheduler application is composed of computational \link bsc::kernel <em>
kernels</em>\endlink --- self-contained objects which store data and have routines
to process it. In each routine a kernel may create any number of \em
subordinates to decompose application into smaller parts. Some kernels can be
transferred to another cluster node to make application distributed. An
application exits when there are no kernels left to process.

Kernels are processed by \link bsc::basic_pipeline <em>pipelines</em>\endlink
--- kernel queues with processing threads attached. Each device has its own
pipeline (there is a pipeline for CPU, I/O device and NIC) which allows them to work
in parallel: process one part of data with CPU pipeline and simultaneously write
another one with disk pipeline. Every pipeline work until application exit.

Each programme begins with starting all necessary pipelines and sending the
main kernel to one of the them. After that programme execution resembles that
of sequential programme with each nested call to a procedure replaced with
construction of a subordinate kernel and sending it to appropriate pipeline.
The difference is that pipelines process kernels \em asynchronously, so
procedure code is decomposed into \link bsc::kernel::act()
<code>act()</code>\endlink routine which constructs subordinates and \link
bsc::kernel::react(kernel*) <code>react()</code>\endlink routine which
processes results they return.

\section api Developing distributed applications

The first step is to decide which pipelines your programme needs. Most probably
these are standard
- \link bsc::parallel_pipeline CPU pipeline\endlink,
- \link bsc::io_pipeline I/O pipeline\endlink,
- \link bsc::socket_pipeline NIC pipeline\endlink and
- \link bsc::timer_pipeline Timer pipeline\endlink (for periodic and
  schedule-based execution of kernels).

Standard pipelines for all devices except NIC are initialised in \c
bscheduler/api.hh header. To initialise NIC pipeline you need to tell it which
pipeline is local and which one is remote. The following code snippet shows the
usual way of doing this.

\code{.cpp}
#include <bscheduler/api.hh>
\endcode

The second step is to subclass \link bsc::kernel <code>kernel</code>\endlink
and implement \c act() and \c react() member functions for each sequential
stage of your programme and for parallel parts of each stage.

\code{.cpp}
struct My_app: public kernel {

	void
	act() {
		std::cout << "Hello world" << std::endl;
		bsc::commit(this);
	}

	void
	react(kernel*) {
		// not needed for such a simple programme
	}

};
\endcode

\note Even if your programme does not have parallel parts, its performance may
still increase by constructing a separate instance of a kernel for each input
file. For example, to make a programme that searches files for matching pattern
(\c grep command in UNIX) parallel it is sufficient to construct a kernel for
each file and send all of them to CPU pipeline. A better way is to construct a
separate kernel to read portions of the files via I/O pipeline and for each
portion construct and send new kernel to CPU pipeline to process it in
parallel.

Finally, you need to start every pipeline and send the main kernel to the local
one via \link bsc::send <code>send</code>\endlink function.

\code{.cpp}
int main() {
	using namespace bsc;
	install_error_handler();  // print backtrace on exception or signal
	factory_guard g;          // automatically start and stop the factory
	send(new My_app);         // start the programme by sending the first
	                          // kernel to the pipeline
	return wait_and_return(); // wait for the programme completion
}
\endcode

Use \link bsc::commit <code>commit</code>\endlink to return the kernel to its
parent and reclaim system resources.

\section failover Automatic failure handling

In general, there are two types of failures occurring in any hierarchical
distributed system:

- failure of a \em subordinate node --- a node which connects only to its
  principal and no other node --- and
- failure of a \em principal node --- a node with multiple connections.

In Bscheduler the "node" refers both to a cluster node and to a kernel, failures
of which are handled differently.

\subsection kernel-failures Handling kernel failures

Since any subordinate kernel is part of a hierarchy the simplest method of
handling its failure is to let its principal restart it on a healthy cluster
node. Bscheduler does this automatically for any kernel that has parent. This
approach works well unless your hierarchy is deep and require restarting a lot
of kernels upon a failure; however, this approach does not work for the main
kernel --- the first kernel of an application that does not have a parent.

In case of the main kernel failure the only option is to keep a copy of it on
some other cluster node and restore from it when the former node fails.
Bscheduler implements this for any kernel with the \link
bsc::kernel_flag::carries_parent <code>carries_parent</code>\endlink flag set,
but the approach works only for those principal kernels that have only one
subordinate at a time (extending algorithm to cover more cases is one of the
goals of ongoing research).

At present, a kernel is considered failed when a node to which it was sent
fails, and a node is considered failed when the corresponding connection closes
unexpectedly. At the moment, there is no mechanism that deals with unreliable
connections other than timeouts configured in underlying operating system.

\subsection node-failures Handling cluster node failures

Cluster node failures are much simpler to mitigate: there is no state to be
lost and the only invariant that should be preserved in a cluster is
connectivity of nodes. All nodes should "know" each other and be able to
establish arbitrary connections between each other; in other words, nodes
should be able to \em discover each other. Bscheduler implements this
functionality without distributed consensus algorithm: the framework builds tree
hierarchy of nodes using IP addresses and pre-set fan-out value to rank nodes.
Using this algorithm a node computes IP address of its would-be principal and
tries to connect to it; if the connection fails it tries another node from the
same or higher level of tree hierarchy. If it reaches the root of the tree and no
node responds, it becomes the root node. This algorithm is used both during node
bootstrap phase and upon a failure of any principal.

\section hierarchy Hierarchical architecture

At high-level Bscheduler framework is composed of multiple layers:

- physical layer (fully-connected servers and network switches),
- middleware layer (hierarchy of nodes),
- application layer (hierarchy of kernels).

Load balancing is implemented by superimposing hierarchy of kernels on the
hierarchy of nodes: When a node pipelines are overflown by kernels some of them
may be "spilled" to subordinate nodes (planned feature), much like water flows
from the top of a cocktail pyramid down to its bottom when volume of glasses
in the current layer is to small to hold it.

\section bottomup Bottom-up design

Bscheduler framework uses [bottom-up](http://www.paulgraham.com/progbot.html)
source code development approach which means we create low-level abstractions
to simplify high-level code and make it clean and readable. There are three
layers of abstractions:

- \link sys operating system layer\endlink, which contains thin C++ wrappers
  for POSIX and Linux system calls and data structures, and
- generic C++ abstractions layer,
- top layer which contains all business logic.

From the developer perspective it is unclear which layer is the bottom one, but
still we would like to separate them via
<a href="https://erdani.com/publications/xp2000.pdf">policy-based programming</a>
and <a href="http://erdani.com/publications/traits.html">traits classes</a>.

*/
