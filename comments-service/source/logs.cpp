#pragma warning(push)
#pragma warning(disable:4819)
#   include <boost/shared_ptr.hpp>
#   include <boost/date_time/posix_time/posix_time_types.hpp>
#   include <boost/log/trivial.hpp>
#   include <boost/log/core.hpp>
#   include <boost/log/expressions.hpp>
#   include <boost/log/sources/logger.hpp>
#   include <boost/log/utility/setup/file.hpp>
#   include <boost/log/utility/setup/console.hpp>
#   include <boost/log/utility/setup/common_attributes.hpp>
#   include <boost/log/support/date_time.hpp>
#   include <boost/log/sinks/sync_frontend.hpp>
#   include <boost/log/sinks/text_file_backend.hpp>
#   include <boost/log/sinks/text_ostream_backend.hpp>
#   include <boost/log/attributes/named_scope.hpp>
#pragma warning(pop)
#pragma warning(disable:4503)

/**
 * @brief Initialization of logging.
 */
static void init_log(void) {
  namespace log = boost::log;
  namespace keywords = boost::log::keywords;
  namespace expr = boost::log::expressions;

  log::add_common_attributes();
  log::core::get()->add_global_attribute("Scope", log::attributes::named_scope());
  log::core::get()->set_filter(log::trivial::severity >= log::trivial::trace);

  auto fmtTimeStamp = expr::
    format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S.%f");
  auto fmtThreadId = expr::
    attr<boost::log::attributes::current_thread_id::value_type>("ThreadID");
  auto fmtSeverity = expr::
    attr<boost::log::trivial::severity_level>("Severity");
  auto fmtScope = expr::
    format_named_scope("Scope",
    keywords::format = "%n(%f:%l)",
    keywords::iteration = expr::reverse,
    keywords::depth = 2);
  boost::log::formatter logFmt =
    expr::format("[%1%] (%2%) [%3%] [%4%] %5%")
    % fmtTimeStamp % fmtThreadId % fmtSeverity % fmtScope % expr::smessage;

  auto consoleSink = boost::log::add_console_log(std::clog);
  consoleSink->set_formatter(logFmt);

  // auto fsSink = boost::log::add_file_log(
  //   keywords::file_name = "test_%Y-%m-%d_%H-%M-%S.%N.log",
  //   keywords::rotation_size = 10 * 1024 * 1024,
  //   keywords::min_free_space = 30 * 1024 * 1024,
  //   keywords::open_mode = std::ios_base::app);
  // fsSink->set_formatter(logFmt);
  // fsSink->locked_backend()->auto_flush(true);
}

/**
 * @brief Test log call.
 */
static void Test(void) {
  BOOST_LOG_FUNCTION();
  BOOST_LOG_TRIVIAL(info) << "Info Log in Test()";
}