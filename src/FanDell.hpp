#ifndef FANCON_FANDELL_HPP
#define FANCON_FANDELL_HPP

#include "DellSMM.hpp"
#include "FanSysfs.hpp"

namespace fc {
class FanDell : public fc::FanSysfs {
public:
  FanDell() = default;
  FanDell(string label_, const path &adapter_path_, uint id_);
  ~FanDell() override;

  bool enable_control() const override;
  bool disable_control() const override;
  bool valid() const override;
  virtual DevType type() const override;

  void to(fc_pb::Fan &f) const override;

private:
  void test_driver_enable_flag() override;
  SMM::Cmd mode(bool enable) const;
};
} // namespace fc

#endif // FANCON_FANDELL_HPP
