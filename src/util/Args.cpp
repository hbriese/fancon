#include "Args.hpp"

fc::Arg::Arg(string name, string short_name, bool potential_value,
             bool needs_value, string value, bool triggered)
    : key(move(name)), short_key(move(short_name)), value(move(value)),
      triggered(triggered), potential_value(potential_value),
      needs_value(needs_value) {}

bool fc::Arg::has_value() const { return !value.empty(); }

fc::Arg::operator bool() const { return triggered; }

map<string, string> fc::Args::short_to_key() const {
  map<string, string> short_to_key;
  for (const auto &[key, a] : from_key) {
    if (!a.short_key.empty())
      short_to_key.insert_or_assign(a.short_key, a.key);
  }

  return short_to_key;
}

pair<string, fc::Arg &> fc::Args::a(Arg &arg) { return {arg.key, arg}; }
