#ifndef SERVER_H
#define SERVER_H

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include <cassandra.h>
#include <stdio.h>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <memory>
#include <string>

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

  void get_comments();
  void delete_comment();
  void add_comment();
  void change_comment();

  nlohmann::json get_request_json_body();
  void execute_query(const char* query);
  void make_statement_and_send_query(const char* query, CassSession* session);
  void handle_query_result(CassFuture* result_future);
  nlohmann::json get_json_row(const CassResult* result, CassIterator* rows);
  

  tcp::socket socket_;

  beast::flat_buffer buffer_{8192};

  http::request<http::dynamic_body> request_;

  http::response<http::dynamic_body> response_;

  net::steady_timer deadline_{
      socket_.get_executor(), std::chrono::seconds(60)};
};

void http_server(tcp::acceptor& acceptor, tcp::socket& socket);

#endif // SERVER_HPP