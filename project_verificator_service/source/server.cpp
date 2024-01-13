#include "server.hpp"
// #include "logs.cpp"

std::size_t my_program_state::request_count() {
  std::cout << "http_server()" << std::endl;
  static std::size_t count = 0;
  return ++count;
}

std::time_t my_program_state::now() {
  std::cout << "http_server()" << std::endl;
  return std::time(0);
}

http_connection::http_connection(tcp::socket socket) : socket_(std::move(socket)) {}

void http_connection::start() {
  std::cout << "http_server()" << std::endl;
  read_request();
  check_deadline();
}

void http_connection::read_request() {
  std::cout << "http_server()" << std::endl;
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
  std::cout << "http_server()" << std::endl;
  response_.version(request_.version());
  response_.keep_alive(false);

  switch(request_.method()) {
    case http::verb::get: {
      response_.result(http::status::ok);
      response_.set(http::field::server, "Beast");
      create_response();
      break;
    }

    default: {
      response_.result(http::status::bad_request);
      response_.set(http::field::content_type, "text/plain");
      beast::ostream(response_.body())
        << "Invalid request-method '"
        << std::string(request_.method_string())
        << "'";
      break;
    }
  }

  write_response();
}

void http_connection::create_response() {
  std::cout << "http_server()" << std::endl;
  if(request_.target() == "/count") {
    response_.set(http::field::content_type, "text/html");
    beast::ostream(response_.body())
      << "<html>\n"
      <<  "<head><title>Request count</title></head>\n"
      <<  "<body>\n"
      <<  "<h1>Request count</h1>\n"
      <<  "<p>There have been "
      <<  my_program_state::request_count()
      <<  " requests so far.</p>\n"
      <<  "</body>\n"
      <<  "</html>\n";
  }
  else if(request_.target() == "/time") {
    response_.set(http::field::content_type, "text/html");
    beast::ostream(response_.body())
      <<  "<html>\n"
      <<  "<head><title>Current time</title></head>\n"
      <<  "<body>\n"
      <<  "<h1>Current time</h1>\n"
      <<  "<p>The current time is "
      <<  my_program_state::now()
      <<  " seconds since the epoch.</p>\n"
      <<  "</body>\n"
      <<  "</html>\n";
  }
  else {
    response_.result(http::status::not_found);
    response_.set(http::field::content_type, "text/plain");
    beast::ostream(response_.body()) << "File not found\r\n";
  }
}

void http_connection::write_response() {
  std::cout << "http_server()" << std::endl;
  auto self = shared_from_this();

  response_.content_length(response_.body().size());

  http::async_write(socket_, response_,
    [self](beast::error_code ec, std::size_t) {
      self->socket_.shutdown(tcp::socket::shutdown_send, ec);
      self->deadline_.cancel();
  });
}

void http_connection::check_deadline() {
  std::cout << "http_server()" << std::endl;
  auto self = shared_from_this();

  deadline_.async_wait([self](beast::error_code ec) {
    if(!ec) {
      self->socket_.close(ec);
    }
  });
}

void http_server(tcp::acceptor& acceptor, tcp::socket& socket) {
  std::cout << "http_server()" << std::endl;
  acceptor.async_accept(socket, [&](beast::error_code ec) {
    if(!ec) {
      std::make_shared<http_connection>(std::move(socket))->start();
      http_server(acceptor, socket);
    }
  });
}


