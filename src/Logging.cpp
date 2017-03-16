#include "Logging.hpp"

BOOST_LOG_GLOBAL_LOGGER_INIT(logger, logger_t) {
  using namespace boost::log;
  namespace expr = boost::log::expressions;

  logger_t lg;
  core::get()->add_global_attribute(aux::default_attribute_names::timestamp(), attributes::local_clock());

  auto formatter =
      // Time [PID] <Severity> - Message
      expr::format("%1% [%2%] <%3%> - %4%")
          % expr::format_date_time<boost::posix_time::ptime>(aux::default_attribute_names::timestamp(),
                                                             "%m/%d %H:%M:%S")
          % getpid()
          % expr::attr<boost::log::trivial::severity_level>(aux::default_attribute_names::severity())
          % expr::smessage;

  // error console sink
  auto s = add_console_log(std::clog, keywords::format = formatter);
  s->set_filter(trivial::severity >= llvl::info);   // default filter

  core::get()->add_sink(s);

  return lg;
}

void fancon::log::setLevel(llvl logLevel) {
  boost::log::core::get()->set_filter(boost::log::trivial::severity >= logLevel);
}