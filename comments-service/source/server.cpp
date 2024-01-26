#include "server.hpp"
// #include "logs.cpp"

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

  switch(request_.method()) {
    case http::verb::get: {
      response_.result(http::status::ok);
      response_.set(http::field::server, "presetshare.comments");
      create_get_response();
      break;
    }

    case http::verb::post: {
      response_.result(http::status::ok);
      response_.set(http::field::server, "presetshare.comments");
      create_post_response();
      break;
    }

    case http::verb::patch: {
      response_.result(http::status::ok);
      response_.set(http::field::server, "presetshare.comments");
      create_patch_response();
      break;
    }

    default: {
      response_.result(http::status::bad_request);
      response_.set(http::field::content_type, "text/plain"); //application/json
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
    response_.set(http::field::content_type, "text/plain"); //application/json
    beast::ostream(response_.body())
      << "{/comments}";
  }
  else {
    response_.result(http::status::not_found);
    response_.set(http::field::content_type, "text/plain"); //application/json
    beast::ostream(response_.body()) << "unknown get request\r\n";
  }
}

void http_connection::create_post_response() {
  if(request_.target() == "/comments/make") {
    response_.set(http::field::content_type, "text/plain"); //application/json
    beast::ostream(response_.body())
      << "{/comments/make}";
  }
  else {
    response_.result(http::status::not_found);
    response_.set(http::field::content_type, "text/plain"); //application/json
    beast::ostream(response_.body()) << "unknown post request\r\n";
  }
}

void http_connection::create_patch_response() {
  if(request_.target() == "/comments/delete") {
    response_.set(http::field::content_type, "text/plain"); //application/json
    beast::ostream(response_.body())
      << "{/comments/delete}";
  }
  else if(request_.target() == "/comments/change") {
    response_.set(http::field::content_type, "text/plain"); //application/json
    beast::ostream(response_.body())
      << "{/comments/change}";
  }
  else {
    response_.result(http::status::not_found);
    response_.set(http::field::content_type, "text/plain"); //application/json
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

void http_server(tcp::acceptor& acceptor, tcp::socket& socket) {
  acceptor.async_accept(socket, [&](beast::error_code ec) {
    if(!ec) {
      std::make_shared<http_connection>(std::move(socket))->start();
      http_server(acceptor, socket);
    }
  });
}

