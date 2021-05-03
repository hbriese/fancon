#ifdef FANCON_NVIDIA_NVML_SUPPORT
#ifndef FANCON_NVIDIANVML_HPP
#define FANCON_NVIDIANVML_HPP

#include <dlfcn.h>
#include <nvml.h>

#include "Fan.hpp"
#include "SensorInterface.hpp"
#include "Util.hpp"

namespace fc::NV {
class Nvml {
public:
  Nvml();
  ~Nvml();

  void enumerate(vector<unique_ptr<fc::Fan>> &fans,
                 vector<unique_ptr<fc::Sensor>> &sensors);

private:
  void *handle;
  bool found;

  static void print_err(nvmlReturn_t status, const char *prefix = "");

  bool on_same_board(vector<nvmlDevice_t> &devices, nvmlDevice_t &dev);

  decltype(nvmlInit_v2) *init;
  decltype(nvmlShutdown) *shutdown;
  decltype(nvmlDeviceGetHandleByIndex_v2) *device_get_handle_by_index;
  decltype(nvmlDeviceGetHandleByUUID) *device_get_handle_by_uuid;
  decltype(nvmlDeviceGetUUID) *device_get_uuid;
  decltype(nvmlDeviceGetCount_v2) *device_get_count;
  decltype(nvmlDeviceOnSameBoard) *device_on_same_board;
  decltype(nvmlDeviceGetName) *device_get_name;
};

extern Nvml nvml;
} // namespace fc::NV

#endif // FANCON_NVIDIANVML_HPP
#endif // FANCON_NVIDIA_NVML_SUPPORT
