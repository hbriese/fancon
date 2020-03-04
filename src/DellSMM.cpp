#include "DellSMM.hpp"

using namespace fc;

namespace fc::SMM {
short smm_found{-1};
InitState init_state{InitState::NOT_INITIALIZED};
} // namespace fc::SMM

bool SMM::found() {
  if (smm_found == 0 || smm_found == 1)
    return smm_found;

  for (const auto c : {SMM::SMM_GET_DELL_SIG_1, SMM::SMM_GET_DELL_SIG_2}) {
    struct smm_regs regs {};
    regs.eax = c;

    if (!i8k_smm(regs)) {
      smm_found = 0;
      return false;
    }

    if (regs.eax == SMM::DIAG_SIG && regs.edx == SMM::DELL_SIG) {
      smm_found = 1;
      return true;
    }
  }

  smm_found = 0;
  return false;
}

bool SMM::is_smm_dell(const string_view &sensor_chip_name) {
  const string_view prefix = "dell_smm";
  if (prefix != sensor_chip_name.substr(0, prefix.length()))
    return false;

  return SMM::found();
}

int SMM::fan_status(int fan) {
  smm_regs regs{};
  regs.eax = SMM::SMM_GET_FAN;
  regs.ebx = fan & 0xff;

  if (!i8k_smm(regs)) {
    LOG(llvl::error) << "Failed to get SMM fan status";
    return SMM::FAN_NOT_FOUND;
  }
  return regs.eax & 0xff;
}

// vector<int> SMM::enumerate_fans() {
//  vector<int> fans;
//  for (int i = 0; SMM::fan_status(i) != SMM::FAN_NOT_FOUND; ++i)
//    fans.push_back(i);
//
//  return fans;
//}
//
// Rpm SMM::get_rpm(int fan) {
//  smm_regs regs{};
//  regs.eax = SMM::SMM_GET_SPEED;
//  regs.ebx = fan & 0xff;
//
//  if (const int res = i8k_smm(regs); res < 0) {
//    LOG(llvl::error) << "Failed to get SMM fan RPM: " << res;
//    return res;
//  }
//  return regs.eax & 0xffff;
//}

bool SMM::init_ioperms() {
  if (SMM::init_state == InitState::NOT_INITIALIZED) {
    init_state = (ioperm(0xb2, 4, 1) == 0 && ioperm(0x84, 4, 1) == 0)
                     ? InitState::SUCCESSFUL
                     : InitState::FAILED;
  }

  return SMM::init_state == InitState::SUCCESSFUL;
}

bool SMM::i8k_smm(smm_regs &regs) {
  if (!init_ioperms())
    return false;

  const auto eax = regs.eax;
  int rc;

#if defined(__X86_64__) || defined(__amd64__)
  asm volatile("pushq %%rax\n\t"
               "movl 0(%%rax),%%edx\n\t"
               "pushq %%rdx\n\t"
               "movl 4(%%rax),%%ebx\n\t"
               "movl 8(%%rax),%%ecx\n\t"
               "movl 12(%%rax),%%edx\n\t"
               "movl 16(%%rax),%%esi\n\t"
               "movl 20(%%rax),%%edi\n\t"
               "popq %%rax\n\t"
               "out %%al,$0xb2\n\t"
               "out %%al,$0x84\n\t"
               "xchgq %%rax,(%%rsp)\n\t"
               "movl %%ebx,4(%%rax)\n\t"
               "movl %%ecx,8(%%rax)\n\t"
               "movl %%edx,12(%%rax)\n\t"
               "movl %%esi,16(%%rax)\n\t"
               "movl %%edi,20(%%rax)\n\t"
               "popq %%rdx\n\t"
               "movl %%edx,0(%%rax)\n\t"
               "pushfq\n\t"
               "popq %%rax\n\t"
               "andl $1,%%eax\n"
               : "=a"(rc)
               : "a"(&regs)
               : "%ebx", "%ecx", "%edx", "%esi", "%edi", "memory");
#elif defined(__i386__) || defined(__X86__)
  asm volatile("pushl %%eax\n\t"
               "movl 0(%%eax),%%edx\n\t"
               "push %%edx\n\t"
               "movl 4(%%eax),%%ebx\n\t"
               "movl 8(%%eax),%%ecx\n\t"
               "movl 12(%%eax),%%edx\n\t"
               "movl 16(%%eax),%%esi\n\t"
               "movl 20(%%eax),%%edi\n\t"
               "popl %%eax\n\t"
               "out %%al,$0xb2\n\t"
               "out %%al,$0x84\n\t"
               "xchgl %%eax,(%%esp)\n\t"
               "movl %%ebx,4(%%eax)\n\t"
               "movl %%ecx,8(%%eax)\n\t"
               "movl %%edx,12(%%eax)\n\t"
               "movl %%esi,16(%%eax)\n\t"
               "movl %%edi,20(%%eax)\n\t"
               "popl %%edx\n\t"
               "movl %%edx,0(%%eax)\n\t"
               "lahf\n\t"
               "shrl $8,%%eax\n\t"
               "andl $1,%%eax\n"
               : "=a"(rc)
               : "a"(&regs)
               : "%ebx", "%ecx", "%edx", "%esi", "%edi", "memory");
#else
  return false;
#endif

  return (rc == 0 && (regs.eax & 0xffff) != 0xffff && regs.eax != eax);
}

bool SMM::smm_send(SMM::Cmd cmd) {
  SMM::smm_regs regs{};
  regs.eax = cmd;

  return SMM::i8k_smm(regs);
}
