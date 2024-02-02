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
  setup_response();

  try {
    const std::unordered_map<http::verb, std::function<void()>> method_handlers = {
      {http::verb::get, std::bind(&http_connection::handle_get_request, this)},
      {http::verb::post, std::bind(&http_connection::handle_post_request, this)},
      {http::verb::patch, std::bind(&http_connection::handle_patch_request, this)}
    };

    auto handler = method_handlers.find(request_.method());
    if (handler != method_handlers.end()) {
      handler->second();
    } else {
      BOOST_LOG_TRIVIAL(error) 
          << "Invalid request method: " << request_.method_string();
      response_.result(http::status::not_found);
    }
  } catch (const std::exception &e) {
    BOOST_LOG_TRIVIAL(error) 
      << "Error with request handling: " << e.what();
    response_.result(http::status::bad_request);
  }

  write_response();
}

void http_connection::setup_response() {
  response_.version(request_.version());
  response_.keep_alive(false);
  response_.set(http::field::content_type, "application/json");
  response_.set(http::field::server, "presetshare.comments");
  target_ = request_.target();
  response_.result(http::status::ok);
}

void http_connection::handle_get_request() {
  if(target_ == "/comments") {
    get_comments();
  } else {
    BOOST_LOG_TRIVIAL(error) 
      << "Invalid GET request target: " << target_;
    response_.result(http::status::not_found);
  }
}

void http_connection::handle_post_request() {
  if(target_ == "/comments/make") {
    add_comment();
  } else {
    BOOST_LOG_TRIVIAL(error) 
      << "Invalid POST request target: " << target_;
    response_.result(http::status::not_found);
  }
}

void http_connection::handle_patch_request() {
  if(target_ == "/comments/delete") {
    delete_comment();
  } else if(target_ == "/comments/change") {
    change_comment();
  } else {
    BOOST_LOG_TRIVIAL(error) 
      << "Invalid POST request target: " << target_;
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

void http_connection::get_comments() {
  auto entity = std::string(request_.find("Entity")->value());
  auto page = std::stoll(request_.find("Pagination-Page")->value());
  auto per_page = std::stoi(request_.find("Pagination-Per-Page")->value());

  page = std::max(page, 1LL);
  per_page = std::clamp(per_page, 1, 100);

  request_un_map_["entity"] = entity;
  request_un_map_["pagination-page"] = page;
  request_un_map_["pagination-per-page"] = per_page;

  response_.set("Pagination-Current-Page", std::to_string(page));
  response_.set("Pagination-Per-Page", std::to_string(per_page));

  BOOST_LOG_TRIVIAL(info) 
    << "Fetching comments for entity: " << entity
    << ", Page: " << page 
    << ", Per Page: " << per_page;

  const char* query =
    "SELECT entity, created_time, comment_id, author, created_by, text, updated_time "
    "FROM keyspace_comments.comments "
    "WHERE entity = ? AND deleted = false";

  auto statement = std::unique_ptr<CassStatement, 
    decltype(&cass_statement_free)>(cass_statement_new(query, 1), &cass_statement_free);
  cass_statement_bind_string(statement.get(), 0, entity.c_str());

  connect(statement.get());
}

void http_connection::add_comment() {
  auto json_body = get_request_json_body();

  auto text = json_body.value("text", "");
  auto author = std::string(request_.find("Author")->value());
  auto entity = std::string(request_.find("Entity")->value());
  auto created_by = std::stoll(request_.find("Created_by")->value());

  request_un_map_["entity"] = entity;

  BOOST_LOG_TRIVIAL(info) 
    << "Adding new comment for entity: " << entity;

  const char* query =
    "INSERT INTO keyspace_comments.comments (comment_id, entity, author, "
    "text, deleted, created_by, created_time, updated_time) "
    "VALUES (uuid(), ?, ?, "
    "?, false, ?, toUnixTimestamp(now()), toUnixTimestamp(now()))";

  auto statement = std::unique_ptr<CassStatement, 
    decltype(&cass_statement_free)>(cass_statement_new(query, 4), &cass_statement_free);
  cass_statement_bind_string(statement.get(), 0, entity.c_str());
  cass_statement_bind_string(statement.get(), 1, author.c_str());
  cass_statement_bind_string(statement.get(), 2, text.c_str());
  cass_statement_bind_int64(statement.get(), 3, created_by);

  connect(statement.get());
}

void http_connection::delete_comment() {
  auto entity = std::string(request_.find("Entity")->value());
  auto comment_id = std::string(request_.find("Comment_id")->value());
  auto created_time = std::stoll(request_.find("Created_time")->value());

  request_un_map_["comment_id"] = comment_id;
  request_un_map_["created_time"] = created_time;
  request_un_map_["entity"] = entity;

  BOOST_LOG_TRIVIAL(info) 
    << "Deleting comment with ID: " << comment_id 
    << " for entity: " << entity;

  const char* query =
    "UPDATE keyspace_comments.comments SET deleted = true "
    "WHERE entity = ? AND comment_id = ? AND created_time = ?";

  CassUuid uuid_comment_id;
  cass_uuid_from_string(comment_id.c_str(), &uuid_comment_id);

  auto statement = std::unique_ptr<CassStatement, 
    decltype(&cass_statement_free)>(cass_statement_new(query, 3), &cass_statement_free);
  cass_statement_bind_string(statement.get(), 0, entity.c_str());
  cass_statement_bind_uuid(statement.get(), 1, uuid_comment_id);
  cass_statement_bind_int64(statement.get(), 2, created_time);

  connect(statement.get());
}

void http_connection::change_comment() {
  auto json_body = get_request_json_body();

  auto text = json_body.value("text", "");
  auto entity = std::string(request_.find("Entity")->value());
  auto comment_id = std::string(request_.find("Comment_id")->value());
  auto created_time = std::stoll(request_.find("Created_time")->value());

  request_un_map_["comment_id"] = comment_id;
  request_un_map_["entity"] = entity;
  request_un_map_["created_time"] = created_time;

  BOOST_LOG_TRIVIAL(info) 
    << "Changing comment with ID: " << comment_id 
    << " for entity: " << entity;

  const char* query =
    "UPDATE keyspace_comments.comments SET text = ?, "
    "updated_time = toUnixTimestamp(now()) WHERE entity = ? "
    "AND comment_id = ? AND created_time = ?";

  CassUuid uuid_comment_id;
  cass_uuid_from_string(comment_id.c_str(), &uuid_comment_id);
  
  auto statement = std::unique_ptr<CassStatement, 
    decltype(&cass_statement_free)>(cass_statement_new(query, 4), &cass_statement_free);
  cass_statement_bind_string(statement.get(), 0, text.c_str());
  cass_statement_bind_string(statement.get(), 1, entity.c_str());
  cass_statement_bind_uuid(statement.get(), 2, uuid_comment_id);
  cass_statement_bind_int64(statement.get(), 3, created_time);

  connect(statement.get());
}

void http_connection::connect(CassStatement* statement) {
  auto cluster = std::unique_ptr<CassCluster, 
    decltype(&cass_cluster_free)>(cass_cluster_new(), &cass_cluster_free);
  cass_cluster_set_contact_points(cluster.get(), hosts_);
  auto connect_future = std::unique_ptr<CassFuture, 
    decltype(&cass_future_free)>(cass_session_connect(
      session_.get(), cluster.get()), &cass_future_free);

  if (cass_future_error_code(connect_future.get()) == CASS_OK) {
    execute_query(statement);
  } else {
    const char* message;
    size_t message_length;
    cass_future_error_message(connect_future.get(), &message, &message_length);
    BOOST_LOG_TRIVIAL(error) 
      << "Unable to connect to db: " << std::string(message, message_length);
    response_.result(http::status::bad_request);
  }

}

void http_connection::execute_query(CassStatement* statement) {
  bool executes = true;

  if (target_ == "/comments/change" || target_ == "/comments/delete") {
    executes = is_comment_exists();
    if (!executes) {
      BOOST_LOG_TRIVIAL(error) 
        << "Comment does not exists";
      response_.result(http::status::bad_request);
    }
  }

  if (executes) {
    CassFuture* result_future = cass_session_execute(session_.get(), statement);
    if (cass_future_error_code(result_future) == CASS_OK) {
      if (target_ == "/comments") {
        handle_query_result(result_future);
      }
    } else {
      const char* message;
      size_t message_length;
      cass_future_error_message(result_future, &message, &message_length);
      BOOST_LOG_TRIVIAL(error) 
        << "Unable to run query: " << std::string(message, message_length);
      response_.result(http::status::bad_request);
    }

    cass_future_free(result_future);
  }
}

bool http_connection::is_comment_exists() {
  CassUuid uuid_comment_id;
  cass_uuid_from_string((std::any_cast<std::string>(
    request_un_map_["comment_id"])).c_str(), &uuid_comment_id);

  const char* check_query = 
    "SELECT * FROM keyspace_comments.comments "
    "WHERE entity = ? AND comment_id = ? AND created_time = ? AND deleted = false";

  auto check_statement = std::unique_ptr<CassStatement, 
    decltype(&cass_statement_free)>(cass_statement_new(check_query, 3), &cass_statement_free);

  cass_statement_bind_string(check_statement.get(), 0, 
    (std::any_cast<std::string>(request_un_map_["entity"])).c_str());
  cass_statement_bind_uuid(check_statement.get(), 1, uuid_comment_id);
  cass_statement_bind_int64(check_statement.get(), 2, 
    std::any_cast<long long>(request_un_map_["created_time"]));

  auto check_result_future = std::unique_ptr<CassFuture, 
    decltype(&cass_future_free)>(cass_session_execute(
    session_.get(), check_statement.get()), &cass_future_free);

  const CassResult* check_result = cass_future_get_result(check_result_future.get());
  auto count_rows = cass_result_row_count(check_result);
  
  cass_result_free(check_result);

  return count_rows == 1;
}

void http_connection::handle_query_result(CassFuture* result_future) {
  const CassResult* result = cass_future_get_result(result_future);
  auto rows = std::unique_ptr<CassIterator, 
    decltype(&cass_iterator_free)>(cass_iterator_from_result(result), &cass_iterator_free);
  nlohmann::json json_array = nlohmann::json::array();
  size_t total_rows = cass_result_row_count(result);

  long long page = std::any_cast<long long>(request_un_map_["pagination-page"]);
  int per_page = std::any_cast<int>(request_un_map_["pagination-per-page"]);

  size_t start_row = per_page * (page - 1);
  size_t end_row = per_page * page;

  for (size_t i = 0; cass_iterator_next(rows.get()) && i < end_row; ++i) {
    if (i >= start_row) {
      json_array.push_back(get_json_row(result, rows.get()));
    }
  }

  auto total_page_count = 
    static_cast<long long>(std::ceil(static_cast<double>(total_rows) / per_page));
  response_.set("Pagination-Total-Pages", std::to_string(total_page_count));
  response_.set("Pagination-Total-Comments", std::to_string(total_rows));

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

  for(const auto& buffer : body.data()) {
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