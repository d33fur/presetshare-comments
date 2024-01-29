#include "server.hpp"
#include "logs.cpp"

http_connection::http_connection(tcp::socket socket) : socket_(std::move(socket)) {}

void http_connection::start() {
  read_request();
  check_deadline();
}

void http_connection::read_request() {
  auto self = shared_from_this();

  http::async_read(socket_, buffer_, request_,
    [self](beast::error_code ec, std::size_t bytes_transferred) {
      boost::ignore_unused(bytes_transferred);
      if(!ec) {
        self->process_request();
      }
    });
}

void http_connection::process_request() {
  response_.version(request_.version());
  response_.keep_alive(false);
  response_.set(http::field::content_type, "application/json");
  response_.set(http::field::server, "presetshare.comments");

  switch(request_.method()) {
    case http::verb::get: {
      response_.result(http::status::ok);
      create_get_response();
      break;
    }

    case http::verb::post: {
      response_.result(http::status::ok);
      create_post_response();
      break;
    }

    case http::verb::patch: {
      response_.result(http::status::ok);
      create_patch_response();
      break;
    }

    default: {
      response_.result(http::status::bad_request);
      beast::ostream(response_.body())
        << "Invalid request-method '"
        << std::string(request_.method_string())
        << "'";
      break;
    }
  }

  write_response();
}

void http_connection::create_get_response() {
  if(request_.target() == "/comments") {
    get_comments();
  }
  else {
    response_.result(http::status::not_found);
    beast::ostream(response_.body()) << "unknown get request\r\n";
  }
}

void http_connection::create_post_response() {
  if(request_.target() == "/comments/make") {
    add_comment();
  }
  else {
    response_.result(http::status::not_found);
    beast::ostream(response_.body()) << "unknown post request\r\n";
  }
}

void http_connection::create_patch_response() {
  if(request_.target() == "/comments/delete") {
    delete_comment();
  }
  else if(request_.target() == "/comments/change") {
    change_comment();
  }
  else {
    response_.result(http::status::not_found);
    beast::ostream(response_.body()) << "unknown patch request\r\n";
  }
}

void http_connection::write_response() {
  auto self = shared_from_this();

  response_.content_length(response_.body().size());

  http::async_write(socket_, response_,
    [self](beast::error_code ec, std::size_t) {
      self->socket_.shutdown(tcp::socket::shutdown_send, ec);
      self->deadline_.cancel();
  });
}

void http_connection::check_deadline() {
  auto self = shared_from_this();

  deadline_.async_wait([self](beast::error_code ec) {
    if(!ec) {
      self->socket_.close(ec);
    }
  });
}

void http_connection::get_comments() {
  std::string entity((request_.find("Entity"))->value());

  BOOST_LOG_TRIVIAL(info) << "Fetching comments for entity: " << entity;

  std::stringstream query_ss;
  query_ss << "SELECT * FROM keyspace_comments.comments WHERE entity = '" << entity
           << "' AND deleted = false";

  std::string query_str = query_ss.str();
  const char* query = query_str.c_str();
  execute_query(query);
}

void http_connection::add_comment() {
  auto json_body = get_request_json_body();

  std::string text(json_body.value("text", ""));
  std::string author((request_.find("Author"))->value());
  std::string entity((request_.find("Entity"))->value());
  int64_t created_by = std::stoll(std::string(request_.find("Created_by")->value()));

  BOOST_LOG_TRIVIAL(info) << "Adding new comment for entity: " << entity;

  std::stringstream query_ss;
  query_ss << "INSERT INTO keyspace_comments.comments (comment_id, entity, author, "
           << "text, deleted, created_by, created_time, updated_time) "
           << "VALUES (uuid(), '" << entity << "', '" << author << "', '" << text << "', false, " 
           << created_by << ", toUnixTimestamp(now()), toUnixTimestamp(now()));";

  std::string query_str = query_ss.str();
  const char* query = query_str.c_str();
  execute_query(query);
}

void http_connection::delete_comment() {
  std::string entity((request_.find("Entity"))->value());
  std::string comment_id((request_.find("Comment_id")->value()));
  int64_t created_time = std::stoll(std::string(request_.find("Created_time")->value()));

  BOOST_LOG_TRIVIAL(info) << "Deleting comment with ID: " << comment_id << " for entity: " << entity;

  std::stringstream query_ss;
  query_ss << "UPDATE keyspace_comments.comments SET deleted = true WHERE entity = '"
           << entity << "' AND comment_id = " << comment_id << " AND created_time = "
           << created_time << ";";

  std::string query_str = query_ss.str();
  const char* query = query_str.c_str();
  execute_query(query);
}

void http_connection::change_comment() {
  auto json_body = get_request_json_body();

  std::string text(json_body.value("text", ""));
  std::string entity((request_.find("Entity"))->value());
  std::string comment_id((request_.find("Comment_id")->value()));
  int64_t created_time = std::stoll(std::string(request_.find("Created_time")->value()));

  BOOST_LOG_TRIVIAL(info) << "Changing comment with ID: " << comment_id << " for entity: " << entity;

  std::stringstream query_ss;
  query_ss << "UPDATE keyspace_comments.comments SET text = '" << text
           << "', updated_time = toUnixTimestamp(now())"
           << " WHERE entity = '" << entity << "' AND comment_id = " << comment_id 
           << " AND created_time = " << created_time << ";";

  const std::string query_str = query_ss.str();
  const char* query = query_str.c_str();
  execute_query(query);
}

void http_connection::execute_query(const char* query) {
  CassFuture* connect_future = NULL;
  CassCluster* cluster = cass_cluster_new();
  CassSession* session = cass_session_new();
  const char* hosts = "scylla-node1";

  cass_cluster_set_contact_points(cluster, hosts);
  connect_future = cass_session_connect(session, cluster);

  if (cass_future_error_code(connect_future) == CASS_OK) {
    make_statement_and_send_query(query, session);
  } else {
    const char* message;
    size_t message_length;
    cass_future_error_message(connect_future, &message, &message_length);
    fprintf(stderr, "Unable to connect: '%.*s'\n", (int)message_length, message);
  }

  cass_future_free(connect_future);
  cass_cluster_free(cluster);
  cass_session_free(session);
}

void http_connection::make_statement_and_send_query(const char* query, CassSession* session) {
  CassFuture* close_future = NULL;
  CassStatement* statement = cass_statement_new(query, 0);
  CassFuture* result_future = cass_session_execute(session, statement);

  if (cass_future_error_code(result_future) == CASS_OK) {
    handle_query_result(result_future);
  } else {
    const char* message;
    size_t message_length;
    cass_future_error_message(result_future, &message, &message_length);
    fprintf(stderr, "Unable to run query: '%.*s'\n", (int)message_length, message);
  }

  cass_statement_free(statement);
  cass_future_free(result_future);

  close_future = cass_session_close(session);
  cass_future_wait(close_future);
  cass_future_free(close_future);
}

void http_connection::handle_query_result(CassFuture* result_future) {
  const CassResult* result = cass_future_get_result(result_future);
  CassIterator* rows = cass_iterator_from_result(result);
  nlohmann::json json_array = nlohmann::json::array();

  while (cass_iterator_next(rows)) {
    json_array.push_back(get_json_row(result, rows));
  }

  cass_iterator_free(rows);
  cass_result_free(result);

  beast::ostream(response_.body()) << json_array;
}

nlohmann::json http_connection::get_json_row(const CassResult* result, CassIterator* rows) {
  size_t column_count = cass_result_column_count(result);
  const CassRow* row = cass_iterator_get_row(rows);
  nlohmann::json json_row;

  for (size_t i = 0; i < column_count; ++i) {
    const char* column_name;
    size_t column_name_length;
    cass_result_column_name(result, i, &column_name, &column_name_length);
    std::string col_name(column_name, column_name_length);

    const CassValue* column = cass_row_get_column(row, i);

    CassValueType type = cass_value_type(column);
    
    switch (type) {
      case CASS_VALUE_TYPE_UUID: {
        CassUuid uuid;
        char uuid_str[CASS_UUID_STRING_LENGTH];
        cass_value_get_uuid(column, &uuid);
        cass_uuid_string(uuid, uuid_str);
        json_row[col_name] = uuid_str;
        break;
      }
      case CASS_VALUE_TYPE_BIGINT: {
        cass_int64_t column_value;
        cass_value_get_int64(column, &column_value);
        json_row[col_name] = column_value;
        break;
      }
      case CASS_VALUE_TYPE_BOOLEAN: {
        cass_bool_t column_value;
        cass_value_get_bool(column, &column_value);
        json_row[col_name] = (column_value == cass_true ? true : false);
        break;
      }
      default: {
        const char* column_value;
        size_t column_value_length;
        cass_value_get_string(column, &column_value, &column_value_length);
        std::string col_value(column_value, column_value_length);
        json_row[col_name] = col_value;
        break;
      }
    }
  }

  return json_row;
}

nlohmann::json http_connection::get_request_json_body() {
  boost::beast::multi_buffer& body = request_.body();
  std::stringstream ss;

  for (const auto& buffer : body.data()) {
    ss << boost::beast::make_printable(buffer);
  }

  return nlohmann::json::parse(ss);
}

void http_server(tcp::acceptor& acceptor, tcp::socket& socket) {
  acceptor.async_accept(socket, [&](beast::error_code ec) {
    if(!ec) {
      std::make_shared<http_connection>(std::move(socket))->start();
      http_server(acceptor, socket);
    }
  });
}


