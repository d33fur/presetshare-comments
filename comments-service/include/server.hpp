#ifndef SERVER_H
#define SERVER_H

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio.hpp>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <memory>
#include <string>
// #include <cassandra.h>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

class http_connection : public std::enable_shared_from_this<http_connection> {
 public:
  http_connection(tcp::socket socket);
  void start();

 private:
  void read_request();
  void process_request();
  void create_get_response();
  void create_post_response();
  void create_patch_response();
  void write_response();
  void check_deadline();

  tcp::socket socket_;

  beast::flat_buffer buffer_{8192};

  http::request<http::dynamic_body> request_;

  http::response<http::dynamic_body> response_;

  net::steady_timer deadline_{
      socket_.get_executor(), std::chrono::seconds(60)};
};

void http_server(tcp::acceptor& acceptor, tcp::socket& socket);

#endif // SERVER_HPP