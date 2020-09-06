#ifndef FANCON_DELLSMM_HPP
#define FANCON_DELLSMM_HPP

#include <sys/io.h>

#include "FanInterface.hpp"
#include "Util.hpp"

using DellID = uint;

// Based on Linux dell-smm-hwmon
// https://github.com/torvalds/linux/blob/master/drivers/hwmon/dell-smm-hwmon.c
// And Clopez dellfan https://github.com/clopez/dellfan/blob/master/dellfan.c
namespace fc::SMM {
extern optional<bool> smm_found, io_initialized;

struct smm_regs {
  unsigned int eax;
  unsigned int ebx __attribute__((packed));
  unsigned int ecx __attribute__((packed));
  unsigned int edx __attribute__((packed));
  unsigned int esi __attribute__((packed));
  unsigned int edi __attribute__((packed));
};

enum Cmd : uint {
  SMM_SET_FAN = 0x01a3,
  SMM_GET_FAN = 0x00a3,
  SMM_GET_SPEED = 0x02a3,
  SMM_GET_FAN_TYPE = 0x03a3,
  SMM_GET_NOM_SPEED = 0x04a3,
  SMM_GET_TOLERANCE = 0x05a3,
  SMM_GET_TEMP = 0x10a3,
  SMM_GET_TEMP_TYPE = 0x11a3,
  SMM_MANUAL_CONTROL_1 = 0x30a3,
  SMM_AUTO_CONTROL_1 = 0x31a3,
  SMM_MANUAL_CONTROL_3 = 0x32a3,
  SMM_AUTO_CONTROL_3 = 0x33a3,
  SMM_MANUAL_CONTROL_2 = 0x34a3,
  SMM_AUTO_CONTROL_2 = 0x35a3,
  SMM_GET_DELL_SIG_1 = 0xfea3,
  SMM_GET_DELL_SIG_2 = 0xffa3
};

enum Result : uint {
  DELL_SIG = 0x44454C4C,
  DIAG_SIG = 0x44494147,
  FAN_NOT_FOUND = 0xff
};

bool found();
bool is_smm_dell(const string_view &sensor_chip_name);

uint fan_status(DellID fan);
// vector<int> enumerate_fans();
// Rpm get_rpm(int fan);

bool init_ioperms();
bool i8k_smm(smm_regs &regs);
bool smm_send(SMM::Cmd cmd);
} // namespace fc::SMM

#endif // FANCON_DELLSMM_HPP
