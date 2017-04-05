#include "Logging.hpp"

using namespace boost::log;
namespace expr = boost::log::expressions;

namespace {
bool systemdListening(int fileNumber) {
  const char *jstreamEnv = getenv("JOURNAL_STREAM");
  if (!jstreamEnv)
    return false;

  std::string jsEnv(jstreamEnv), stderrFN = std::to_string(fileNumber);
  for (std::string::iterator it = jsEnv.begin(), endIt = jsEnv.end(), sep;
       (sep = std::find(it, endIt, ':')) != endIt; it = std::next(sep)) {
    if (std::string(it, sep) == stderrFN)
      return true;
  }

  return false;
}

// TODO: replace with function returning a tuple
template<typename T>
void setFormatter(boost::shared_ptr<boost::log::sinks::synchronous_sink<T>> sink, bool systemd) {
  if (!systemd)
    sink->set_formatter(
        expr::format("%1% [%2%] <%3%> - %4%")
            % expr::format_date_time<boost::posix_time::ptime>(aux::default_attribute_names::timestamp(), "%m/%d %H:%M")
            % getpid()
            % expr::attr<boost::log::trivial::severity_level>(aux::default_attribute_names::severity())
            % expr::smessage
    );
  else
    sink->set_formatter(
        expr::format("<%1%> - %2%")
            % expr::attr<boost::log::trivial::severity_level>(aux::default_attribute_names::severity())
            % expr::smessage
    );
}
}

BOOST_LOG_GLOBAL_LOGGER_INIT(logger, logger_t) {
  logger_t lg;
  core::get()->add_global_attribute(aux::default_attribute_names::timestamp(), attributes::local_clock());

  bool systemd = systemdListening(STDERR_FILENO);

  // Systemd-journal has it's own format attributes, so use a simpler formatter if outputting there
  if (systemd || (isatty(STDERR_FILENO) > 0)) {
    // llvl::info -> cout
    auto coutSink = add_console_log(std::cout, keywords::auto_flush = true,  // Unspecified format is "%Message%"
                                    keywords::filter = (trivial::severity == llvl::info));
    core::get()->add_sink(coutSink);

    // non llvl::info -> cerr
    auto cerrSink = add_console_log(std::cerr, keywords::auto_flush = true,
                                    keywords::filter = (trivial::severity != llvl::info));
    setFormatter(cerrSink, systemd);
    core::get()->add_sink(cerrSink);
  }

  // Write to file when not using systemd-journal
  if (!systemd) {
    auto fileSink = add_file_log(keywords::file_name = fancon::log::log_path,
                                 keywords::open_mode = std::ios_base::app, keywords::max_files = 1,
                                 keywords::max_size = 524288,   // 512KB (512 * 1024)
                                 keywords::filter = (trivial::severity != llvl::info)
    );
    setFormatter(fileSink, systemd);
    core::get()->add_sink(fileSink);
  }

  return lg;
}

void fancon::log::setLevel(llvl logLevel) {
  boost::log::core::get()->reset_filter();
  boost::log::core::get()->set_filter(boost::log::trivial::severity >= logLevel);
}