#ifndef SERVER_HPP
#define SERVER_HPP

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
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <any>
#include "logs.hpp"

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

/**
 * @brief Http connection class.
 */
class http_connection : public std::enable_shared_from_this<http_connection> {
 public:
    /**
   * @brief Constructor of the http_connection class.
   * @param socket Socket
   */
  http_connection(tcp::socket socket);
  /**
   * @brief Calls read_request and check_deadline.
   */
  void start();

 private:
  /**
   * @brief Asynchronously reads the request and calls process_request.
   */
  void read_request();

  /**
   * @brief Causes the request to be processed according to the corresponding 
   * GET POST PATCH type, and then calls write_response.
   */
  void process_request();

  /**
   * @brief Sets information for response.
   */
  void setup_response();

  /**
   * @brief Checks the target URL.
   */
  void handle_get_request();

  /**
   * @brief Checks the target URL.
   */
  void handle_post_request();

  /**
   * @brief Checks the target URL.
   */
  void handle_patch_request();

   /**
   * @brief Writing from the buffer and sending a response.
   */ 
  void write_response();

   /**
   * @brief Checks the lifetime of current session.
   */ 
  void check_deadline();

  /**
   * @brief Retrieves data from the query and creates an sql query.
   */
  void get_comments();

  /**
   * @brief Retrieves data from the query and creates an sql query.
   */
  void delete_comment();

  /**
   * @brief Retrieves data from the query and creates an sql query.
   */
  void add_comment();

  /**
   * @brief Retrieves data from the query and creates an sql query.
   */
  void change_comment();

  /**
   * @brief Counts comments by entity.
   */
  void count_comments_by_entity();

  /**
   * @brief Checks if the comment exists. Returns true if the comment exists otherwise returns false.
   */
  bool is_comment_exists();

  /**
   * @brief Retrieves a json object from the request body.
   */
  nlohmann::json get_request_json_body() const;

  /**
   * @brief Creates a session and connects to the database.
   * @param statement CQL statement
   */
  void connect(CassStatement* statement);

  /**
   * @brief Executes query.
   * @param statement CQL statement
   */
  void execute_query(CassStatement* statement);

  /**
   * @brief Processes the response from the database.
   * @param result_future CassFuture object representing furure result of the query
   */
  void handle_query_result(CassFuture* result_future);

  /**
   * @brief Returns json row from databas response.
   * @param result CassResult object representing result of the query
   * @param rows Iterator for the rows of the result 
   */
  nlohmann::json get_json_row(const CassResult* result, CassIterator* rows) const;
  
  //! Socker
  tcp::socket socket_;

  //! Buffer for response
  beast::flat_buffer buffer_{8192};

  //! Request
  http::request<http::dynamic_body> request_;

  //! Response
  http::response<http::dynamic_body> response_;
  
  //! Timer for session timeout
  net::steady_timer deadline_{socket_.get_executor(), std::chrono::seconds(60)};

  //! Request objects in unordered_map
  std::unordered_map<std::string, std::any> request_un_map_;
  //! Available ScyllaDB connection hosts
  const char* hosts_ = "172.22.0.3"; //scylla-node1
  //! db session
  std::unique_ptr<CassSession, decltype(&cass_session_free)> session_ = std::unique_ptr<CassSession, decltype(&cass_session_free)>(cass_session_new(), &cass_session_free);
  //! Request target
  beast::string_view target_;
};

/**
 * @brief Starts the server.
 */
void http_server(tcp::acceptor& acceptor, tcp::socket& socket);

#endif // SERVER_HPP