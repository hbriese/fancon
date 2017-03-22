#ifndef FANCON_LOGGING_HPP
#define FANCON_LOGGING_HPP

#include <iostream>   // cout, clog
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/smart_ptr/make_shared_object.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/global_logger_storage.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>

using boost::log::trivial::severity_level;
using llvl = boost::log::trivial::severity_level;

//declares a global logger with a custom initialization
typedef boost::log::sources::severity_logger_mt<boost::log::trivial::severity_level> logger_t;
BOOST_LOG_GLOBAL_LOGGER(logger, logger_t)

#define LOG(lvl)  BOOST_LOG_SEV(logger::get(), lvl)

namespace fancon {
namespace log {
void setLevel(llvl logLevel);
}
}

#endif //FANCON_LOGGING_HPP