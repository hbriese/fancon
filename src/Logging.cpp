#include "Logging.hpp"
#include <fstream>
#include <sys/stat.h>

using std::string;

using namespace boost::log;
namespace expr = boost::log::expressions;

namespace {
/// \return True if the file descriptor's device ID and inode number is being accessed by the systemd journal
bool systemdAccessing(int fileDescriptor) {
  // Get systemd journal file access list
  const char *jstreamEnv = getenv("JOURNAL_STREAM");
  if (!jstreamEnv)
    return false;
  string js(jstreamEnv);

  // Format of js: `device_id:inode_number`
  struct stat s;
  fstat(fileDescriptor, &s);
  string fd = std::to_string(s.st_dev) + ':' + std::to_string(s.st_ino);

  // Return true if fd is found in js
  auto it = std::search(js.begin(), js.end(), fd.begin(), fd.end());
  return it != js.end();
}

// TODO: replace with function returning a tuple
template<typename T>
void setFormatter(boost::shared_ptr<boost::log::sinks::synchronous_sink<T>> sink, bool systemd) {
  if (!systemd)
    sink->set_formatter(
        expr::format("%1% [%2%] <%3%> %4%")
            % expr::format_date_time<boost::posix_time::ptime>(aux::default_attribute_names::timestamp(), "%m/%d %H:%M")
            % getpid()
            % expr::attr<boost::log::trivial::severity_level>(aux::default_attribute_names::severity())
            % expr::smessage
    );
  else
    sink->set_formatter(
        expr::format("<%1%> %2%")
            % expr::attr<boost::log::trivial::severity_level>(aux::default_attribute_names::severity())
            % expr::smessage
    );
}
}

BOOST_LOG_GLOBAL_LOGGER_INIT(logger, logger_t) {
  logger_t lg;
  core::get()->add_global_attribute(aux::default_attribute_names::timestamp(), attributes::local_clock());

  bool systemd = systemdAccessing(STDERR_FILENO);

  // Add cout & cerr sink if they are being by a TTY or systemd's journal
  if (systemd || isatty(STDERR_FILENO)) {
    // llvl::info -> cout
    auto coutSink = add_console_log(std::cout, keywords::auto_flush = true,  // Unspecified format is "%Message%"
                                    keywords::filter = (trivial::severity == llvl::info));
    core::get()->add_sink(coutSink);

    // != llvl::info -> cerr
    auto cerrSink = add_console_log(std::cerr, keywords::auto_flush = true,
                                    keywords::filter = (trivial::severity != llvl::info));
    // Systemd-journal has it's own format attributes, so use a simpler formatter when outputting there
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