#ifndef HANDLER
#define HANDLER
#include <websocketpp.hpp>
#include <websocket_connection_handler.hpp>
#include <boost/shared_ptr.hpp>

#include <string>
#include <vector>

using websocketpp::session_ptr;

namespace handler {

	class web_handler : public websocketpp::connection_handler {
	public:
		web_handler() {}
		virtual ~web_handler() {}

		void validate(session_ptr client);
		void on_open(session_ptr client);
		void on_close(session_ptr client);

		void on_message(session_ptr client,const std::string &msg);
		void on_message(session_ptr client, const std::vector<unsigned char> &data);
		void send_to_all(std::string data);
	};

	typedef boost::shared_ptr<web_handler> echo_server_handler_ptr;
	std::list<websocketpp::session_ptr> connections;

	void web_handler::send_to_all(std::string data) {
		std::list<session_ptr>::iterator it;
		for (it = connections.begin(); it != connections.end(); it++) {
			(*it)->send(data);
		}
	}
}

#endif
