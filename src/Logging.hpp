#ifndef FANCON_LOGGING_HPP
#define FANCON_LOGGING_HPP

#ifndef FANCON_LOCALSTATEDIR
#define FANCON_LOCALSTATEDIR "/var"
#endif // FANCON_LOCALSTATEDIR

#include <algorithm> // find
#include <boost/filesystem.hpp>
#include <boost/log/attributes/clock.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sources/global_logger_storage.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <iostream> // cout, cerr
#include <iterator> // next
#include <memory>
#include <optional>
#include <vector>

using llvl = boost::log::trivial::severity_level;
using logger_t = boost::log::sources::severity_logger_mt<llvl>;

// Declare a global logger with a custom initialization
BOOST_LOG_GLOBAL_LOGGER(logger, logger_t)

#define LOG(lvl) BOOST_LOG_SEV(logger::get(), lvl)

namespace fc {
bool debugging();
bool is_systemd();

namespace log {
extern llvl logging_level;
extern const char *output_reset, *output_bold, *output_red, *output_bold_red;

std::vector<boost::shared_ptr<boost::log::sinks::sink>> generate_sinks();
std::ostream &flush(std::ostream &os);
void set_level(const llvl log_level);
std::optional<llvl> str_to_log_level(const std::string &level);
} // namespace log
} // namespace fc

#endif // FANCON_LOGGING_HPP
