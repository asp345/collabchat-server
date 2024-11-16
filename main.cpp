#include "base64.hpp"
#include "database.hpp"
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/insert_linebreaks.hpp>
#include <boost/archive/iterators/remove_whitespace.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/buffers_iterator.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/core/ostream.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/beast/version.hpp>
#include <boost/json.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>
#include <boost/json/value_ref.hpp>
#include <boost/url.hpp>
#include <boost/url/parse.hpp>
#include <cstdlib>
#include <ctime>
#include <http_connection.hpp>
#include <iostream>
#include <memory>
#include <sqlite3.h>
#include <utility>

namespace beast = boost::beast;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;
using namespace boost::archive::iterators;

Database db = Database("server.db");

// Loop for accepting new connections.
void http_server(tcp::acceptor &acceptor, tcp::socket &socket) {
  acceptor.async_accept(socket, [&](beast::error_code ec) {
    if (!ec)
      std::make_shared<http_connection>(std::move(socket), &db)->start();
    http_server(acceptor, socket);
  });
}

void initialize_db() { // Create tables on db file
  try {
    db.execute("CREATE TABLE IF NOT EXISTS docs (id INTEGER PRIMARY KEY, "
               "workspace TEXT, time INTEGER, date TEXT, title "
               "TEXT,content TEXT)");
    db.execute(
        "CREATE TABLE IF NOT EXISTS workspaces (id INTEGER PRIMARY KEY, name "
        "TEXT, password TEXT)");
    db.execute("CREATE TABLE IF NOT EXISTS chat (id INTEGER PRIMARY KEY, time "
               "INTEGER, "
               "workspace TEXT, title "
               "TEXT,content TEXT)");
    db.execute("CREATE TABLE IF NOT EXISTS online_users (workspace TEXT,"
               "user_id TEXT, "
               "last_ping INTEGER, UNIQUE(workspace, user_id))");
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
  }
}

int main(int argc, char *argv[]) {
  try {
    // Check command line arguments.
    if (argc != 3) {
      std::cerr << "Usage: " << argv[0] << " <address> <port>\n";
      std::cerr << "  For IPv4, try:\n";
      std::cerr << "    server 0.0.0.0 80\n";
      std::cerr << "  For IPv6, try:\n";
      std::cerr << "    server 0::0 80\n";
      return EXIT_FAILURE;
    }

    initialize_db();

    auto const address = net::ip::make_address(argv[1]);
    unsigned short port = static_cast<unsigned short>(std::atoi(argv[2]));

    net::io_context ioc{1};

    tcp::acceptor acceptor{ioc, {address, port}};
    tcp::socket socket{ioc};
    http_server(acceptor, socket);

    ioc.run();
  } catch (std::exception const &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
}
