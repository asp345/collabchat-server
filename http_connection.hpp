#ifndef HTTP_CONNECTION_HPP
#define HTTP_CONNECTION_HPP

#include "base64.hpp"
#include "database.hpp"
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/json.hpp>
#include <boost/shared_ptr.hpp>

namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = net::ip::tcp;

class http_connection : public std::enable_shared_from_this<http_connection> {
public:
  explicit http_connection(tcp::socket socket, Database *db);

  // Initiate the asynchronous operations associated with the connection.
  void start();

private:
  // Base64
  Base64 base64;

  // Database
  Database *db;

  // The socket for the currently connected client.
  tcp::socket socket_;

  // The buffer for performing reads.
  beast::flat_buffer buffer_{16384};

  // The request message.
  http::request<http::dynamic_body> request_;

  // The response message.
  http::response<http::dynamic_body> response_;

  // The timer for putting a deadline on connection processing.
  net::steady_timer deadline_{socket_.get_executor(), std::chrono::seconds(60)};

  // Asynchronously receive a complete request message.
  void read_request();

  void router();

  // Asynchronously transmit the response message.
  void write_response();

  // Check whether we have spent enough time on this connection.
  void check_deadline();
};

// "Loop" forever accepting new connections.
void http_server(tcp::acceptor &acceptor, tcp::socket &socket);

#endif // HTTP_CONNECTION_HPP
