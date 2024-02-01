#include "server.hpp"

http_connection::http_connection(tcp::socket socket) : 
  socket_(std::move(socket)) {}

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
  try {
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
        BOOST_LOG_TRIVIAL(error) << "Invalid request method: " 
                                 << std::string(request_.method_string());
        response_.result(http::status::not_found);
        break;
      }
    }
  } catch (const std::exception &e) {
    BOOST_LOG_TRIVIAL(error) << "Error with request handling: " 
                             << e.what();
    response_.result(http::status::bad_request);
  }
  write_response();
}

void http_connection::create_get_response() {
  if(request_.target() == "/comments") {
    get_comments();
  }
  else {
    BOOST_LOG_TRIVIAL(error) << "Invalid GET request target: " 
                             << request_.target();
    response_.result(http::status::not_found);
  }
}

void http_connection::create_post_response() {
  if(request_.target() == "/comments/make") {
    add_comment();
  }
  else {
    BOOST_LOG_TRIVIAL(error) << "Invalid POST request target: " 
                             << request_.target();
    response_.result(http::status::not_found);
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
    BOOST_LOG_TRIVIAL(error) << "Invalid POST request target: " 
                             << request_.target();
    response_.result(http::status::not_found);
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
// paging state и тесты.
void http_connection::get_comments() {
  std::string entity((request_.find("Entity"))->value());
  auto page = std::stoll(std::string(request_.find("Pagination-Page")->value()));
  auto per_page = std::stoul(std::string(request_.find("Pagination-Per-Page")->value()));
  page < 1 ? page = 1 : 0;
  per_page > 100 ? per_page = 100 : 0;
  per_page < 1 ? per_page = 1 : 0;

  response_.set("Pagination-Current-Page", std::to_string(page));
  response_.set("Pagination-Per-Page", std::to_string(per_page));

  BOOST_LOG_TRIVIAL(info) << "Fetching comments for entity: " << entity
                          << ", Page: " << page 
                          << ", Per Page: " << per_page;

  std::stringstream query_ss;
  query_ss << "SELECT entity, created_time, comment_id, author, created_by, text, updated_time "
            << "FROM keyspace_comments.comments WHERE entity = '" 
            << entity << "' AND deleted = false LIMIT " << per_page;

  auto query_str = query_ss.str();
  const char* query = query_str.c_str();
  execute_query(query);
}

void http_connection::add_comment() {
  auto json_body = get_request_json_body();

  std::string text(json_body.value("text", ""));
  std::string author((request_.find("Author"))->value());
  std::string entity((request_.find("Entity"))->value());
  auto created_by = std::stoll(std::string(request_.find("Created_by")->value()));

  BOOST_LOG_TRIVIAL(info) << "Adding new comment for entity: " << entity;

  std::stringstream query_ss;
  query_ss << "INSERT INTO keyspace_comments.comments (comment_id, entity, author, "
           << "text, deleted, created_by, created_time, updated_time) "
           << "VALUES (uuid(), '" << entity << "', '" << author << "', '" << text << "', false, " 
           << created_by << ", toUnixTimestamp(now()), toUnixTimestamp(now()));";

  auto query_str = query_ss.str();
  const char* query = query_str.c_str();
  execute_query(query);
}

void http_connection::delete_comment() {
  std::string entity((request_.find("Entity"))->value());
  std::string comment_id((request_.find("Comment_id")->value()));
  auto created_time = std::stoll(std::string(request_.find("Created_time")->value()));

  BOOST_LOG_TRIVIAL(info) << "Deleting comment with ID: " 
                          << comment_id 
                          << " for entity: " 
                          << entity;

  std::stringstream query_ss;
  query_ss << "UPDATE keyspace_comments.comments SET deleted = true WHERE entity = '" << entity 
          << "' AND comment_id = " << comment_id 
          << " AND created_time = " << created_time << ";";

  auto query_str = query_ss.str();
  const char* query = query_str.c_str();
  execute_query(query);
}

void http_connection::change_comment() {
  auto json_body = get_request_json_body();

  std::string text(json_body.value("text", ""));
  std::string entity((request_.find("Entity"))->value());
  std::string comment_id((request_.find("Comment_id")->value()));
  auto created_time = std::stoll(std::string(request_.find("Created_time")->value()));

  BOOST_LOG_TRIVIAL(info) << "Changing comment with ID: " 
                          << comment_id 
                          << " for entity: " 
                          << entity;

  std::stringstream query_ss;
  query_ss << "UPDATE keyspace_comments.comments SET text = '" << text
            << "', updated_time = toUnixTimestamp(now())"
            << " WHERE entity = '" << entity 
            << "' AND comment_id = " << comment_id 
            << " AND created_time = " << created_time << ";";

  auto query_str = query_ss.str();
  const char* query = query_str.c_str();
  execute_query(query);
}

void http_connection::execute_query(const char* query) {
  CassFuture* connect_future = NULL;
  CassCluster* cluster = cass_cluster_new();
  CassSession* session = cass_session_new();
  const char* hosts = "172.22.0.2"; //scylla-node1

  cass_cluster_set_contact_points(cluster, hosts);
  connect_future = cass_session_connect(session, cluster);

  if (cass_future_error_code(connect_future) == CASS_OK) {
    make_statement_and_send_query(query, session);
  } 
  else {
    const char* message;
    size_t message_length;
    cass_future_error_message(connect_future, &message, &message_length);
    BOOST_LOG_TRIVIAL(error) << "Unable to connect to db: " 
                             << std::string(message, message_length);
  }

  cass_future_free(connect_future);
  cass_cluster_free(cluster);
  cass_session_free(session);
}

void http_connection::make_statement_and_send_query(const char* query, 
  CassSession* session) {
  CassFuture* close_future = NULL;
  CassStatement* statement = cass_statement_new(query, 0);
  CassFuture* result_future = NULL;
 
  if(request_.target() == "/comments/change" || request_.target() == "/comments/delete") {
    if(is_comment_exists(session)) {
      result_future = cass_session_execute(session, statement);
    }
    else {
      BOOST_LOG_TRIVIAL(error) << "Comment does not exists";
    }
  }
  else {
     result_future = cass_session_execute(session, statement);
  }

  if(result_future != NULL) {
    if (cass_future_error_code(result_future) == CASS_OK) {
      if(request_.target() == "/comments") {
        handle_query_result(result_future);
        count_comments_by_entity(session);
      }
    } 
    else {
      const char* message;
      size_t message_length;
      cass_future_error_message(result_future, &message, &message_length);
      BOOST_LOG_TRIVIAL(error) << "Unable to run query: " 
                              << std::string(message, message_length);
    }
  }

  cass_statement_free(statement);
  if(result_future != NULL) {
    cass_future_free(result_future);
  }

  close_future = cass_session_close(session);
  cass_future_wait(close_future);
  cass_future_free(close_future);
}

void http_connection::count_comments_by_entity(CassSession* session) {
  std::string entity((request_.find("Entity"))->value());
  const char* check_query = "SELECT COUNT(*) FROM keyspace_comments.comments WHERE entity = ?;";
  CassStatement* check_statement = cass_statement_new(check_query, 1);
  const char* entity_char = entity.c_str();
  cass_statement_bind_string(check_statement, 0, entity_char);
  
  CassFuture* check_result_future = cass_session_execute(session, check_statement);
  const CassResult* check_result = cass_future_get_result(check_result_future);
  const CassRow* row = cass_result_first_row(check_result);
  const CassValue* column = cass_row_get_column(row, 0);

  cass_int64_t count_rows;
  cass_value_get_int64(column, &count_rows);
  auto per_page = std::stoll(std::string(request_.find("Pagination-Per-Page")->value()));
  long long total_page_count = static_cast<long long>(std::ceil(static_cast<double>(count_rows) / per_page));
  response_.set("Pagination-Total-Count", std::to_string(total_page_count));

  cass_result_free(check_result);
  cass_statement_free(check_statement);
  cass_future_free(check_result_future);
}

bool http_connection::is_comment_exists(CassSession* session) {
  std::string entity((request_.find("Entity"))->value());
  std::string comment_id((request_.find("Comment_id")->value()));
  auto created_time = std::stoll(std::string(request_.find("Created_time")->value()));

  CassUuid uuid_comment_id;
  cass_uuid_from_string(comment_id.c_str(), &uuid_comment_id);

  const char* check_query = "SELECT * FROM keyspace_comments.comments WHERE entity = ? AND comment_id = ? AND created_time = ?;";
  CassStatement* check_statement = cass_statement_new(check_query, 3);

  cass_statement_bind_string(check_statement, 0, entity.c_str());
  cass_statement_bind_uuid(check_statement, 1, uuid_comment_id);
  cass_statement_bind_int64(check_statement, 2, created_time);

  CassFuture* check_result_future = cass_session_execute(session, check_statement);
  const CassResult* check_result = cass_future_get_result(check_result_future);
  auto count_rows = cass_result_row_count(check_result);

  cass_statement_free(check_statement);
  cass_future_free(check_result_future);

  if(count_rows == 1) {
    return true;
  } 
  else {
    return false;
  }
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

nlohmann::json http_connection::get_json_row(const CassResult* result, 
  CassIterator* rows) const {
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

nlohmann::json http_connection::get_request_json_body() const {
  const auto& body = request_.body();
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