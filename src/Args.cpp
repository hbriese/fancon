#include "Args.hpp"

fc::Arg::Arg(string name, string short_name, bool has_value, bool needs_value,
             string value, bool triggered)
    : key(move(name)), short_key(move(short_name)), value(move(value)),
      triggered(triggered), potential_value(has_value),
      needs_value(needs_value) {}

bool fc::Arg::has_value() const { return !value.empty(); }

fc::Arg::operator bool() const { return triggered; }

pair<string, fc::Arg &> fc::Args::a(Arg &arg) { return {arg.key, arg}; }
