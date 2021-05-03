#ifdef FANCON_NVIDIA_SUPPORT
#ifdef FANCON_NVIDIA_NVML_SUPPORT

#include "NvidiaNvml.hpp"

namespace fc::NV {
Nvml nvml;
}

#define GET_SYMBOL(_proc, _name)                                               \
  _proc = (decltype(_proc))(dlsym(handle, _name));                             \
  if (!_proc)                                                                  \
    LOG(llvl::error) << "Failed to load DL symbol: " << _name;

fc::NV::Nvml::Nvml() : handle(dlopen("libnvidia-ml.so", RTLD_LAZY)) {
  GET_SYMBOL(init, "nvmlInit_v2");
  GET_SYMBOL(shutdown, "nvmlShutdown");
  GET_SYMBOL(device_get_handle_by_index, "nvmlDeviceGetHandleByIndex_v2");
  GET_SYMBOL(device_get_handle_by_uuid, "nvmlDeviceGetHandleByUUID");
  GET_SYMBOL(device_get_uuid, "nvmlDeviceGetUUID");
  GET_SYMBOL(device_get_count, "nvmlDeviceGetCount_v2");
  GET_SYMBOL(device_on_same_board, "nvmlDeviceOnSameBoard");
  GET_SYMBOL(device_get_name, "nvmlDeviceGetName");

  const nvmlReturn_t ret = init();
  found = ret == NVML_SUCCESS;
  if (ret != NVML_SUCCESS && ret != NVML_ERROR_DRIVER_NOT_LOADED) {
    const char *err = (ret == NVML_ERROR_NO_PERMISSION)
                          ? "insufficient permissions"
                          : "unknown";
    LOG(llvl::error) << "NVML init error: " << err;
  }
}

fc::NV::Nvml::~Nvml() {
  if (found) {
    const nvmlReturn_t ret = shutdown();
    if (ret != NVML_SUCCESS)
      print_err(ret);
  }
}

void fc::NV::Nvml::enumerate(vector<unique_ptr<fc::Fan>> &fans,
                             vector<unique_ptr<fc::Sensor>> &sensors) {
  uint num_gpus;
  nvmlReturn_t ret = device_get_count(&num_gpus);
  if (ret != NVML_SUCCESS || num_gpus == 0)
    return print_err(ret);

  vector<nvmlDevice_t> devices;
  for (uint i = 0; i < num_gpus; ++i) {
    nvmlDevice_t dev;
    ret = device_get_handle_by_index(i, &dev);
    if (ret != NVML_SUCCESS) {
      print_err(ret);
      continue;
    }

    if (on_same_board(devices, dev))
      continue;
    devices.push_back(dev);

    char *uuid_ptr = nullptr;
    ret = device_get_uuid(dev, uuid_ptr, NVML_DEVICE_UUID_BUFFER_SIZE);
    if (ret != NVML_SUCCESS) {
      print_err(ret);
      continue;
    }
    const string uuid(uuid_ptr); // uuid_ptr is NULL terminated

    char *name_ptr = nullptr;
    ret = device_get_name(dev, name_ptr, NVML_DEVICE_NAME_BUFFER_SIZE);
    if (ret != NVML_SUCCESS)
      print_err(ret);
    const string name =
        (name_ptr) ? name_ptr : string("nvidia:") + to_string(i);
  }
}

void fc::NV::Nvml::print_err(nvmlReturn_t status, const char *prefix) {
  const char *err;
  switch (status) {
  case NVML_SUCCESS:
    return;
  case NVML_ERROR_UNKNOWN:
    err = "unknown";
    break;
  case NVML_ERROR_NO_PERMISSION:
    err = "insufficient permissions";
    break;
  case NVML_ERROR_UNINITIALIZED:
    err = "library uninitialized";
    break;
  case NVML_ERROR_INVALID_ARGUMENT:
    err = "invalid argument";
    break;
  case NVML_ERROR_INSUFFICIENT_POWER:
    err = "device has insufficient power";
    break;
  case NVML_ERROR_IRQ_ISSUE:
    err = "kernel interrupt issue";
    break;
  case NVML_ERROR_GPU_IS_LOST:
    err = "device is inaccessible";
    break;
  case NVML_ERROR_NOT_FOUND:
    err = "device not found";
    break;
  case NVML_ERROR_INSUFFICIENT_SIZE:
    err = "insufficient buffer size";
    break;
  default:
    err = (string("UNHANDLED (") + to_string(status) + ")").c_str();
  }

  if (!prefix)
    prefix = "NVML error: ";

  LOG(llvl::error) << prefix << err;
}

bool fc::NV::Nvml::on_same_board(vector<nvmlDevice_t> &devices,
                                 nvmlDevice_t &dev) {
  nvmlReturn_t ret;
  int same_board = 0;
  for (const auto &d : devices) {
    ret = device_on_same_board(dev, d, &same_board);
    if (ret != NVML_SUCCESS) {
      print_err(ret);
      continue;
    }

    if (same_board > 0)
      return true;
  }

  return false;
}

#undef GET_SYMBOL

#endif // FANCON_NVIDIA_NVML_SUPPORT
#endif // FANCON_NVIDIA_SUPPORT
