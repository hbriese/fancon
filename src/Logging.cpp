#include "Logging.hpp"
#include <fstream>
#include <sys/stat.h>

using std::string;

using namespace boost::log;
namespace expr = boost::log::expressions;

namespace fc::log {
llvl logging_level = llvl::info;
const char *fmt_reset = "\033[0m", *fmt_bold = "\033[1m",
           *fmt_red = "\033[31m", *fmt_red_bold = "\033[1;31m",
           *fmt_green = "\033[32m", *fmt_green_bold = "\033[1;32m";
} // namespace fc::log

template <typename T>
void set_formatter(boost::shared_ptr<sinks::synchronous_sink<T>> sink,
                   bool systemd, bool print_red) {
  const auto severity = expr::attr<trivial::severity_level>(
      aux::default_attribute_names::severity());

  const char *red = (print_red) ? fc::log::fmt_red : "",
             *reset = (print_red) ? fc::log::fmt_reset : "";

  if (!systemd) {
    const auto datetime = expr::format_date_time<boost::posix_time::ptime>(
        aux::default_attribute_names::timestamp(), "%y/%m/%d %H:%M");

    sink->set_formatter(expr::format("%1%%2% [%3%] %4%: %5%%6%") % red %
                        datetime % getpid() % severity % expr::smessage %
                        reset);
  } else {
    sink->set_formatter(expr::format("%1%%2%: %3%%4%") % red % severity %
                        expr::smessage % reset);
  }
}

std::vector<boost::shared_ptr<sinks::sink>> fc::log::generate_sinks() {
  std::vector<boost::shared_ptr<sinks::sink>> sinks_;
  const bool systemd = is_systemd();

  // severity < llvl::info -> cout    custom format
  auto s = add_console_log(std::cout, keywords::auto_flush = true,
                           keywords::filter = (trivial::severity < llvl::info));
  set_formatter(s, systemd, false);
  sinks_.emplace_back(std::move(s));

  // severity == llvl::info -> cout   only message
  s = add_console_log(std::cout, keywords::auto_flush = true,
                      keywords::filter = (trivial::severity == llvl::info));
  sinks_.emplace_back(std::move(s));

  // severity > Don't log to file if running as systemdllvl::info -> cerr custom
  // format
  s = add_console_log(std::cerr, keywords::auto_flush = true,
                      keywords::filter = (trivial::severity > llvl::info));

  set_formatter(s, systemd, true);
  sinks_.emplace_back(std::move(s));

  // Only log to file if running as root whilst not run by systemd
  if (getuid() == 0 && !systemd) {
    try {
      const char *log_path = FANCON_LOCALSTATEDIR "/log/fancon.log";
      auto f = add_file_log(
          keywords::file_name = log_path,
          keywords::open_mode = std::ios_base::app, keywords::max_files = 1,
          keywords::max_size = 524288, // 512KB (512 * 1024)
          keywords::filter = (trivial::severity != llvl::info));
      set_formatter(f, systemd, false);
      sinks_.emplace_back(std::move(f));
    } catch (const std::exception &e) {
      LOG(llvl::error) << e.what();
    }
  }

  return sinks_;
}

std::ostream &fc::log::flush(std::ostream &os) {
  boost::log::core::get()->flush();
  return os;
}

BOOST_LOG_GLOBAL_LOGGER_INIT(logger, logger_t) {
  core::get()->add_global_attribute(aux::default_attribute_names::timestamp(),
                                    attributes::local_clock());

  for (const auto &s : fc::log::generate_sinks())
    core::get()->add_sink(s);

  return logger_t();
}

void fc::log::set_level(const llvl log_level) {
  fc::log::logging_level = log_level;
  boost::log::core::get()->reset_filter();
  boost::log::core::get()->set_filter(trivial::severity >=
                                      boost::ref(fc::log::logging_level));
}

std::optional<llvl> fc::log::str_to_log_level(const std::string &level) {
  if (level == "info")
    return llvl::info;
  if (level == "debug")
    return llvl::debug;
  if (level == "warning")
    return llvl::warning;
  if (level == "error")
    return llvl::error;
  if (level == "trace")
    return llvl::trace;
  if (level == "fatal")
    return llvl::fatal;

  return std::nullopt;
}

bool fc::debugging() { return fc::log::logging_level <= llvl::debug; }

bool fc::is_systemd() {
  // Systemd is running if stderr is in the journal stream
  const char *jstreamEnv = getenv("JOURNAL_STREAM");
  if (!jstreamEnv)
    return false;

  struct stat s;
  fstat(STDERR_FILENO, &s);
  const string fd = std::to_string(s.st_dev) + ':' + std::to_string(s.st_ino);

  // Format of js: `device_id:inode_number ...` - find the file descriptor in js
  const std::string_view js(jstreamEnv);
  return std::search(js.begin(), js.end(), fd.begin(), fd.end()) != js.end();
}
