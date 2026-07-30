from esphome import automation
import esphome.codegen as cg
from esphome.components import esp32, media_player, micro_wake_word, microphone, switch
from esphome.components.http_request import validate_url
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MICROPHONE

AUTO_LOAD = ["json"]
DEPENDENCIES = ["esp32", "media_player", "micro_wake_word", "microphone", "network"]
CODEOWNERS = ["@smarthomeshop"]

CONF_API_URL = "api_url"
CONF_MEDIA_PLAYER = "media_player"
CONF_MICRO_WAKE_WORD = "micro_wake_word"
CONF_WAKE_WORD_SWITCH = "wake_word_switch"
CONF_MAX_RECORDING_SECONDS = "max_recording_seconds"
CONF_MIN_RECORDING_SECONDS = "min_recording_seconds"
CONF_SILENCE_SECONDS = "silence_seconds"
CONF_SPEECH_RMS = "speech_rms"
CONF_SILENCE_RMS = "silence_rms"
CONF_ON_LISTENING = "on_listening"
CONF_ON_PROCESSING = "on_processing"
CONF_ON_REPLYING = "on_replying"
CONF_ON_IDLE = "on_idle"
CONF_ON_ERROR = "on_error"

cloud_voice_ns = cg.esphome_ns.namespace("cloud_voice")
CloudVoice = cloud_voice_ns.class_("CloudVoice", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(CloudVoice),
        cv.Required(CONF_API_URL): validate_url,
        cv.Required(CONF_MICROPHONE): microphone.microphone_source_schema(
            min_bits_per_sample=16,
            max_bits_per_sample=16,
            min_channels=1,
            max_channels=1,
        ),
        cv.Required(CONF_MEDIA_PLAYER): cv.use_id(media_player.MediaPlayer),
        cv.Required(CONF_MICRO_WAKE_WORD): cv.use_id(
            micro_wake_word.MicroWakeWord,
        ),
        cv.Required(CONF_WAKE_WORD_SWITCH): cv.use_id(switch.Switch),
        cv.Optional(CONF_MAX_RECORDING_SECONDS, default=8): cv.int_range(
            min=3,
            max=15,
        ),
        cv.Optional(CONF_MIN_RECORDING_SECONDS, default=2.5): cv.float_range(
            min=0.5,
            max=5,
        ),
        cv.Optional(CONF_SILENCE_SECONDS, default=0.9): cv.float_range(
            min=0.3,
            max=3,
        ),
        cv.Optional(CONF_SPEECH_RMS, default=50): cv.int_range(
            min=1,
            max=32767,
        ),
        cv.Optional(CONF_SILENCE_RMS, default=30): cv.int_range(
            min=0,
            max=32767,
        ),
        cv.Optional(CONF_ON_LISTENING): automation.validate_automation(),
        cv.Optional(CONF_ON_PROCESSING): automation.validate_automation(),
        cv.Optional(CONF_ON_REPLYING): automation.validate_automation(),
        cv.Optional(CONF_ON_IDLE): automation.validate_automation(),
        cv.Optional(CONF_ON_ERROR): automation.validate_automation(),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    mic_source = await microphone.microphone_source_to_code(
        config[CONF_MICROPHONE],
    )
    player = await cg.get_variable(config[CONF_MEDIA_PLAYER])
    wake_word = await cg.get_variable(config[CONF_MICRO_WAKE_WORD])
    wake_word_switch = await cg.get_variable(config[CONF_WAKE_WORD_SWITCH])

    cg.add(var.set_microphone_source(mic_source))
    cg.add(var.set_media_player(player))
    cg.add(var.set_micro_wake_word(wake_word))
    cg.add(var.set_wake_word_switch(wake_word_switch))
    cg.add(var.set_api_url(config[CONF_API_URL]))
    cg.add(var.set_max_recording_seconds(config[CONF_MAX_RECORDING_SECONDS]))
    cg.add(var.set_min_recording_seconds(config[CONF_MIN_RECORDING_SECONDS]))
    cg.add(var.set_silence_seconds(config[CONF_SILENCE_SECONDS]))
    cg.add(var.set_speech_rms(config[CONF_SPEECH_RMS]))
    cg.add(var.set_silence_rms(config[CONF_SILENCE_RMS]))

    for conf in config.get(CONF_ON_LISTENING, []):
        await automation.build_automation(
            var.get_listening_trigger(),
            [],
            conf,
        )
    for conf in config.get(CONF_ON_PROCESSING, []):
        await automation.build_automation(
            var.get_processing_trigger(),
            [],
            conf,
        )
    for conf in config.get(CONF_ON_REPLYING, []):
        await automation.build_automation(
            var.get_replying_trigger(),
            [],
            conf,
        )
    for conf in config.get(CONF_ON_IDLE, []):
        await automation.build_automation(
            var.get_idle_trigger(),
            [],
            conf,
        )
    for conf in config.get(CONF_ON_ERROR, []):
        await automation.build_automation(
            var.get_error_trigger(),
            [],
            conf,
        )

    esp32.include_builtin_idf_component("esp_http_client")
    esp32.add_idf_sdkconfig_option("CONFIG_MBEDTLS_CERTIFICATE_BUNDLE", True)
