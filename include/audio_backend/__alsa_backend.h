// libstdaudio
// Copyright (c) 2019 - Conrad Jones
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#pragma once

#include <cctype>
#include <string>
#include <iostream>
#include <vector>
#include <functional>
#include <memory>
#include <forward_list>
#include <map>
#include <atomic>
#include <thread>
#include <alsa/asoundlib.h>

_LIBSTDAUDIO_NAMESPACE_BEGIN

// -----------------------------------------------------------------------------
struct __snd_ctl_close {
  void operator()(snd_ctl_t* p) {
    snd_ctl_close(p);
  }
};
using __snd_ctl_t_raai = unique_ptr<snd_ctl_t, __snd_ctl_close>;

struct __snd_ctl_card_info_free {
  void operator()(snd_ctl_card_info_t* ptr) {
    snd_ctl_card_info_free(ptr);
  }
};
using __snd_ctl_card_info_raai = unique_ptr<snd_ctl_card_info_t, __snd_ctl_card_info_free>;

struct __snd_pcm_info_free {
  void operator()(snd_pcm_info_t* pcmInfo) {
    snd_pcm_info_free(pcmInfo);
  }
};

using __snd_pcm_info_t_raai = unique_ptr<snd_pcm_info_t, __snd_pcm_info_free>;

struct __snd_pcm_hw_params_free {
  void operator()(snd_pcm_hw_params_t* ptr) {
    snd_pcm_hw_params_free(ptr);
  }
};

using __snd_pcm_hw_params_raai = unique_ptr<snd_pcm_hw_params_t, __snd_pcm_hw_params_free>;

struct __snd_pcm_t_free {
  void operator()(snd_pcm_t* pcm) {
    snd_pcm_close(pcm);
  }
};

using __snd_pcm_t_raai = unique_ptr<snd_pcm_t, __snd_pcm_t_free>;

struct __snd_pcm_chmap_query_free {
  void operator()(snd_pcm_chmap_query_t** ptr) {
    snd_pcm_free_chmaps(ptr);
  }
};

using __snd_pcm_chmap_query_raai = unique_ptr<snd_pcm_chmap_query_t*, __snd_pcm_chmap_query_free>;

struct __snd_pcm_chmap_t_free {
  void operator()(snd_pcm_chmap_t* ptr) {
    free(ptr);
  }
};

using __snd_pcm_chmap_t_raai = unique_ptr<snd_pcm_chmap_t, __snd_pcm_chmap_t_free>;


struct __snd_device_name_hint_free {
  void operator()(void** ptr)
  {
    snd_device_name_free_hint(ptr);
  }
};

using __snd_device_name_hint_raai = unique_ptr<void*, __snd_device_name_hint_free>;

struct __snd_pcm_format_mask_free {
  void operator()(snd_pcm_format_mask_t* formatMask)
  {
    snd_pcm_format_mask_free(formatMask);
  }
};

using __snd_pcm_format_mask_raai = unique_ptr<snd_pcm_format_mask_t, __snd_pcm_format_mask_free>;

struct __snd_pcm_chmap_free {
  void operator()(snd_pcm_chmap_t* p)
  {
    free(p);
  }
};

using __snd_pcm_chmap_raai = unique_ptr<snd_pcm_chmap_t, __snd_pcm_chmap_free>;

__snd_pcm_chmap_raai __make_snd_pcm_chmap(size_t count)
{
  snd_pcm_chmap_t * chmap = static_cast<snd_pcm_chmap_t*>(malloc(sizeof(int) + sizeof(int) * count));
  chmap->channels = count;
  //TODO support other channel maps
  if (count == 2) {
    chmap->pos[0] = SND_CHMAP_FL;
    chmap->pos[1] = SND_CHMAP_FR;
  } else if (count == 1) {
    chmap->pos[0] = SND_CHMAP_FC;
  }
  return __snd_pcm_chmap_raai(chmap);
}

struct __snd_pcm_sw_params_free {
  void operator()(snd_pcm_sw_params_t* ptr)
  {
    snd_pcm_sw_params_free(ptr);
  }
};

using __snd_pcm_sw_params_raai = unique_ptr<snd_pcm_sw_params_t, __snd_pcm_sw_params_free>;

__snd_pcm_sw_params_raai __make_snd_pcm_sw_params() {
  snd_pcm_sw_params_t *params = nullptr;
  snd_pcm_sw_params_malloc(&params);
  return __snd_pcm_sw_params_raai(params);
}

// -----------------------------------------------------------------------------

template <typename V, typename... T>
constexpr auto __array_of(T&&... t)
-> std::array<V, sizeof...(T)>
{
  return { { std::forward<T>(t)... } };
}

// TODO: make __coreaudio_sample_type flexible according to the recommendation (see AudioSampleType).
using __coreaudio_native_sample_type = int16_t;

struct __alsa_stream_config {
//  AudioBufferList
  int input_config = {0};
//  AudioBufferList
  int output_config = {0};
};

class __alsa_util {
public:
  static bool check_error(int error) {
    if (error == 0)
      return true;

    _log_message(_format_error(error));
    assert(false);
    return false;
  }

private:
  static string _format_error(int error) {
    return snd_strerror(error);
  }

  static void _log_message(const string& s) {
    // TODO: only do this in DEBUG
    cerr << "__alsa_backend error: " << s << endl;
  }
};

class __alsa_pollfd {

private:
  int _poll_fd_count = {0};
  std::vector<pollfd> _poll_fd;
  snd_pcm_t* _pcm;

  friend std::optional<__alsa_pollfd> __make_alsa_pollfd(snd_pcm_t* pcm);

public:
  __alsa_pollfd() = default;

  int wait()
  {
    while (true) {
      int result = poll(_poll_fd.data(), _poll_fd.size(), -1);
      if (result < 0) {
        return -1;
      }
      unsigned short revents;

      result = snd_pcm_poll_descriptors_revents(_pcm, _poll_fd.data(), _poll_fd.size() - 1, &revents);
      if (result < 0) {
        return -1;
      }
      if (revents & (POLLERR | POLLNVAL | POLLHUP)) {
        return 0;
      }
      if (revents & POLLOUT)
        return 0;
    }
  }
};

std::optional<__alsa_pollfd> __make_alsa_pollfd(snd_pcm_t* pcm) {
  __alsa_pollfd pollfd;

  pollfd._pcm = pcm;
  pollfd._poll_fd_count = snd_pcm_poll_descriptors_count(pcm);
  if (pollfd._poll_fd_count <= 0)
    return nullopt;

  pollfd._poll_fd.resize(pollfd._poll_fd_count + 1);
  pollfd._poll_fd[0] = {};
  pollfd._poll_fd[1] = {};

  int result = snd_pcm_poll_descriptors(pcm, pollfd._poll_fd.data(), pollfd._poll_fd_count);
  if (result < 0)
    return nullopt;

  int poll_exit_pipe_fd[2];
  if (pipe2(poll_exit_pipe_fd, O_NONBLOCK) != 0) {
    return nullopt;
  }
  pollfd._poll_fd[pollfd._poll_fd_count].fd = poll_exit_pipe_fd[0];
  pollfd._poll_fd[pollfd._poll_fd_count].events = POLLIN;
  return pollfd;
}

struct audio_device_exception : public runtime_error {
  explicit audio_device_exception(const char* what)
    : runtime_error(what) {
  }
};

struct __alsa_audio_device_id
{
  int card_id {-1};
  int device_id {-1};

  string get_card_str_id() const {
    return string("hw:") + to_string(card_id);
  }

  __snd_ctl_card_info_raai get_card_info() const {
    __snd_ctl_t_raai snd_ctl_handle = card_handle();
    snd_ctl_card_info_t* card_info_raw = {nullptr};

    snd_ctl_card_info_malloc(&card_info_raw);
    return __snd_ctl_card_info_raai(card_info_raw);
  }

  __snd_pcm_info_t_raai get_pcm_info() const {
    snd_pcm_info_t* pcm_info_raw = {nullptr};
    snd_pcm_info_malloc(&pcm_info_raw);

    if (pcm_info_raw) {
      snd_pcm_info_set_device(pcm_info_raw, device_id);
      snd_pcm_info_set_subdevice(pcm_info_raw, 0);
    }

    return __snd_pcm_info_t_raai(pcm_info_raw);
  }

  string get_card_name(__snd_ctl_t_raai& snd_ctl_handle) const {
    __snd_ctl_card_info_raai card_info = get_card_info();
    if (!card_info)
      return string();

    int result = snd_ctl_card_info(snd_ctl_handle.get(), card_info.get());
    if (result < 0)
      return string();

    string card_name = snd_ctl_card_info_get_name(card_info.get());
    return card_name;
  }

  string get_device_id_str() const {
    return get_card_str_id() + ',' + to_string(device_id);
  }

  string get_device_name() const {
    __snd_ctl_t_raai snd_ctl_handle = card_handle();

    __snd_pcm_info_t_raai pcm_info = get_pcm_info();
    int result = snd_ctl_pcm_info(snd_ctl_handle.get(), pcm_info.get());
    if (result < 0)
      return string();

    string card_name = get_card_name(snd_ctl_handle);

    const char * pcm_name = snd_pcm_info_get_name(pcm_info.get());
    if (!pcm_name)
      return string();

    return card_name + ", " + pcm_name;
  }

  __snd_pcm_t_raai get_pcm() const {
    snd_pcm_t* pcm_t_raw {};
    __alsa_util::check_error(snd_pcm_open(&pcm_t_raw, get_device_id_str().c_str(), SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK | SND_PCM_NO_AUTO_RESAMPLE | SND_PCM_NO_AUTO_CHANNELS | SND_PCM_NO_AUTO_FORMAT));
    return __snd_pcm_t_raai(pcm_t_raw);
  }

  __snd_pcm_hw_params_raai get_hw_params() const {
    snd_pcm_hw_params_t* hw_params_raw {};
    snd_pcm_hw_params_malloc(&hw_params_raw);
    return __snd_pcm_hw_params_raai(hw_params_raw);
  }

  __snd_ctl_t_raai card_handle() const {
    snd_ctl_t *raw_handle;
    int result = snd_ctl_open(&raw_handle, get_card_str_id().c_str(), 0);
    if (result < 0)
      return nullptr;

    return __snd_ctl_t_raai(raw_handle);
  }

  bool operator==(const __alsa_audio_device_id& rhs) {
    return device_id == rhs.device_id && card_id == rhs.card_id;
  }
};

using __audio_device_id = __alsa_audio_device_id;

class audio_device {
public:
  audio_device() = delete;
  audio_device(const audio_device&) = delete;
  audio_device& operator=(const audio_device&) = delete;
  audio_device(audio_device&& other)
      : _access_type(other._access_type)
      , _sample_rate(other._sample_rate)
      , _audio_format(other._audio_format)
      , _buffer_size_frames(other._buffer_size_frames)
      , _poll_fd(move(other._poll_fd))
      , _supported_sample_rates(move(other._supported_sample_rates))
      , _supported_audio_formats(move(other._supported_audio_formats))
      , _min_supported_buffer_size(other._min_supported_buffer_size)
      , _max_supported_buffer_size(other._max_supported_buffer_size)
      , _device_id(other._device_id)
      , _device_pcm(move(other._device_pcm))
      , _hw_params(move(other._hw_params))
      , _processing_thread(move(other._processing_thread))
      , _name(move(other._name))
      , _config(other._config)
  {}

  audio_device& operator=(audio_device&& other) noexcept {
    _access_type = other._access_type;
    _sample_rate = other._sample_rate;
    _audio_format = other._audio_format;
    _buffer_size_frames = other._buffer_size_frames;
    _poll_fd = move(other._poll_fd);
    _supported_sample_rates = move(other._supported_sample_rates);
    _supported_audio_formats = move(other._supported_audio_formats);
    _min_supported_buffer_size = other._min_supported_buffer_size;
    _max_supported_buffer_size = other._max_supported_buffer_size;
    _device_id = other._device_id;
    _device_pcm = move(other._device_pcm);
    _hw_params = move(other._hw_params);
    _processing_thread = move(other._processing_thread);
    _name = move(other._name);
    _config = other._config;
    return *this;
  }

  ~audio_device() {
    stop();
  }

  string_view name() const noexcept {
    return _name;
  }

  using device_id_t = __alsa_audio_device_id;

  device_id_t device_id() const noexcept {
    return _device_id;
  }

  bool is_input() const noexcept {
    return get_num_input_channels() > 0;
  }

  bool is_output() const noexcept {
    return get_num_output_channels() > 0;
  }

  int get_num_input_channels() const noexcept {
    return _config.input_config;
  }

  int get_num_output_channels() const noexcept {
    return _config.output_config;
  }

  using sample_rate_t = unsigned int;

  sample_rate_t get_sample_rate() const noexcept {
    return _sample_rate;
    __snd_pcm_hw_params_raai hw_params = _device_id.get_hw_params();

    __snd_pcm_helper pcm(this);

    sample_rate_t rate = 44100;
    int direction = SND_PCM_STREAM_PLAYBACK;

    if (!__alsa_util::check_error(snd_pcm_hw_params_any(pcm.get(), hw_params.get())))
      return {};

    if (!__alsa_util::check_error( snd_pcm_hw_params_set_rate_resample	(pcm.get(), hw_params.get(), false)))
      return {};

    if (!__alsa_util::check_error(snd_pcm_hw_params_set_rate_near(pcm.get(), hw_params.get(), &rate, &direction)))
      return {};

    if (!__alsa_util::check_error(snd_pcm_hw_params (pcm.get(), hw_params.get())))
      return {};

    if (!__alsa_util::check_error(snd_pcm_hw_params_get_rate(hw_params.get(), &rate, &direction)))
      return {};

    return rate;
  }
/*
  bool set_sample_rate(sample_rate_t new_sample_rate) {
    AudioObjectPropertyAddress pa = {
      kAudioDevicePropertyNominalSampleRate,
      kAudioObjectPropertyScopeGlobal,
      kAudioObjectPropertyElementMaster
    };

    return __coreaudio_util::check_error(AudioObjectSetPropertyData(
      _device_id, &pa, 0, nullptr, sizeof(sample_rate_t), &new_sample_rate));
  }
*/
  using buffer_size_t = snd_pcm_uframes_t;
  snd_pcm_format_t get_audio_format() const noexcept {

    if (_device_pcm.get()) {
      __snd_pcm_hw_params_raai hw_params = _device_id.get_hw_params();

      if (!__alsa_util::check_error(
              snd_pcm_hw_params_any(_device_pcm.get(), hw_params.get())))
        return {};

      snd_pcm_format_t format;
      __alsa_util::check_error(
          snd_pcm_hw_params_get_format(hw_params.get(), &format));

      return format;
    }

    return _audio_format;
  }


  buffer_size_t get_buffer_size_frames() const noexcept {

    __snd_pcm_hw_params_raai hw_params = _device_id.get_hw_params();
    __snd_pcm_helper pcm(this);

    if (!__alsa_util::check_error(snd_pcm_hw_params_any(pcm.get(), hw_params.get())))
      return {};

    if (!__alsa_util::check_error(snd_pcm_hw_params (pcm.get(), hw_params.get())))
      return {};

    snd_pcm_uframes_t frames {};

    if (!__alsa_util::check_error(snd_pcm_hw_params_get_buffer_size(hw_params.get(), &frames)))
      return {};

    return frames;
  }

  bool set_buffer_size_frames(buffer_size_t new_buffer_size) {

    if (new_buffer_size < _min_supported_buffer_size || new_buffer_size > _max_supported_buffer_size)
      return false;

    if (!__alsa_util::check_error(snd_pcm_hw_params_set_buffer_size_near(_device_pcm.get(), _hw_params.get(), &new_buffer_size)) )
      return false;

    return __alsa_util::check_error(snd_pcm_hw_params_get_buffer_size(_hw_params.get(), &_buffer_size_frames));
  }

  template <typename _SampleType>
  constexpr bool supports_sample_type() const noexcept {
    return is_same_v<_SampleType, __coreaudio_native_sample_type>;
  }

  constexpr bool can_connect() const noexcept {
    return true;
  }

  constexpr bool can_process() const noexcept {
    return false;
  }

  template <typename _CallbackType,
            typename = enable_if_t<is_nothrow_invocable_v<_CallbackType, audio_device&, audio_device_io<__coreaudio_native_sample_type >&>>>
  void connect(_CallbackType callback) {
    if (_running)
      throw audio_device_exception("cannot connect to running audio_device");

    _user_callback = move(callback);
  }

  // TODO: remove std::function as soon as C++20 default-ctable lambda and lambda in unevaluated contexts become available
  using no_op_t = std::function<void(audio_device&)>;

  template <typename _StartCallbackType = no_op_t,
            typename _StopCallbackType = no_op_t,
            // TODO: is_nothrow_invocable_t does not compile, temporarily replaced with is_invocable_t
            typename = enable_if_t<is_invocable_v<_StartCallbackType, audio_device&> && is_invocable_v<_StopCallbackType, audio_device&>>>
  bool start(_StartCallbackType&& start_callback = [](audio_device&) noexcept {},
             _StopCallbackType&& stop_callback = [](audio_device&) noexcept {}) {
    if (!_running) {

      _device_pcm = _device_id.get_pcm();
      _hw_params = _device_id.get_hw_params();

      snd_pcm_uframes_t period_size = 0;

      __snd_pcm_chmap_raai chmap = __make_snd_pcm_chmap(_config.output_config);
      __snd_pcm_sw_params_raai sw_params  = __make_snd_pcm_sw_params();

      __alsa_util::check_error(snd_pcm_hw_params_any(_device_pcm.get(), _hw_params.get()));
      __alsa_util::check_error(snd_pcm_hw_params_set_rate_resample(_device_pcm.get(), _hw_params.get(), false));

      auto access = [this]() -> std::optional<snd_pcm_access_t> {
        for (auto access_type : _permited_access_types) {
          if (0 == snd_pcm_hw_params_set_access(_device_pcm.get(), _hw_params.get(), access_type))
            return access_type;
        }
        return nullopt;
      }();

      if (!access)
        return false;

      _access_type = access.value();

      __alsa_util::check_error(snd_pcm_hw_params_set_channels(_device_pcm.get(), _hw_params.get(), this->_config.output_config));
      __alsa_util::check_error(snd_pcm_hw_params_set_rate(_device_pcm.get(), _hw_params.get(), _sample_rate, SND_PCM_STREAM_PLAYBACK));
      __alsa_util::check_error(snd_pcm_hw_params_set_format(_device_pcm.get(), _hw_params.get(), _audio_format));


      __alsa_util::check_error(snd_pcm_hw_params_set_buffer_size_near(_device_pcm.get(), _hw_params.get(), &_buffer_size_frames));
      __alsa_util::check_error(snd_pcm_hw_params_get_buffer_size(_hw_params.get(), &_buffer_size_frames));

      __alsa_util::check_error(snd_pcm_hw_params(_device_pcm.get(), _hw_params.get()));

      __alsa_util::check_error(snd_pcm_set_chmap(_device_pcm.get(), chmap.get()));

      __alsa_util::check_error(snd_pcm_hw_params_get_period_size(_hw_params.get(), &period_size, nullptr));
      __alsa_util::check_error(snd_pcm_sw_params_current(_device_pcm.get(), sw_params.get()));
      __alsa_util::check_error(snd_pcm_sw_params_set_start_threshold(_device_pcm.get(), sw_params.get(), 0));
      __alsa_util::check_error(snd_pcm_sw_params_set_avail_min(_device_pcm.get(), sw_params.get(), period_size));

      __alsa_util::check_error(snd_pcm_sw_params(_device_pcm.get(), sw_params.get()));

      auto poll_fd = __make_alsa_pollfd(_device_pcm.get());
      if (!poll_fd.has_value())
        return false;

      _poll_fd = std::move(poll_fd.value());

      _running = true;

      _processing_thread = std::thread(&audio_device::run_thread, this);
    }

    return true;
  }

  bool stop() {
    if (_running) {
      _running = false;

      if (_processing_thread.joinable())
        _processing_thread.join();

    }

    return true;
  }

  bool is_running() const noexcept  {
    return _running;
  }

  void wait() const {
    assert(false);
  }

  template <typename _CallbackType>
  void process(_CallbackType&) {
    assert(false);
  }

  constexpr bool has_unprocessed_io() const noexcept {
    return false;
  }

private:
  friend class __audio_device_enumerator;

  struct __snd_pcm_helper {
    __snd_pcm_helper(const audio_device * device)
    {
      if (!device->_device_pcm) {
        snd_pcm_raai = device->device_id().get_pcm();
        pcm = snd_pcm_raai.get();
      } else {
        pcm = device->_device_pcm.get();
      }
    }

    __snd_pcm_t_raai snd_pcm_raai;
    snd_pcm_t * pcm;

    snd_pcm_t * get() {
      return pcm;
    }
  };

  audio_device(device_id_t device_id, string name, __alsa_stream_config config)
  : _device_id(device_id),
    _name(move(name)),
    _config(config)
    {
    assert(!_name.empty());
//    assert(config.input_config.mNumberBuffers == 0 || config.input_config.mNumberBuffers == 1);
//    assert(config.output_config.mNumberBuffers == 0 || config.output_config.mNumberBuffers == 1);

    //_device_pcm = _device_id.get_pcm();
    //_hw_params = _device_id.get_hw_params();

    // TODO : QUERY CHANNEL MAP HERE

    _init_supported_sample_rates();
    _init_supported_buffer_sizes();
    _init_supported_formats();
    _buffer_size_frames = get_buffer_size_frames();
  }

  void run_thread()
  {
    auto recover_xrun = [this](int err)
    {
      if (err == -EPIPE) {
        err = snd_pcm_prepare(_device_pcm.get());
      } else if (err == -ESTRPIPE) {
        while ((err = snd_pcm_resume(_device_pcm.get())) == -EAGAIN) {
          poll(NULL, 0, 1);
        }
        if (err < 0)
          err = snd_pcm_prepare(_device_pcm.get());
      }
      return err;
    };


    while (_running) {
      snd_pcm_state_t state = snd_pcm_state(_device_pcm.get());
      switch (state) {
      case SND_PCM_STATE_SETUP: {
        __alsa_util::check_error(snd_pcm_prepare(_device_pcm.get()));
        continue;
      }
      case SND_PCM_STATE_PREPARED: {
        snd_pcm_sframes_t avail = snd_pcm_avail(_device_pcm.get());
        if (avail < 0) {
          std::cout << "SND_PCM_STATE_PREPARED: avail < 0 :" << avail << std::endl;
          return;
        }

        if ((snd_pcm_uframes_t)avail == _buffer_size_frames) {
          _fill_buffers(avail);
          continue;
        }

        __alsa_util::check_error(snd_pcm_start(_device_pcm.get()));
        continue;
      }
      case SND_PCM_STATE_RUNNING:
      case SND_PCM_STATE_PAUSED: {
        int result = _poll_fd.wait();
        if (result < 0) {
          std::cout << "this->waitForPoll() = " << result << std::endl;
          return;
        }

        snd_pcm_sframes_t avail = snd_pcm_avail_update(_device_pcm.get());
        if (avail < 0) {
          std::cout << "outstream_xrun_recovery :" << avail << std::endl;
          if (recover_xrun(avail) < 0) {
            std::cout << "SND_PCM_STATE_PAUSED|SND_PCM_STATE_RUNNING: avail < 0 :" << avail << std::endl;
            return;
          }
        }

        if (avail > 0) {
          _fill_buffers(avail);
        }
        continue;
      }
      case SND_PCM_STATE_XRUN:
        if (recover_xrun(-EPIPE) < 0) {
          std::cout << "SND_PCM_STATE_XRUN" << std::endl;

          return;
        }

        continue;
      case SND_PCM_STATE_OPEN:
        std::cout << "SND_PCM_STATE_OPEN" << std::endl;
        return;
      case SND_PCM_STATE_DRAINING:
        std::cout << "SND_PCM_STATE_DRAINING" << std::endl;
        return;
      case SND_PCM_STATE_DISCONNECTED:
        std::cout << "SND_PCM_STATE_DISCONNECTED" << std::endl;
        return;
      default:
        continue;
      }
    }
  }

/*
  static OSStatus _device_callback(AudioObjectID device_id,
                                   const AudioTimeStamp* &* now *&*/ /*,
                                   const AudioBufferList* input_data,
                                   const AudioTimeStamp* input_time,
                                   AudioBufferList* output_data,
                                   const AudioTimeStamp* output_time,
                                   void *void_ptr_to_this_device) {
    assert (void_ptr_to_this_device != nullptr);
    audio_device& this_device = *reinterpret_cast<audio_device*>(void_ptr_to_this_device);

    _fill_buffers(input_data, input_time, output_data, output_time, this_device._current_buffers);

    invoke(this_device._user_callback, this_device, this_device._current_buffers);
    return noErr;
  }
*/
  void _fill_buffers(snd_pcm_uframes_t available_frames) {
    const snd_pcm_channel_area_t *areas;
    snd_pcm_uframes_t frames = available_frames;
    snd_pcm_uframes_t offset = 0;
    __alsa_util::check_error(
        snd_pcm_mmap_begin(_device_pcm.get(), &areas, &offset, &frames));
    audio_device_io<int16_t> device_io;
    if (_access_type == SND_PCM_ACCESS_MMAP_INTERLEAVED) {
      int16_t* ptr2 = (int16_t *)areas->addr;
      ptr2 += ( offset * _config.output_config );
      device_io.output_buffer = audio_buffer<int16_t>(ptr2 , frames,
                                   _config.output_config,
                                   contiguous_interleaved);

    } else if (_access_type == SND_PCM_ACCESS_MMAP_NONINTERLEAVED) {
      int16_t* ptr2 = (int16_t *)areas->addr;
      ptr2 += ( offset );
      device_io.output_buffer = audio_buffer<int16_t>((int16_t *)areas->addr, frames,
                                   _config.output_config,
                                   contiguous_deinterleaved);


    }
    _user_callback(*this, device_io);
    snd_pcm_sframes_t committed = snd_pcm_mmap_commit(_device_pcm.get(), offset, frames);
    assert ( committed == frames );
  }

                                   /*
  static void _fill_buffers(const AudioBufferList* input_bl,
                            const AudioTimeStamp* input_time,
                            const AudioBufferList* output_bl,
                            const AudioTimeStamp* output_time,
                            audio_device_io<__coreaudio_native_sample_type>& buffers) {
    assert(input_bl != nullptr);
    assert(output_bl != nullptr);

    const size_t num_input_buffers = input_bl->mNumberBuffers;
    assert(num_input_buffers == 0 || num_input_buffers == 1);

    const size_t num_output_buffers = output_bl->mNumberBuffers;
    assert(num_output_buffers == 0 || num_output_buffers == 1);

    if (num_input_buffers == 1) {
      buffers.input_buffer = coreaudio_buffer_to_buffer(input_bl->mBuffers[0]);
      buffers.input_time = coreaudio_timestamp_to_timepoint (input_time);
    }

    if (num_output_buffers == 1) {
      buffers.output_buffer = coreaudio_buffer_to_buffer(output_bl->mBuffers[0]);
      buffers.output_time = coreaudio_timestamp_to_timepoint (output_time);
    }
  }
*/
/*  static audio_buffer<__coreaudio_native_sample_type> coreaudio_buffer_to_buffer(const AudioBuffer& ca_buffer) {
    // TODO: allow different sample types here! It will possibly be int16_t instead of float on iOS!

    auto* data_ptr = reinterpret_cast<__coreaudio_native_sample_type*>(ca_buffer.mData);
    const size_t num_channels = ca_buffer.mNumberChannels;
    const size_t num_frames = ca_buffer.mDataByteSize / sizeof(__coreaudio_native_sample_type) / num_channels;

    return {data_ptr, num_frames, num_channels, contiguous_interleaved};
  }

  static audio_clock_t::time_point coreaudio_timestamp_to_timepoint(const AudioTimeStamp* timestamp) {
    auto count = static_cast<audio_clock_t::rep>(timestamp->mHostTime);
    return audio_clock_t::time_point() + audio_clock_t::duration(count);
  }*/

  inline static constexpr int _alsa_invalid_parameter = -22;

  inline static constexpr auto _permited_access_types = __array_of<snd_pcm_access_t>(
      SND_PCM_ACCESS_MMAP_INTERLEAVED,
      SND_PCM_ACCESS_MMAP_NONINTERLEAVED
  );
  inline static constexpr auto _test_sample_rates = __array_of<size_t>(
      44100u,
      48000u,
      96000u,
      32000u,
      22050u,
      8000u,
      4000u,
      192000u);

  inline static constexpr auto _test_audio_formats = __array_of<snd_pcm_format_t>(
      SND_PCM_FORMAT_FLOAT_LE,
      SND_PCM_FORMAT_S16_LE,
      SND_PCM_FORMAT_S24_LE,
      SND_PCM_FORMAT_S32_LE,
      SND_PCM_FORMAT_FLOAT64_LE,
      SND_PCM_FORMAT_S8
     );

  void _init_supported_formats() {
    snd_pcm_format_mask_t* format_mask_raw = nullptr;
    snd_pcm_format_mask_malloc(&format_mask_raw);

    __snd_pcm_format_mask_raai format_mask(format_mask_raw);
    __snd_pcm_hw_params_raai hw_params = _device_id.get_hw_params();

    __snd_pcm_helper pcm(this);

    __alsa_util::check_error(snd_pcm_hw_params_any(pcm.get(), hw_params.get()));
    snd_pcm_hw_params_get_format_mask(hw_params.get(), format_mask.get());

    for (auto test_format : _test_audio_formats) {
      if (snd_pcm_format_mask_test(format_mask.get(), test_format)) {
        _supported_audio_formats.push_back(test_format);
      }
    }

    if (_supported_audio_formats.size() > 0)
      _audio_format = _supported_audio_formats[0];
  }
  void _init_supported_sample_rates() {
    __snd_pcm_hw_params_raai hw_params = _device_id.get_hw_params();
    __snd_pcm_helper pcm(this);

    __alsa_util::check_error(snd_pcm_hw_params_any(pcm.get(), hw_params.get()));

    for (auto rate : _test_sample_rates) {
      int result = snd_pcm_hw_params_test_rate(pcm.get(), hw_params.get(), rate, SND_PCM_STREAM_PLAYBACK);
      if (result == 0)
        _supported_sample_rates.push_back(rate);
    }

    if (_supported_sample_rates.size() > 0)
      _sample_rate = _supported_sample_rates[0];
  }

  void _init_supported_buffer_sizes() {

    __snd_pcm_hw_params_raai hw_params = _device_id.get_hw_params();
    __snd_pcm_helper pcm(this);

    __alsa_util::check_error(snd_pcm_hw_params_any(pcm.get(), hw_params.get()));

    snd_pcm_uframes_t min_frames {};
    snd_pcm_uframes_t max_frames {};

    __alsa_util::check_error(snd_pcm_hw_params_get_buffer_size_min(hw_params.get(), &min_frames));
    __alsa_util::check_error(snd_pcm_hw_params_get_buffer_size_max(hw_params.get(), &max_frames));

    _min_supported_buffer_size = static_cast<buffer_size_t>(min_frames);
    _max_supported_buffer_size = static_cast<buffer_size_t>(max_frames);

    // ensure that the supported buffer size range obtained from Wasapi makes sense:
    // TODO: do this using proper error handling/reporting instead of asserts
    assert(_min_supported_buffer_size > 0);
    assert(_max_supported_buffer_size >= _min_supported_buffer_size);
  }

  snd_pcm_access_t _access_type {};
  sample_rate_t _sample_rate {};
  snd_pcm_format_t _audio_format {};
  buffer_size_t _buffer_size_frames {};
  __alsa_pollfd _poll_fd {};

  vector<sample_rate_t> _supported_sample_rates = {};
  vector<snd_pcm_format_t> _supported_audio_formats = {};
  buffer_size_t _min_supported_buffer_size = 0;
  buffer_size_t _max_supported_buffer_size = 0;

  __alsa_audio_device_id _device_id = {};

  __snd_pcm_t_raai _device_pcm;
  __snd_pcm_hw_params_raai _hw_params;

  thread _processing_thread;
  atomic<bool> _running = false;

  string _name = {};
  __alsa_stream_config _config;

  using __coreaudio_callback_t = function<void(audio_device&, audio_device_io<__coreaudio_native_sample_type>&)>;
  __coreaudio_callback_t _user_callback;
  audio_device_io<__coreaudio_native_sample_type> _current_buffers;
};

class audio_device_list : public forward_list<audio_device> {
};

class __audio_device_enumerator {
public:
  static __audio_device_enumerator& get_instance() {
    static __audio_device_enumerator cde;
    return cde;
  }

  optional<audio_device> get_default_io_device(snd_pcm_stream_t direction) {

    //TODO ignoring direction
    auto device_ids = get_device_ids();

    void ** name_hints_raw = {nullptr};
    snd_device_name_hint(-1, "pcm", &name_hints_raw);
    __snd_device_name_hint_raai nameHints(name_hints_raw);

    for (void** name_hint_itr = name_hints_raw; *name_hint_itr; ++name_hint_itr) {
      std::unique_ptr<char> name_raai(snd_device_name_get_hint(*name_hint_itr, "NAME"));
      std::unique_ptr<char> desc_raai(snd_device_name_get_hint(*name_hint_itr, "DESC"));

      std::string name(name_raai.get());
      std::string desc(desc_raai.get());
      desc = desc.substr(0, desc.find('\n'));
      if (name.rfind("sysdefault:CARD=", 0) == 0) {
        auto device_id_iterator = find_if(begin(device_ids), end(device_ids), [desc](const auto& device_id){
          return device_id.get_device_name() == desc;
        });
        if (device_id_iterator != end(device_ids)) {
          return get_device(*device_id_iterator);
        }
      }
    }
    return nullopt;
  }

  template <typename Condition>
  auto get_device_list(Condition condition) {
    audio_device_list devices;
    const auto device_ids = get_device_ids();

    for (const auto device_id : device_ids) {
      auto device_from_id = get_device(device_id);
      if (condition(device_from_id))
        devices.push_front(move(device_from_id));
    }

    return devices;
  }

  auto get_input_device_list() {
    return get_device_list([](const audio_device& d){
      return d.is_input();
    });
  }

  auto get_output_device_list() {
    return get_device_list([](const audio_device& d){
      return d.is_output();
    });
  }

private:
  __audio_device_enumerator() = default;

  static vector<__alsa_audio_device_id> get_device_ids() {

    vector<__alsa_audio_device_id> device_ids;

    __alsa_audio_device_id device_id;
    while (true) {

      int result = snd_card_next(&device_id.card_id);
      if (result < 0 || device_id.card_id < 0)
        break;

      __snd_ctl_t_raai snd_ctl_handle = device_id.card_handle();
      if (!snd_ctl_handle)
        continue;

      while (true) {
        result = snd_ctl_pcm_next_device(snd_ctl_handle.get(), &device_id.device_id);
        if (result < 0 || device_id.device_id < 0)
          break;

        device_ids.push_back(device_id);
      }
    }

    return device_ids;
  }

  static audio_device get_device(__alsa_audio_device_id device_id) {
    string name = get_device_name(device_id);
    auto config = get_device_io_stream_config(device_id);

    return {device_id, move(name), config};
  }

  static string get_device_name(__alsa_audio_device_id device_id) {
    return device_id.get_device_name();
  }

  static __alsa_stream_config get_device_io_stream_config(__alsa_audio_device_id device_id) {
    return {
      get_device_stream_config(device_id, SND_PCM_STREAM_CAPTURE),
      get_device_stream_config(device_id, SND_PCM_STREAM_PLAYBACK)
    };
  }

  static int get_device_stream_config(__alsa_audio_device_id device_id, snd_pcm_stream_t streamDirection) {

    __snd_pcm_chmap_query_raai chmap_query(snd_pcm_query_chmaps_from_hw(device_id.card_id, device_id.device_id, 0, streamDirection));

    if (!chmap_query) {
      return 0;
    }

    // TODO do better than just matching 2 or 1 channels
    auto find_chmap = [&chmap_query](int count) -> bool  {
      auto ** chmap_iterator = chmap_query.get();
      while (*chmap_iterator != nullptr) {
        if ((*chmap_iterator)->map.channels == count)
          return true;
        ++chmap_iterator;
      }
      return false;
    };

    if (find_chmap(2)) {
      return 2;
    }

    if (find_chmap(1)) {
      return 1;
    }

    return 0;
  }
};

optional<audio_device> get_default_audio_input_device() {
  return __audio_device_enumerator::get_instance().get_default_io_device(
      SND_PCM_STREAM_CAPTURE);
}

optional<audio_device> get_default_audio_output_device() {
  return __audio_device_enumerator::get_instance().get_default_io_device(
      SND_PCM_STREAM_PLAYBACK);
}

audio_device_list get_audio_input_device_list() {
  return __audio_device_enumerator::get_instance().get_input_device_list();
}

audio_device_list get_audio_output_device_list() {
  return __audio_device_enumerator::get_instance().get_output_device_list();
}
 /*
struct __coreaudio_device_config_listener {
  static void register_callback(audio_device_list_event event, function<void()> cb) {
    static __coreaudio_device_config_listener dcl;
    const auto selector = get_coreaudio_selector(event);
    dcl.callbacks[selector] = move(cb);
  }

private:
  map<AudioObjectPropertySelector, function<void()>> callbacks;

  __coreaudio_device_config_listener() {
    coreaudio_add_internal_callback<kAudioHardwarePropertyDevices>();
    coreaudio_add_internal_callback<kAudioHardwarePropertyDefaultInputDevice>();
    coreaudio_add_internal_callback<kAudioHardwarePropertyDefaultOutputDevice>();
  }

  template <AudioObjectPropertySelector selector>
  void coreaudio_add_internal_callback() {
    AudioObjectPropertyAddress pa = {
        selector,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };

    if (!__coreaudio_util::check_error(AudioObjectAddPropertyListener(
        kAudioObjectSystemObject, &pa, &coreaudio_internal_callback<selector>, this))) {
      assert(false); // failed to register device config listener!
    }
  }

  template <AudioObjectPropertySelector selector>
  static OSStatus coreaudio_internal_callback(AudioObjectID device_id,
                                              UInt32 &* inNumberAddresses *&,
                                              const AudioObjectPropertyAddress* &* inAddresses *&,
                                              void* void_ptr_to_this_listener) {
    __coreaudio_device_config_listener& this_listener = *reinterpret_cast<__coreaudio_device_config_listener*>(void_ptr_to_this_listener);
    this_listener.call<selector>();
    return {};
  }

  template <AudioObjectPropertySelector selector>
  void call() {
    if (auto cb_iter = callbacks.find(selector); cb_iter != callbacks.end()) {
      invoke(cb_iter->second);
    }
  }

  static constexpr AudioObjectPropertySelector get_coreaudio_selector(audio_device_list_event event) noexcept {
    switch (event) {
      case audio_device_list_event::device_list_changed:
        return kAudioHardwarePropertyDevices;
      case audio_device_list_event::default_input_device_changed:
        return kAudioHardwarePropertyDefaultInputDevice;
      case audio_device_list_event::default_output_device_changed:
        return kAudioHardwarePropertyDefaultOutputDevice;
      default:
        assert(false); // invalid event!
        return {};
    }
  }*/
//};

/*
template <typename F, typename &* = enable_if_t<is_nothrow_invocable_v<F>> *&>
void set_audio_device_list_callback(audio_device_list_event event, F&& cb) {
  __coreaudio_device_config_listener::register_callback(event, function<void()>(cb));
}
*/
_LIBSTDAUDIO_NAMESPACE_END
