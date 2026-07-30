#include "cloud_voice.h"

#include "esphome/components/json/json_util.h"
#include "esphome/components/network/util.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <esp_crt_bundle.h>
#include <esp_err.h>
#include <esp_http_client.h>

namespace esphome::cloud_voice {

static const char *const TAG = "cloud_voice";
static const char *const MULTIPART_BOUNDARY = "----UltimateSensorCloudVoice";

void CloudVoice::set_api_url(const std::string &api_url) {
  this->api_url_ = api_url;
  while (!this->api_url_.empty() && this->api_url_.back() == '/') {
    this->api_url_.pop_back();
  }
}

void CloudVoice::setup() {
  this->audio_capacity_ = static_cast<size_t>(this->max_recording_seconds_) * SAMPLE_RATE * BYTES_PER_SAMPLE;
  RAMAllocator<uint8_t> allocator(RAMAllocator<uint8_t>::ALLOC_EXTERNAL);
  this->audio_buffer_ = allocator.allocate(this->audio_capacity_);
  if (this->audio_buffer_ == nullptr) {
    ESP_LOGE(TAG, "Could not allocate %u bytes of PSRAM for voice recording",
             static_cast<unsigned>(this->audio_capacity_));
    this->mark_failed();
    return;
  }

  this->microphone_source_->add_data_callback(
      [this](const std::vector<uint8_t> &data) { this->handle_audio_(data); });

  this->media_player_->add_on_state_callback([this](media_player::MediaPlayerState state) {
    const bool active = state == media_player::MEDIA_PLAYER_STATE_ANNOUNCING ||
                        state == media_player::MEDIA_PLAYER_STATE_PLAYING;

    if (this->state_ == State::REPLYING) {
      if (active) {
        this->playback_started_ = true;
      } else if (this->playback_started_) {
        this->playback_finished_.store(true);
      }
      return;
    }

    if (active && this->state_ == State::IDLE) {
      this->external_announcement_ = true;
      this->micro_wake_word_->stop();
    } else if (!active && this->external_announcement_) {
      this->external_announcement_ = false;
      this->resume_at_ms_ = millis() + RESUME_DELAY_MS;
    }
  });

  this->set_state_(State::MUTED);
}

float CloudVoice::get_setup_priority() const { return setup_priority::AFTER_CONNECTION; }

void CloudVoice::dump_config() {
  ESP_LOGCONFIG(TAG,
                "SmartHomeShop Cloud Voice:\n"
                "  API: %s\n"
                "  Max recording: %us\n"
                "  PSRAM recording buffer: %u bytes\n"
                "  RMS speech/silence: %u/%u",
                this->api_url_.c_str(), this->max_recording_seconds_, static_cast<unsigned>(this->audio_capacity_),
                this->speech_rms_, this->silence_rms_);
}

void CloudVoice::loop() {
  if (this->capture_complete_.exchange(false)) {
    this->finish_capture_();
  }

  if (this->upload_done_.exchange(false)) {
    this->upload_task_handle_ = nullptr;
    this->handle_upload_result_();
  }

  if (this->playback_finished_.exchange(false)) {
    this->handle_playback_finished_();
  }

  const uint32_t now = millis();
  if (this->state_ == State::REPLYING && now - this->playback_started_ms_ >= PLAYBACK_TIMEOUT_MS) {
    this->fail_("playback_timeout");
  }

  if (this->resume_at_ms_ != 0 && static_cast<int32_t>(now - this->resume_at_ms_) >= 0) {
    this->resume_at_ms_ = 0;
    if (this->response_continue_conversation_ && this->follow_up_enabled_ &&
        this->follow_up_turn_ < std::min(this->max_follow_up_turns_, HARD_MAX_FOLLOW_UP_TURNS)) {
      this->follow_up_turn_++;
      this->set_state_(State::IDLE);
      this->start_recording_(true);
    } else {
      this->finish_to_idle_();
    }
  }
}

void CloudVoice::set_credentials(const std::string &device_id, const std::string &token) {
  this->device_id_ = device_id;
  this->token_ = token;
}

void CloudVoice::clear_credentials() {
  this->device_id_.clear();
  this->token_.clear();
  this->set_cloud_enabled(false);
}

void CloudVoice::set_cloud_enabled(bool enabled) {
  if (this->cloud_enabled_ == enabled) {
    return;
  }
  this->cloud_enabled_ = enabled;
  if (!enabled) {
    this->micro_wake_word_->stop();
    if (this->capturing_.exchange(false)) {
      this->microphone_source_->stop();
    }
    this->set_state_(State::MUTED);
    this->idle_trigger_.trigger();
    return;
  }
  this->finish_to_idle_();
}

void CloudVoice::set_user_enabled(bool enabled) {
  if (this->user_enabled_ == enabled) {
    return;
  }
  this->user_enabled_ = enabled;
  if (!enabled) {
    this->micro_wake_word_->stop();
    if (this->capturing_.exchange(false)) {
      this->microphone_source_->stop();
    }
    this->set_state_(State::MUTED);
    this->idle_trigger_.trigger();
    return;
  }
  this->finish_to_idle_();
}

void CloudVoice::set_follow_up_settings(bool enabled, uint8_t timeout_seconds, uint8_t max_turns) {
  this->follow_up_enabled_ = enabled;
  this->follow_up_timeout_seconds_ = clamp<uint8_t>(timeout_seconds, 3, this->max_recording_seconds_);
  this->max_follow_up_turns_ = clamp<uint8_t>(max_turns, 0, HARD_MAX_FOLLOW_UP_TURNS);
}

void CloudVoice::set_speaker_volume(uint8_t volume) {
  this->speaker_volume_ = std::min<uint8_t>(volume, 100);
  auto call = this->media_player_->make_call();
  call.set_volume(this->speaker_volume_ / 100.0f);
  call.perform();
}

void CloudVoice::start() {
  if (!this->may_listen_()) {
    this->fail_("cloud_voice_not_ready");
    return;
  }

  const uint32_t now = millis();
  if (this->conversation_id_.empty() || now - this->conversation_last_activity_ms_ > 600000UL) {
    this->conversation_id_.clear();
    this->follow_up_turn_ = 0;
  }
  this->start_recording_(false);
}

bool CloudVoice::may_listen_() const {
  return !this->is_failed() && this->cloud_enabled_ && this->user_enabled_ && this->wake_word_switch_->state &&
         !this->device_id_.empty() && !this->token_.empty() && network::is_connected() &&
         this->state_ != State::LISTENING && this->state_ != State::PROCESSING && this->state_ != State::REPLYING;
}

void CloudVoice::start_recording_(bool follow_up) {
  if (!this->may_listen_()) {
    this->finish_to_idle_();
    return;
  }

  this->micro_wake_word_->stop();
  this->audio_size_ = 0;
  this->speech_started_ = false;
  this->silent_follow_up_ = false;
  this->current_is_follow_up_ = follow_up;
  this->capture_started_ms_ = millis();
  this->last_speech_ms_ = this->capture_started_ms_;
  this->capture_complete_.store(false);
  this->capturing_.store(true);
  this->set_state_(State::LISTENING);
  this->listening_trigger_.trigger();
  this->microphone_source_->start();
}

void CloudVoice::handle_audio_(const std::vector<uint8_t> &data) {
  if (!this->capturing_.load() || data.empty()) {
    return;
  }

  const size_t remaining = this->audio_capacity_ - this->audio_size_;
  const size_t copy_size = std::min(remaining, data.size() - (data.size() % BYTES_PER_SAMPLE));
  if (copy_size > 0) {
    memcpy(this->audio_buffer_ + this->audio_size_, data.data(), copy_size);
    this->audio_size_ += copy_size;
  }

  const uint32_t now = millis();
  const uint16_t rms = calculate_rms_(data.data(), data.size());
  if (rms >= this->speech_rms_) {
    this->speech_started_ = true;
    this->last_speech_ms_ = now;
  } else if (rms > this->silence_rms_ && this->speech_started_) {
    this->last_speech_ms_ = now;
  }

  const uint32_t elapsed = now - this->capture_started_ms_;
  const uint32_t max_duration_ms =
      static_cast<uint32_t>(this->current_is_follow_up_ ? this->follow_up_timeout_seconds_
                                                       : this->max_recording_seconds_) *
      1000UL;
  const bool timed_out = elapsed >= max_duration_ms || this->audio_size_ >= this->audio_capacity_;
  const bool end_of_speech =
      this->speech_started_ && elapsed >= this->min_recording_ms_ && now - this->last_speech_ms_ >= this->silence_ms_;

  if (timed_out || end_of_speech) {
    this->silent_follow_up_ = this->current_is_follow_up_ && !this->speech_started_;
    this->capturing_.store(false);
    this->capture_complete_.store(true);
  }
}

void CloudVoice::finish_capture_() {
  this->microphone_source_->stop();
  if (!this->cloud_enabled_ || !this->user_enabled_) {
    this->finish_to_idle_();
    return;
  }
  if (this->silent_follow_up_) {
    this->response_continue_conversation_ = false;
    this->finish_to_idle_();
    return;
  }
  if (!this->speech_started_ || this->audio_size_ < SAMPLE_RATE * BYTES_PER_SAMPLE / 2) {
    this->fail_("no_speech");
    return;
  }

  this->set_state_(State::PROCESSING);
  this->processing_trigger_.trigger();
  this->launch_upload_();
}

void CloudVoice::launch_upload_() {
  if (this->upload_task_handle_ != nullptr) {
    this->fail_("request_busy");
    return;
  }

  this->upload_done_.store(false);
  this->upload_status_ = 0;
  this->response_body_.clear();
  this->upload_error_.clear();
  this->request_device_id_ = this->device_id_;
  this->request_token_ = this->token_;
  this->request_conversation_id_ = this->conversation_id_;

  const BaseType_t result =
      xTaskCreatePinnedToCore(upload_task_, "cloud_voice_http", 16384, this, 2, &this->upload_task_handle_,
                              tskNO_AFFINITY);
  if (result != pdPASS) {
    this->upload_task_handle_ = nullptr;
    this->fail_("request_task_failed");
  }
}

void CloudVoice::upload_task_(void *parameter) {
  auto *self = static_cast<CloudVoice *>(parameter);
  self->perform_upload_();
  self->upload_done_.store(true);
  vTaskDelete(nullptr);
}

bool CloudVoice::write_all_(void *client_ptr, const uint8_t *data, size_t length) {
  auto client = static_cast<esp_http_client_handle_t>(client_ptr);
  size_t offset = 0;
  while (offset < length) {
    const int written =
        esp_http_client_write(client, reinterpret_cast<const char *>(data + offset), length - offset);
    if (written <= 0) {
      return false;
    }
    offset += written;
  }
  return true;
}

void CloudVoice::perform_upload_() {
  const std::string endpoint = this->api_url_ + "/api/v1/devices/voice/requests";
  const std::string device_part = std::string("--") + MULTIPART_BOUNDARY +
                                  "\r\nContent-Disposition: form-data; name=\"device_id\"\r\n\r\n" +
                                  this->request_device_id_ + "\r\n";
  const std::string conversation_part =
      this->request_conversation_id_.empty()
          ? ""
          : std::string("--") + MULTIPART_BOUNDARY +
                "\r\nContent-Disposition: form-data; name=\"conversation_id\"\r\n\r\n" +
                this->request_conversation_id_ + "\r\n";
  const std::string audio_part =
      std::string("--") + MULTIPART_BOUNDARY +
      "\r\nContent-Disposition: form-data; name=\"audio\"; filename=\"request.wav\"\r\n"
      "Content-Type: audio/wav\r\n\r\n";
  const std::string footer = std::string("\r\n--") + MULTIPART_BOUNDARY + "--\r\n";
  const size_t content_length = device_part.size() + conversation_part.size() + audio_part.size() + WAV_HEADER_SIZE +
                                this->audio_size_ + footer.size();

  esp_http_client_config_t config = {};
  config.url = endpoint.c_str();
  config.method = HTTP_METHOD_POST;
  config.timeout_ms = 90000;
  config.buffer_size = 4096;
  config.buffer_size_tx = 4096;
#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
  config.crt_bundle_attach = esp_crt_bundle_attach;
#endif

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (client == nullptr) {
    this->upload_error_ = "http_init_failed";
    return;
  }

  const std::string authorization = "Bearer " + this->request_token_;
  const std::string content_type = std::string("multipart/form-data; boundary=") + MULTIPART_BOUNDARY;
  esp_http_client_set_header(client, "Authorization", authorization.c_str());
  esp_http_client_set_header(client, "Accept", "application/json");
  esp_http_client_set_header(client, "Content-Type", content_type.c_str());
  esp_http_client_set_header(client, "User-Agent", "UltimateSensor-CloudVoice");

  esp_err_t error = esp_http_client_open(client, content_length);
  uint8_t wav_header[WAV_HEADER_SIZE];
  write_wav_header_(wav_header, this->audio_size_);

  const bool written =
      error == ESP_OK &&
      this->write_all_(client, reinterpret_cast<const uint8_t *>(device_part.data()), device_part.size()) &&
      (conversation_part.empty() ||
       this->write_all_(client, reinterpret_cast<const uint8_t *>(conversation_part.data()),
                        conversation_part.size())) &&
      this->write_all_(client, reinterpret_cast<const uint8_t *>(audio_part.data()), audio_part.size()) &&
      this->write_all_(client, wav_header, sizeof(wav_header)) &&
      this->write_all_(client, this->audio_buffer_, this->audio_size_) &&
      this->write_all_(client, reinterpret_cast<const uint8_t *>(footer.data()), footer.size());

  if (!written) {
    this->upload_error_ = error == ESP_OK ? "http_write_failed" : esp_err_to_name(error);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return;
  }

  const int64_t response_length = esp_http_client_fetch_headers(client);
  (void) response_length;
  this->upload_status_ = esp_http_client_get_status_code(client);
  this->response_body_.reserve(2048);

  char buffer[1024];
  while (this->response_body_.size() < MAX_RESPONSE_BYTES) {
    const int read = esp_http_client_read(client, buffer, sizeof(buffer));
    if (read > 0) {
      const size_t accepted = std::min<size_t>(read, MAX_RESPONSE_BYTES - this->response_body_.size());
      this->response_body_.append(buffer, accepted);
      continue;
    }
    if (read == 0 && esp_http_client_is_complete_data_received(client)) {
      break;
    }
    if (read <= 0) {
      break;
    }
  }

  if (this->upload_status_ <= 0) {
    this->upload_error_ = "http_response_failed";
  }

  esp_http_client_close(client);
  esp_http_client_cleanup(client);
}

void CloudVoice::handle_upload_result_() {
  if (!this->cloud_enabled_ || !this->user_enabled_ || !this->wake_word_switch_->state) {
    this->response_continue_conversation_ = false;
    this->finish_to_idle_();
    return;
  }

  if (!this->upload_error_.empty()) {
    ESP_LOGW(TAG, "Cloud Voice upload failed: %s", this->upload_error_.c_str());
    this->fail_("network_error");
    return;
  }

  if (this->upload_status_ != 200) {
    ESP_LOGW(TAG, "Cloud Voice returned HTTP %d", this->upload_status_);
    if (this->upload_status_ == 401) {
      this->clear_credentials();
      this->fail_("invalid_token");
    } else if (this->upload_status_ == 429) {
      this->fail_("voice_quota_exceeded");
    } else if (this->upload_status_ == 403) {
      this->fail_("cloud_voice_disabled");
    } else {
      this->fail_("cloud_voice_error");
    }
    return;
  }

  bool parsed = false;
  this->response_continue_conversation_ = false;
  json::parse_json(this->response_body_, [this, &parsed](JsonObject root) -> bool {
    if (!root["audio_url"].is<const char *>()) {
      return false;
    }
    this->response_audio_url_ = root["audio_url"].as<std::string>();
    if (root["conversation_id"].is<const char *>()) {
      this->conversation_id_ = root["conversation_id"].as<std::string>();
    }
    this->response_continue_conversation_ = root["continue_conversation"] | false;
    parsed = true;
    return true;
  });

  if (!parsed || this->response_audio_url_.empty()) {
    this->fail_("invalid_response");
    return;
  }

  this->conversation_last_activity_ms_ = millis();
  if (this->response_audio_url_.rfind("http://", 0) != 0 && this->response_audio_url_.rfind("https://", 0) != 0) {
    if (this->response_audio_url_.front() != '/') {
      this->response_audio_url_.insert(this->response_audio_url_.begin(), '/');
    }
    this->response_audio_url_ = this->api_url_ + this->response_audio_url_;
  }
  this->start_playback_(this->response_audio_url_);
}

void CloudVoice::start_playback_(const std::string &url) {
  this->set_state_(State::REPLYING);
  this->replying_trigger_.trigger();
  this->playback_started_ = false;
  this->playback_finished_.store(false);
  this->playback_started_ms_ = millis();

  auto volume_call = this->media_player_->make_call();
  volume_call.set_volume(this->speaker_volume_ / 100.0f);
  volume_call.perform();

  auto media_call = this->media_player_->make_call();
  media_call.set_media_url(url);
  media_call.set_announcement(true);
  media_call.perform();
}

void CloudVoice::handle_playback_finished_() {
  this->playback_started_ = false;
  if (this->response_continue_conversation_ && this->follow_up_enabled_ &&
      this->follow_up_turn_ < std::min(this->max_follow_up_turns_, HARD_MAX_FOLLOW_UP_TURNS)) {
    this->resume_at_ms_ = millis() + 500;
  } else {
    this->response_continue_conversation_ = false;
    this->follow_up_turn_ = 0;
    this->resume_at_ms_ = millis() + RESUME_DELAY_MS;
  }
}

void CloudVoice::fail_(const char *code) {
  if (this->capturing_.exchange(false)) {
    this->microphone_source_->stop();
  }
  this->response_continue_conversation_ = false;
  this->follow_up_turn_ = 0;
  this->set_state_(State::ERROR);
  ESP_LOGW(TAG, "Cloud Voice error: %s", code);
  this->error_trigger_.trigger();
  this->resume_at_ms_ = millis() + ERROR_MESSAGE_DURATION_MS;
}

void CloudVoice::finish_to_idle_() {
  if (!this->cloud_enabled_ || !this->user_enabled_ || !this->wake_word_switch_->state ||
      this->device_id_.empty() || this->token_.empty()) {
    this->micro_wake_word_->stop();
    this->set_state_(State::MUTED);
    this->idle_trigger_.trigger();
    return;
  }
  this->set_state_(State::IDLE);
  this->idle_trigger_.trigger();
  this->resume_wake_word_();
}

void CloudVoice::resume_wake_word_() {
  if (this->may_listen_() && !this->external_announcement_) {
    this->micro_wake_word_->stop();
    this->micro_wake_word_->start();
  }
}

void CloudVoice::set_state_(State state) { this->state_ = state; }

const char *CloudVoice::state_name() const {
  switch (this->state_) {
    case State::MUTED:
      return "muted";
    case State::IDLE:
      return "idle";
    case State::LISTENING:
      return "listening";
    case State::PROCESSING:
      return "thinking";
    case State::REPLYING:
      return "replying";
    case State::ERROR:
      return "error";
    default:
      return "unknown";
  }
}

void CloudVoice::write_wav_header_(uint8_t header[WAV_HEADER_SIZE], uint32_t pcm_size) {
  const uint32_t riff_size = pcm_size + 36;
  const uint32_t sample_rate = SAMPLE_RATE;
  const uint32_t byte_rate = SAMPLE_RATE * BYTES_PER_SAMPLE;
  const uint16_t block_align = BYTES_PER_SAMPLE;

  memcpy(header, "RIFF", 4);
  memcpy(header + 4, &riff_size, 4);
  memcpy(header + 8, "WAVEfmt ", 8);
  const uint32_t fmt_size = 16;
  const uint16_t audio_format = 1;
  const uint16_t channels = 1;
  const uint16_t bits_per_sample = 16;
  memcpy(header + 16, &fmt_size, 4);
  memcpy(header + 20, &audio_format, 2);
  memcpy(header + 22, &channels, 2);
  memcpy(header + 24, &sample_rate, 4);
  memcpy(header + 28, &byte_rate, 4);
  memcpy(header + 32, &block_align, 2);
  memcpy(header + 34, &bits_per_sample, 2);
  memcpy(header + 36, "data", 4);
  memcpy(header + 40, &pcm_size, 4);
}

uint16_t CloudVoice::calculate_rms_(const uint8_t *data, size_t length) {
  const size_t samples = length / BYTES_PER_SAMPLE;
  if (samples == 0) {
    return 0;
  }

  uint64_t sum = 0;
  for (size_t index = 0; index < samples; index++) {
    int16_t sample;
    memcpy(&sample, data + index * BYTES_PER_SAMPLE, sizeof(sample));
    const int32_t value = sample;
    sum += static_cast<uint64_t>(value * value);
  }
  return static_cast<uint16_t>(sqrt(static_cast<double>(sum) / samples));
}

}  // namespace esphome::cloud_voice
