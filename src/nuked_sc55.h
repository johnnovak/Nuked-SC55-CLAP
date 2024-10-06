#pragma once

#include <array>
#include <memory>
#include <vector>

#include "clap/clap.h"
#include "nuked-sc55/emu.h"
#include "speex/speex_resampler.h"

class NukedSc55 {

public:
    enum class Model { Sc55_v1_20, Sc55_v1_21, Sc55_v2_00, Sc55mk2_v1_01 };

public:
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

    void PublishSample(const float left, const float right);

    // State handling
    bool LoadState(const clap_istream_t* stream);
    bool SaveState(const clap_ostream_t* stream);

private:
    void ProcessEvent(const clap_event_header_t* event);

    void RenderAudio(const uint32_t num_frames);

    void ResampleAndPublishFrames(const uint32_t num_out_frames,
                                  float* out_left, float* out_right);

private:
    clap_plugin_t plugin_class         = {};
    const clap_host_t* host            = nullptr;
    const clap_plugin* plugin_instance = nullptr;

    Model model = {};

    std::unique_ptr<Emulator> emu = nullptr;

    double render_sample_rate_hz = 0.0;
    double output_sample_rate_hz = 0.0;

    SpeexResamplerState* resampler = nullptr;
    bool do_resample               = false;
    double resample_ratio          = 0.0f;

    std::array<std::vector<float>, 2> render_buf = {};
};
