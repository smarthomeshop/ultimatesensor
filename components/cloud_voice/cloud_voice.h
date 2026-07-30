#pragma once

#include "esphome/components/media_player/media_player.h"
#include "esphome/components/micro_wake_word/micro_wake_word.h"
#include "esphome/components/microphone/microphone_source.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace esphome::cloud_voice {

enum class State : uint8_t {
  MUTED,
  IDLE,
  LISTENING,
  PROCESSING,
  REPLYING,
  ERROR,
};

class CloudVoice final : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void set_microphone_source(microphone::MicrophoneSource *source) { this->microphone_source_ = source; }
  void set_media_player(media_player::MediaPlayer *player) { this->media_player_ = player; }
  void set_micro_wake_word(micro_wake_word::MicroWakeWord *wake_word) { this->micro_wake_word_ = wake_word; }
  void set_wake_word_switch(switch_::Switch *wake_word_switch) { this->wake_word_switch_ = wake_word_switch; }
  void set_api_url(const std::string &api_url);
  void set_max_recording_seconds(uint8_t seconds) { this->max_recording_seconds_ = seconds; }
  void set_min_recording_seconds(float seconds) { this->min_recording_ms_ = static_cast<uint32_t>(seconds * 1000); }
  void set_silence_seconds(float seconds) { this->silence_ms_ = static_cast<uint32_t>(seconds * 1000); }
  void set_speech_rms(uint16_t rms) { this->speech_rms_ = rms; }
  void set_silence_rms(uint16_t rms) { this->silence_rms_ = rms; }

  void set_credentials(const std::string &device_id, const std::string &token);
  void clear_credentials();
  void set_cloud_enabled(bool enabled);
  void set_user_enabled(bool enabled);
  void set_follow_up_settings(bool enabled, uint8_t timeout_seconds, uint8_t max_turns);
  void set_speaker_volume(uint8_t volume);
  void start();

  const char *state_name() const;

  Trigger<> *get_listening_trigger() { return &this->listening_trigger_; }
  Trigger<> *get_processing_trigger() { return &this->processing_trigger_; }
  Trigger<> *get_replying_trigger() { return &this->replying_trigger_; }
  Trigger<> *get_idle_trigger() { return &this->idle_trigger_; }
  Trigger<> *get_error_trigger() { return &this->error_trigger_; }

 protected:
  static constexpr uint32_t SAMPLE_RATE = 16000;
  static constexpr size_t BYTES_PER_SAMPLE = 2;
  static constexpr size_t WAV_HEADER_SIZE = 44;
  static constexpr size_t MAX_RESPONSE_BYTES = 32768;
  static constexpr uint32_t PLAYBACK_TIMEOUT_MS = 60000;
  static constexpr uint32_t ERROR_MESSAGE_DURATION_MS = 6500;
  static constexpr uint32_t RESUME_DELAY_MS = 750;
  static constexpr uint8_t HARD_MAX_FOLLOW_UP_TURNS = 2;

  void handle_audio_(const std::vector<uint8_t> &data);
  void start_recording_(bool follow_up);
  void finish_capture_();
  void launch_upload_();
  static void upload_task_(void *parameter);
  void perform_upload_();
  bool write_all_(void *client, const uint8_t *data, size_t length);
  void handle_upload_result_();
  void start_playback_(const std::string &url);
  void handle_playback_finished_();
  void fail_(const char *code);
  void finish_to_idle_();
  void resume_wake_word_();
  bool may_listen_() const;
  void set_state_(State state);
  static void write_wav_header_(uint8_t header[WAV_HEADER_SIZE], uint32_t pcm_size);
  static uint16_t calculate_rms_(const uint8_t *data, size_t length);

  microphone::MicrophoneSource *microphone_source_{nullptr};
  media_player::MediaPlayer *media_player_{nullptr};
  micro_wake_word::MicroWakeWord *micro_wake_word_{nullptr};
  switch_::Switch *wake_word_switch_{nullptr};

  Trigger<> listening_trigger_;
  Trigger<> processing_trigger_;
  Trigger<> replying_trigger_;
  Trigger<> idle_trigger_;
  Trigger<> error_trigger_;

  std::string api_url_;
  std::string device_id_;
  std::string token_;
  std::string conversation_id_;
  std::string request_device_id_;
  std::string request_token_;
  std::string request_conversation_id_;
  std::string response_body_;
  std::string upload_error_;
  std::string response_audio_url_;

  uint8_t *audio_buffer_{nullptr};
  size_t audio_capacity_{0};
  size_t audio_size_{0};
  uint32_t capture_started_ms_{0};
  uint32_t last_speech_ms_{0};
  uint32_t resume_at_ms_{0};
  uint32_t playback_started_ms_{0};
  uint32_t conversation_last_activity_ms_{0};
  uint32_t min_recording_ms_{2500};
  uint32_t silence_ms_{900};
  uint16_t speech_rms_{50};
  uint16_t silence_rms_{30};
  uint8_t max_recording_seconds_{8};
  uint8_t follow_up_timeout_seconds_{8};
  uint8_t max_follow_up_turns_{2};
  uint8_t follow_up_turn_{0};
  uint8_t speaker_volume_{40};
  int upload_status_{0};

  std::atomic<bool> capturing_{false};
  std::atomic<bool> capture_complete_{false};
  std::atomic<bool> upload_done_{false};
  std::atomic<bool> playback_finished_{false};

  TaskHandle_t upload_task_handle_{nullptr};
  State state_{State::MUTED};
  bool cloud_enabled_{false};
  bool user_enabled_{true};
  bool speech_started_{false};
  bool silent_follow_up_{false};
  bool current_is_follow_up_{false};
  bool follow_up_enabled_{true};
  bool response_continue_conversation_{false};
  bool playback_started_{false};
  bool external_announcement_{false};
};

}  // namespace esphome::cloud_voice
