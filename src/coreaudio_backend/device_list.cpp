// libstdaudio
// Copyright (c) 2018 - Timur Doumler

#include "device_list.h"

#ifdef LIBSTDAUDIO_BACKEND_COREAUDIO
#include <cctype>
#include <string>
#include <iostream>
#include <vector>
#include <AudioToolbox/AudioToolbox.h>
#include "../_device_impl.h"

LIBSTDAUDIO_NAMESPACE_BEGIN

namespace {
  class _coreaudio_util {
  public:
    static bool check_error(OSStatus error) {
      if (error == noErr)
        return true;

      _log_message(_format_error(error));
      return false;
    }

  private:
    static std::string _format_error(OSStatus error)
    {
      char c[4];
      *(uint32_t *)(c) = CFSwapInt32HostToBig(uint32_t(error));
      if (_is_four_character_code(c))
        return std::string{"\'"} + c + "\'";

      return std::to_string(error);
    }

    static bool _is_four_character_code(char* c) {
      for (int i : {0, 1, 2, 3})
        if (!std::isprint(c[i]))
          return false;

      return true;
    }

    static void _log_message(const std::string& s) {
      // TODO: only do this in DEBUG
      std::cerr << "coreaudio_backend error: " << s << std::endl;
    }
  };

  class _coreaudio_device_impl : public _device_impl {
  public:
    _coreaudio_device_impl(device& owner)
    : _device_impl(owner) {}

  private:
    void process() override {
      // TODO: implement
    }
  };

  class _coreaudio_device_enumerator {
  public:
    static _coreaudio_device_enumerator& get_instance() {
      static _coreaudio_device_enumerator cde;
      return cde;
    }

    std::forward_list<device> get_input_device_list_impl() {
      const auto device_ids = _get_device_ids();

      // TODO: implement
      return {};
    }

    std::forward_list<device> get_output_device_list_impl() {
      const auto device_ids = _get_device_ids();

      // TODO: implement
      return {};
    }

  private:
    _coreaudio_device_enumerator() = default;

    static std::vector<AudioDeviceID> _get_device_ids() {
      AudioObjectPropertyAddress pa = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
      };

      uint32_t dataSize = 0;
      if (!_coreaudio_util::check_error(AudioObjectGetPropertyDataSize(
        kAudioObjectSystemObject, &pa,
        0, nullptr, &dataSize)))
        return {};

      const auto device_count = static_cast<uint32_t>(dataSize / sizeof(AudioDeviceID));
      if (device_count == 0)
        return {};

      std::vector<AudioDeviceID> device_ids(device_count);

      if (!_coreaudio_util::check_error(AudioObjectGetPropertyData(
        kAudioObjectSystemObject, &pa,
        0, nullptr, &dataSize, device_ids.data())))
        return {};

      return device_ids;
    }
  };
}

device_list& get_input_device_list() {
  auto& enumerator = _coreaudio_device_enumerator::get_instance();
  static device_list in_devices{enumerator.get_input_device_list_impl()};
  return in_devices;
}

device_list& get_output_device_list() {
  auto& enumerator = _coreaudio_device_enumerator::get_instance();
  static device_list out_devices{enumerator.get_output_device_list_impl()};
  return out_devices;
}

LIBSTDAUDIO_NAMESPACE_END
#endif // LIBSTDAUDIO_BACKEND_COREAUDIO