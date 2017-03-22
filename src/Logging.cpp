#include "Logging.hpp"

BOOST_LOG_GLOBAL_LOGGER_INIT(logger, logger_t) {
  using namespace boost::log;
  namespace expr = boost::log::expressions;

  logger_t lg;
  core::get()->add_global_attribute(aux::default_attribute_names::timestamp(), attributes::local_clock());

  // Time [PID] <Severity> - Message
  auto formatter = expr::format("%1% [%2%] <%3%> - %4%")
      % expr::format_date_time<boost::posix_time::ptime>(aux::default_attribute_names::timestamp(), "%m/%d %H:%M")
      % getpid()
      % expr::attr<boost::log::trivial::severity_level>(aux::default_attribute_names::severity())
      % expr::smessage;

  // Send info to std::cout, and everything else to std::clog
  auto coutSink = add_console_log(std::cout, keywords::auto_flush = true,
                                  keywords::filter = (trivial::severity == llvl::info));
  auto clogSink = add_console_log(std::clog, keywords::format = formatter,
                                  keywords::filter = (trivial::severity != llvl::info));

  core::get()->add_sink(coutSink);
  core::get()->add_sink(clogSink);

  // Define default filter level to >= info
  core::get()->set_filter(trivial::severity >= llvl::info);

  return lg;
}

void fancon::log::setLevel(llvl logLevel) {
  boost::log::core::get()->reset_filter();
  boost::log::core::get()->set_filter(boost::log::trivial::severity >= logLevel);
}