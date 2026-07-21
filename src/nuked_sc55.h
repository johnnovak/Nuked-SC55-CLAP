#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <memory>
#include <vector>

#include "clap/clap.h"
#include "nuked-sc55/backend/emu.h"
#include "speex/speex_resampler.h"

class NukedSc55 {
public:
    enum class Model { Sc55_v1_00, Sc55_v1_10, Sc55_v1_20, Sc55_v1_21, Sc55_v2_00, Sc55mk2_v1_01 };

    // Init/shutdown
    NukedSc55(const clap_plugin_t plugin_class, const clap_host_t* host,
              const Model model);

    const clap_plugin_t* GetPluginClass();

    bool Init(const clap_plugin* plugin_instance);
    void Shutdown();

    bool Activate(const double sample_rate, const uint32_t min_frame_count,
                  const uint32_t max_frame_count);

    // Processing
    clap_process_status Process(const clap_process_t* process);

    void Flush(const clap_input_events_t* in, const clap_output_events_t* out);

    void PublishFrame(const float left, const float right);

    // State handling
    bool LoadState(const clap_istream_t* stream);
    bool SaveState(const clap_ostream_t* stream);

private:
    std::filesystem::path path = {};

    Model model = {};

    clap_plugin_t plugin_class         = {};
    const clap_host_t* host            = nullptr;
    const clap_plugin* plugin_instance = nullptr;

    // Minimal no-op LCD backend (D4). Declared before `emu` so it outlives the
    // emulator, which holds a raw pointer to it. Its presence is what makes
    // lcd.cpp maintain the character RAM we scan for the "Buff. Full!" warning;
    // no rendering ever runs (StartLCD() is never called).
    std::unique_ptr<LCD_Backend> lcd_watcher = nullptr;
    bool buff_full_seen                      = false;

    std::unique_ptr<Emulator> emu = nullptr;

    double render_sample_rate_hz = 0.0;
    double output_sample_rate_hz = 0.0;

    std::array<std::vector<float>, 2> render_buf = {};

    SpeexResamplerState* resampler = nullptr;
    bool do_resample               = false;
    double resample_ratio          = 0.0f;

    // Wire-rate MIDI intake queue (Fix 2). CLAP events are enqueued here and
    // fed into the emulator's UART ring at the 31250-baud wire rate (one byte
    // per ~320 us of emulated time), so byte-stream consumers never receive a
    // burst a real cable could not have delivered.
    std::deque<uint8_t> midi_queue = {};
    double samples_per_byte        = 0.0; // render frames per wire byte
    double midi_byte_deadline      = 0.0; // next feed time, in render frames
    uint64_t frames_rendered_total = 0;   // monotonic render-frame clock

    // UART RX ring headroom watchdog (D2/D3), edge-triggered so we log the
    // crossing rather than every byte above the threshold.
    bool ring_above_highwater = false;
    bool ring_above_nearfull  = false;

    // Methods
    std::vector<std::filesystem::path> GetRomEnvDirs();
    std::vector<std::filesystem::path> GetRomBasePaths();

    void ProcessEvent(const clap_event_header_t* event);

    bool EnqueueMidiMessage(const uint8_t* bytes, const size_t len,
                            const uint8_t status);
    void FeedQueuedMidi();
    uint32_t RingUnreadBytes();
    void UpdateRingWatermarks(const uint32_t unread);

    void CheckBuffFull();

    void RenderAudio(const uint32_t num_frames);

    void ResampleAndPublishFrames(const uint32_t num_out_frames,
                                  float* out_left, float* out_right);
};
