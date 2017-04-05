#ifndef FANCON_LOGGING_HPP
#define FANCON_LOGGING_HPP

#include <algorithm>  // find
#include <iostream>   // cout, cerr
#include <iterator>   // next
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/global_logger_storage.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/attributes/clock.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>

using boost::log::trivial::severity_level;
using llvl = boost::log::trivial::severity_level;

// Declares a global logger with a custom initialization
using logger_t = boost::log::sources::severity_logger_mt<boost::log::trivial::severity_level>;
BOOST_LOG_GLOBAL_LOGGER(logger, logger_t)

#define LOG(lvl)  BOOST_LOG_SEV(logger::get(), lvl)

namespace fancon {
namespace log {
constexpr const char *log_path = "/var/log/fancon.log";

void setLevel(llvl logLevel);
}
}

#endif //FANCON_LOGGING_HPP