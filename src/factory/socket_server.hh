namespace factory {

	namespace components {

		template<class Kernel, class Server>
		struct Handle_kernel_stub: public Server {
			void process_kernel(Kernel*) {}
		};

		template<class Server, class Handler, class Kernel, class Kernel_pair, class Type>
		struct Socket_server: public Server_link<Socket_server<Server, Handler, Kernel, Kernel_pair, Type>, Server> {
			
			typedef Socket_server<Server, Handler, Kernel, Kernel_pair, Type> This;
			typedef typename Event_poller<Handler>::E Event;

			Socket_server():
				_poller(),
				_listener_socket(),
				_upstream(),
				_cpu(0),
				_thread(),
				_mutex()
			{}

			~Socket_server() {
//				std::for_each(_upstream.begin(), _upstream.end(),
//					[] (std::pair<Endpoint,Handler*> pair)
//				{
//					delete pair.second;
//				});
			}
	
			void serve() {
				_poller.run([this] (Event event) {
					std::clog << "Event " << event.user_data()->endpoint() << ' ' << event << std::endl;
					if (event.fd() == (int)_listener_socket) {
						std::pair<Socket,Endpoint> pair = _listener_socket.accept();
						Socket socket = pair.first;
						Endpoint endpoint = pair.second;
						Handler* handler = new Handler(socket, endpoint);
						_poller.register_socket(Event(DEFAULT_EVENTS, handler));
						_upstream[endpoint] = handler;
					} else {
						if (!event.user_data()->valid() && !event.is_closing()) {
							remove(event.user_data()->endpoint());
						} else {
							if (event.is_closing()) {
								// TODO: read event is ignored here
								std::unique_lock<std::mutex> lock(_mutex);
								_upstream.erase(event.user_data()->endpoint());
							} else {
								event.user_data()->handle_event(event, [this, &event] (bool overflow) {
									std::unique_lock<std::mutex> lock(_mutex);
									if (overflow) {
										_poller.modify_socket(Event(DEFAULT_EVENTS | EPOLLOUT, event.user_data()));
									} else {
										_poller.modify_socket(Event(DEFAULT_EVENTS, event.user_data()));
									}
								});
							}
						}
					}
				});
			}

			void send(Kernel* kernel, Endpoint endpoint) {
				std::unique_lock<std::mutex> lock(_mutex);
				std::clog << "Socket_server::send(" << endpoint << ")" << std::endl;
				auto result = _upstream.find(endpoint);
				if (result == _upstream.end()) {
					Handler* handler = add_endpoint(endpoint, DEFAULT_EVENTS | EPOLLOUT);
					handler->send(kernel);
				} else {
					result->second->send(kernel);
					_poller.modify_socket(Event(DEFAULT_EVENTS | EPOLLOUT, result->second));
				}
			}

			void send(Kernel* kernel) {
				std::unique_lock<std::mutex> lock(_mutex);
				std::clog << "Socket_server::send()" << std::endl;
//				std::clog << "Upstream size = " << _upstream.size() << std::endl;
				if (_upstream.size() > 0) {
					auto result = _upstream.begin();
					std::clog << "Socket = " << result->second->socket() << std::endl;
					result->second->send(kernel);
					_poller.modify_socket(Event(DEFAULT_EVENTS | EPOLLOUT, result->second));
				}
			}

			void send(Kernel_pair* kernel) {
				std::unique_lock<std::mutex> lock(_mutex);
				auto result = _upstream.find(kernel->subordinate()->from());
				if (result == _upstream.end()) {
					std::stringstream msg;
					msg << "Can not find upstream server " << kernel->from();
					throw Error(msg.str(), __FILE__, __LINE__, __func__);
				}
				result->second->send(kernel);
				_poller.modify_socket(Event(DEFAULT_EVENTS | EPOLLOUT, result->second));
			}

			void add(Endpoint endpoint) {
				std::unique_lock<std::mutex> lock(_mutex);
				add_endpoint(endpoint);
			}
	
			void remove(Endpoint endpoint) {
				std::unique_lock<std::mutex> lock(_mutex);
				auto result = _upstream.find(endpoint);
				if (result == _upstream.end()) {
					std::stringstream msg;
					msg << "Can not find endpoint to remove: " << endpoint;
					throw Error(msg.str(), __FILE__, __LINE__, __func__);
				}
				std::clog << "Removing " << endpoint << std::endl;
				_poller.erase(Event(DEFAULT_EVENTS, result->second));
				_upstream.erase(endpoint);
			}

			void socket(Endpoint endpoint) {
				_listener_socket.listen(endpoint);
				_poller.register_socket(Event(DEFAULT_EVENTS, new Handler(_listener_socket, endpoint)));
			}

			void start() {
				std::clog << "Socket_server::start()" << std::endl;
				_thread = std::thread([this] { this->serve(); });
			}
	
			void stop_impl() {
				_poller.stop();
				std::clog << "Socket_server::stop_impl()" << std::endl;
			}

			void wait_impl() {
				if (_thread.joinable()) {
					_thread.join();
				}
			}

			void affinity(int cpu) { _cpu = cpu; }

			friend std::ostream& operator<<(std::ostream& out, const This* rhs) {
				return operator<<(out, *rhs);
			}

			friend std::ostream& operator<<(std::ostream& out, const This& rhs) {
				return out << "sserver " << rhs._cpu;
			}

	//	private:
	//		bool is_socket(Socket sock) const {
	//			struct stat stat;
	//			check("fstat()", ::fstat(sock, &stat));
	//			return S_ISSOCK(stat.st_mode);
	//		}
		
		private:

			Handler* add_endpoint(Endpoint endpoint, int events = DEFAULT_EVENTS) {
				Socket socket;
				socket.connect(endpoint);
				Handler* handler = new Handler(socket, endpoint);
				_poller.register_socket(Event(events, handler));
				_upstream[endpoint] = handler;
//				std::clog << "Upstream size = " << _upstream.size() << std::endl;
				return handler;
			}
			
			Event_poller<Handler> _poller;
			Server_socket _listener_socket;
			std::map<Endpoint, Handler*> _upstream;
			int _cpu;
			std::thread _thread;
			std::mutex _mutex;

			static const int DEFAULT_EVENTS = EPOLLET | EPOLLRDHUP | EPOLLIN;
		};

//	template<unsigned int N = 128>
//	struct Web_socket_handler: public Request_handler {
//
//		typedef factory::Kernel Kernel;
//
//		explicit Web_socket_handler(const Event& e, Socket_service<Web_socket_handler<N>>* service):
//			Request_handler(e),
//			_socket(e.data.fd),
//			_context(nullptr),
//			_stream(),
//			_service(service)
//		{
//			_socket.flags(O_NONBLOCK);
//		}
//
//		~Web_socket_handler() {
//			if (_context != nullptr) {
//				free_ws_ctx(_context);
//			}
//		}
//
//		void react(factory::Kernel* k) {
//
//			Request_handler* h = reinterpret_cast<Request_handler*>(k);
//
//			if (_context == nullptr) {
//				_context = ::do_handshake(_socket);
////				Server<Kernel, Strategy_remote>* srv = new Web_socket_server<Kernel, Resource_aware<Round_robin>>(_context);
////				local_repository()->put(_service, srv);
//			} else {
//				std::clog << "WebSocket message" << std::endl;
//				std::clog << "Reading stream" << std::endl;
//				unsigned int opcode;
//				unsigned int left;
//				ssize_t len;
//	            ssize_t bytes = ws_recv(_context, _context->tin_buf + tin_end, BUFSIZE-1);
//	            if (bytes <= 0) {
//	                handler_emsg("client closed connection\n");
//	            }
//	            tin_end += bytes;
//	            /*
//	            printf("before decode: ");
//	            for (i=0; i< bytes; i++) {
//	                printf("%u,", (unsigned char) *(_context->tin_buf+i));
//	            }
//	            printf("\n");
//	            */
//	            if (_context->hybi) {
//	                len = decode_hybi((unsigned char*)(_context->tin_buf + tin_start),
//	                                  tin_end-tin_start,
//	                                  (unsigned char*)(_context->tout_buf), BUFSIZE-1,
//	                                  &opcode, &left);
//	            } else {
//	                len = decode_hixie(_context->tin_buf + tin_start,
//	                                   tin_end-tin_start,
//	                                   (unsigned char*)(_context->tout_buf), BUFSIZE-1,
//	                                   &opcode, &left);
//	            }
//	
//	            if (opcode == 8) {
//	                handler_emsg("client sent orderly close frame\n");
//	            }
//	
//	            printf("decoded: ");
//	            for (int i=0; i<len; i++) {
//					std::cout << _context->tout_buf[i];
////	                printf("%u,", (unsigned char) *(_context->tout_buf+i));
//	            }
//	            printf("\n");
//	            if (len < 0) {
//	                handler_emsg("decoding error\n");
//	            }
//	            if (left) {
//	                tin_start = tin_end - left;
//	                //printf("partial frame from client");
//	            } else {
//	                tin_start = 0;
//	                tin_end = 0;
//	            }
//	
//	            traffic("}");
//				std::clog << "Read end" << std::endl;
//			}
////			ssize_t bytes_read;
////			while ((bytes_read = ::read(_socket, _buffer, N)) > 0) {
////				std::clog << "Bytes read = " << bytes_read << std::endl;
////			}
//
//			if (h->is_close_event()) {
//				std::clog << "Close web socket connection" << std::endl;
//				commit(the_server());
//			}
//		}
//
//		void handle_event(const Event& e) {
//			downstream(the_server(), new Request_handler(e));
//		}
//
//		Socket socket() const { return Socket(_event.data.fd); }
//
//	private:
//		Socket _socket;
//		ws_ctx_t* _context;
//		unsigned int tin_end = 0;
//		unsigned int tin_start = 0;
//		Foreign_stream _stream;
//		Socket_service<Web_socket_handler<N>>* _service;
//	};

		template<class Kernel, template<class X> class Pool, class Type>
		struct Kernel_handler {

			typedef Kernel_handler<Kernel, Pool, Type> This;

			Kernel_handler(Socket socket, Endpoint endpoint):
				_socket(socket),
				_endpoint(endpoint),
				_stream(),
				_ostream(),
				_mutex(),
				_pool(),
				_validated(false)
			{}

			void send(Kernel* kernel) {
				std::unique_lock<std::mutex> lock(_mutex);
				_pool.push(kernel);
			}

			bool valid() const {
//				return _validated ? true : (_socket.error() == 0);
//				TODO: It is probably slow to check error on every event.
				return _socket.error() == 0;
			}

			template<class F>
			void handle_event(Event<This> event, F on_overflow) {
				if (event.is_reading()) {
					try {
						_stream.fill<Socket>(_socket);
						if (_stream.is_full()) {
							std::clog << "Recv " << _ostream << std::endl;
							Type::types().read_and_send_object(_stream, _endpoint);
							_stream.reset();
						}
					} catch (Error& err) {
						std::clog << Error_message(err, __FILE__, __LINE__, __func__);
					} catch (std::exception& err) {
						std::clog << String_message(err.what(), __FILE__, __LINE__, __func__);
					} catch (...) {
						std::clog << String_message(UNKNOWN_ERROR, __FILE__, __LINE__, __func__);
					}
				}
				if (event.is_writing()) {
					try {
						std::unique_lock<std::mutex> lock(_mutex);
						bool overflow = false;
						while (!overflow and !_pool.empty()) {
							if (_ostream.is_flushed()) {
								Kernel* kernel = _pool.front();
								_pool.pop();
								const Type* type = kernel->type();
								if (type == nullptr) {
									std::stringstream msg;
									msg << "Can not find type for kernel " << kernel->id();
									throw Durability_error(msg.str(), __FILE__, __LINE__, __func__);
								}
								_ostream << type->id();
								kernel->write(_ostream);
								_ostream.write_size();
								std::clog << "Send " << _ostream << std::endl;
							}
							_ostream.flush<Socket>(_socket);
							if (_ostream.is_flushed()) {
								std::clog << "Flushed." << std::endl;
								_ostream.reset();
							} else {
								overflow = true;
							}
						}
						on_overflow(overflow);
	//					std::clog << "Buffer size: " << _ostream.size() << std::endl;
	//					_socket.send(_ostream);
					} catch (Connection_error& err) {
						std::clog << Error_message(err, __FILE__, __LINE__, __func__);
	//					the_server()->send(kernel);
	//					this->root()->send(kernel);
					} catch (Error& err) {
						std::clog << Error_message(err, __FILE__, __LINE__, __func__);
					} catch (std::exception& err) {
						std::clog << String_message(err, __FILE__, __LINE__, __func__);
					} catch (...) {
						std::clog << String_message(UNKNOWN_ERROR, __FILE__, __LINE__, __func__);
					}
				}
			}

			int fd() const { return _socket; }
			Socket socket() const { return _socket; }
			Endpoint endpoint() const { return _endpoint; }

		private:
			Server_socket _socket;
			Endpoint _endpoint;
			Foreign_stream _stream;
			Foreign_stream _ostream;
			std::mutex _mutex;
			Pool<Kernel*> _pool;
			bool _validated;
		};

	}

}

