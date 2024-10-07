#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "nuked_sc55.h"

#define DEBUG

//----------------------------------------------------------------------------
// Simple debug logging
// #ifdef DEBUG

static FILE* logfile = nullptr;

static void log_init()
{
    logfile = fopen("/Users/jnovak/nuked-sc55-clap.log", "wb");
    //    logfile = fopen("D:\nuked-sc55-clap.log", "wb");
}

static void _log(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    vfprintf(logfile, fmt, args);
    fprintf(logfile, "\n");
    fflush(logfile);

    va_end(args);
}

static void log_shutdown()
{
    fclose(logfile);
}

#define log(...) _log(__VA_ARGS__)
#else

static void log_init() {}
static void log_shutdown() {}

    #define log(...)
#endif

//----------------------------------------------------------------------------

extern const char* plugin_path;

NukedSc55::NukedSc55(const clap_plugin_t _plugin_class,
                     const clap_host_t* _host, const Model _model)
{
    log_init();

    path = plugin_path;
    log("Plugin path: %s", path.c_str());

    plugin_class = _plugin_class;

    plugin_class.plugin_data = this;

    host  = _host;
    model = _model;
}

const clap_plugin_t* NukedSc55::GetPluginClass()
{
    return &plugin_class;
}

std::filesystem::path NukedSc55::GetRomBasePath()
{
    const char* rom_dir = "ROMs";

#ifdef __APPLE__
    // Try the Resources folder inside the application bundle first on macOS
    const auto bundle_rom_path = path / "Resources" / rom_dir;

    if (std::filesystem::exists(bundle_rom_path)) {
        log("Found ROM directory within application bundle, ROM base path: %s",
            bundle_rom_path.c_str());

        return bundle_rom_path;
    }
#endif
    const char* resources_dir = "NukedSC55-Resources";

    const auto rom_path = path.parent_path() / resources_dir / rom_dir;
    log("ROM base path: %s", rom_path.c_str());

    return rom_path;
}

bool NukedSc55::Init(const clap_plugin* _plugin_instance)
{
    log("Init");

    plugin_instance = _plugin_instance;

    emu = std::make_unique<Emulator>();

    const EMU_Options opts = {.enable_lcd = false};
    if (!emu->Init(opts)) {
        log("emu->Init failed");
        emu.reset(nullptr);
        return false;
    }

    auto rom_path = GetRomBasePath();
    auto romset   = Romset::MK1;

    switch (model) {
    case Model::Sc55_v1_20: rom_path /= "SC-55-v1.20"; break;
    case Model::Sc55_v1_21: rom_path /= "SC-55-v1.21"; break;
    case Model::Sc55_v2_00: rom_path /= "SC-55-v2.00"; break;
    case Model::Sc55mk2_v1_01:
        romset = Romset::MK2;
        rom_path /= "SC-55mk2-v1.01";
        break;
    default: assert(false);
    }

    log("ROM dir: %s", rom_path.c_str());

    if (!emu->LoadRoms(romset, rom_path)) {
        log("emu->LoadRoms failed");
        emu.reset(nullptr);
        return false;
    }

    return true;
}

void NukedSc55::Shutdown()
{
    log("Shutdown");

    if (resampler) {
        speex_resampler_destroy(resampler);
        resampler = nullptr;
    }
    log_shutdown();
}

static void receive_sample(void* userdata, const AudioFrame<int32_t>& in)
{
    assert(userdata);
    auto emu = reinterpret_cast<NukedSc55*>(userdata);

    AudioFrame<float> out = {};
    Normalize(in, out);

    emu->PublishFrame(out.left, out.right);
}

bool NukedSc55::Activate(const double requested_sample_rate,
                         const uint32_t min_frame_count,
                         const uint32_t max_frame_count)
{
    log("Activate: requested_sample_rate: %g, min_frame_count: %d, max_frame_count: %d",
        requested_sample_rate,
        min_frame_count,
        max_frame_count);

    emu->Reset();
    emu->GetPCM().disable_oversampling = true;
    emu->PostSystemReset(EMU_SystemReset::GS_RESET);

    // Speed up the devices' bootup delay
    const size_t num_steps = (model == Model::Sc55mk2_v1_01) ? 9'500'000 : 700'000;

    for (size_t i = 0; i < num_steps; i++) {
        MCU_Step(emu->GetMCU());
    }

    emu->SetSampleCallback(receive_sample, this);

    render_sample_rate_hz = PCM_GetOutputFrequency(emu->GetPCM());

    log("render_sample_rate_hz: %g", render_sample_rate_hz);

    if (requested_sample_rate != render_sample_rate_hz) {
        do_resample = true;

        output_sample_rate_hz = requested_sample_rate;

        // Initialise Speex resampler
        resample_ratio = render_sample_rate_hz / output_sample_rate_hz;

        const spx_uint32_t in_rate_hz = static_cast<int>(render_sample_rate_hz);
        const spx_uint32_t out_rate_hz = static_cast<int>(output_sample_rate_hz);

        constexpr auto NumChannels     = 2; // always stereo
        constexpr auto ResampleQuality = SPEEX_RESAMPLER_QUALITY_DESKTOP;

        resampler = speex_resampler_init(
            NumChannels, in_rate_hz, out_rate_hz, ResampleQuality, nullptr);

        speex_resampler_set_rate(resampler, in_rate_hz, out_rate_hz);
        speex_resampler_skip_zeros(resampler);

        const auto max_render_buf_size = static_cast<size_t>(
            static_cast<double>(max_frame_count) * resample_ratio * 1.10f);

        render_buf[0].reserve(max_render_buf_size);
        render_buf[1].reserve(max_render_buf_size);

    } else {
        do_resample = false;

        output_sample_rate_hz = render_sample_rate_hz;
        resample_ratio        = 1.0;

        render_buf[0].reserve(max_frame_count);
        render_buf[1].reserve(max_frame_count);
    }

    log("do_resample: %s", do_resample ? "true" : "false");
    log("output_sample_rate_hz: %g", output_sample_rate_hz);
    log("resample_ratio: %g", resample_ratio);

    return true;
}

clap_process_status NukedSc55::Process(const clap_process_t* process)
{
    if (!emu) {
        return CLAP_PROCESS_ERROR;
    }

    assert(process->audio_outputs_count == 1);
    assert(process->audio_inputs_count == 0);

    const uint32_t num_frames = process->frames_count;
    const uint32_t num_events = process->in_events->size(process->in_events);
    log("--- num_frames: %d, num_events: %d", num_frames, num_events);

    uint32_t event_index      = 0;
    uint32_t next_event_frame = (num_events == 0) ? num_frames : 0;

    for (uint32_t curr_frame = 0; curr_frame < num_frames;) {
        while (event_index < num_events && next_event_frame == curr_frame) {

            const auto event = process->in_events->get(process->in_events,
                                                       event_index);
            if (event->time != curr_frame) {
                next_event_frame = event->time;
                break;
            }

            ProcessEvent(event);
            ++event_index;

            if (event_index == num_events) {
                // We've reached the end of the event list
                next_event_frame = num_frames;
                break;
            }
        }

        const auto num_frames_to_render = static_cast<int>(
            static_cast<double>(next_event_frame - curr_frame) * resample_ratio);

        // Render samples until the next event
        RenderAudio(num_frames_to_render);

        curr_frame = next_event_frame;
    }

    auto out_left  = process->audio_outputs[0].data32[0];
    auto out_right = process->audio_outputs[0].data32[1];

    if (do_resample) {
        ResampleAndPublishFrames(num_frames, out_left, out_right);

    } else {
        for (size_t i = 0; i < num_frames; ++i) {
            out_left[i]  = render_buf[0][i];
            out_right[i] = render_buf[1][i];

            render_buf[0].clear();
            render_buf[1].clear();
        }
    }

    return CLAP_PROCESS_CONTINUE;
}

bool NukedSc55::LoadState(const clap_istream_t* stream)
{
    if (!emu) {
        return false;
    }

    // TODO return true once implemented
    return false;
}

bool NukedSc55::SaveState(const clap_ostream_t* stream)
{
    if (!emu) {
        return 0;
    }

    // TODO return actual number of bytes written once implemented
    return 0;
}

void NukedSc55::Flush(const clap_input_events_t* in, const clap_output_events_t* out)
{
    if (!emu) {
        return;
    }

    log("Flush");

    const uint32_t num_events = in->size(in);

    // Process events sent to our plugin from the host.
    for (uint32_t event_index = 0; event_index < num_events; ++event_index) {
        ProcessEvent(in->get(in, event_index));
    }
}

void NukedSc55::PublishFrame(const float left, const float right)
{
    render_buf[0].emplace_back(left);
    render_buf[1].emplace_back(right);
}

constexpr uint8_t NoteOff         = 0x80;
constexpr uint8_t NoteOn          = 0x90;
constexpr uint8_t PolyKeyPressure = 0xa0;
constexpr uint8_t ControlChange   = 0xb0;
constexpr uint8_t ProgramChange   = 0xc0;
constexpr uint8_t ChannelPressure = 0xd0;
constexpr uint8_t PitchBend       = 0xe0;

static const char* status_to_string(const uint8_t status)
{
    switch (status) {
    case NoteOff: return "NoteOff"; break;
    case NoteOn: return "NoteOn"; break;
    case PolyKeyPressure: return "PolyKeyPressure"; break;
    case ControlChange: return "ControlChange"; break;
    case ProgramChange: return "ProgramChange"; break;
    case ChannelPressure: return "ChannelPressure"; break;
    case PitchBend: return "PitchBend"; break;
    default: return "unknown";
    }
}

static void log_midi_message(const clap_event_midi_t* event)
{
    const auto status  = event->data[0] & 0xf0;
    const auto channel = event->data[0] & 0x0f;

    // 3-byte messages
    switch (status) {
    case NoteOff:
    case NoteOn:
    case PolyKeyPressure:
    case ControlChange:
    case PitchBend:
        log("MIDI event: %02x %02x %02x | Ch %d, %s",
            event->data[0],
            event->data[1],
            event->data[2],
            channel,
            status_to_string(status));
        break;

    default:
        log("MIDI event: %02x %02x    | Ch %d, %s",
            event->data[0],
            event->data[1],
            channel,
            status_to_string(status));
    }
}

void NukedSc55::ProcessEvent(const clap_event_header_t* event)
{
    if (event->space_id == CLAP_CORE_EVENT_SPACE_ID) {

        switch (event->type) {
        case CLAP_EVENT_MIDI: {
            const auto midi_event = reinterpret_cast<const clap_event_midi_t*>(event);

            emu->PostMIDI(midi_event->data[0]);
            emu->PostMIDI(midi_event->data[1]);

            // 3-byte messages
            switch (const auto status = midi_event->data[0] & 0xf0) {
            case NoteOff:
            case NoteOn:
            case PolyKeyPressure:
            case ControlChange:
            case PitchBend: emu->PostMIDI(midi_event->data[2]); break;
            }
#ifdef DEBUG
            log_midi_message(midi_event);
#endif
        } break;

        case CLAP_EVENT_MIDI_SYSEX: {
            const auto sysex_event = reinterpret_cast<const clap_event_midi_sysex*>(
                event);

            emu->PostMIDI(std::span{sysex_event->buffer, sysex_event->size});

            log("SysEx message, length: %d", sysex_event->size);
        } break;
        }
    }
}

void NukedSc55::RenderAudio(const uint32_t num_frames)
{
    const auto start_size = render_buf[0].size();

    log("RenderAudio: num_frames: %d, start_size: %d", num_frames, start_size);

    while (render_buf[0].size() - start_size < num_frames) {
        MCU_Step(emu->GetMCU());
    }

    log("  num_rendered: %d", render_buf[0].size() - start_size);
}

void NukedSc55::ResampleAndPublishFrames(const uint32_t num_out_frames,
                                         float* out_left, float* out_right)
{
    log("RenderAndPublishFrames: num_out_frames: %d", num_out_frames);

    const auto input_len  = render_buf[0].size();
    const auto output_len = num_out_frames;

    log("  input_len: %d", input_len);

    spx_uint32_t in_len  = input_len;
    spx_uint32_t out_len = output_len;

    speex_resampler_process_float(
        resampler, 0, render_buf[0].data(), &in_len, out_left, &out_len);

    in_len  = input_len;
    out_len = output_len;

    speex_resampler_process_float(
        resampler, 1, render_buf[1].data(), &in_len, out_right, &out_len);

    // Speex returns the number actually consumed and written samples in
    // `in_len` and `out_len`, respectively. There are three outcomes:
    //
    // 1) The input buffer hasn't been fully consumed, but the output buffer
    //    has been completely filled.
    //
    // 2) The output buffer hasn't been filled completely, but all input
    //    samples have been consumed.
    //
    // 3) All input samples have been consumed and the output buffer has been
    //    completely filled.
    //
    if (out_len < output_len) {
        // Case 2: The output buffer hasn't been filled completely; we need to
        // generate more input samples.
        //
        const auto num_out_frames_remaining = output_len - out_len;
        const auto curr_out_pos             = out_len;

        // "It's the only way to be sure"
        const auto render_frame_count = static_cast<int>(std::ceil(
            static_cast<double>(num_out_frames_remaining) * resample_ratio));

        render_buf[0].clear();
        render_buf[1].clear();

        RenderAudio(render_frame_count);

        in_len  = render_buf[0].size();
        out_len = num_out_frames_remaining;

        speex_resampler_process_float(resampler,
                                      0,
                                      render_buf[0].data(),
                                      &in_len,
                                      out_left + curr_out_pos,
                                      &out_len);

        in_len  = render_buf[1].size();
        out_len = num_out_frames_remaining;

        speex_resampler_process_float(resampler,
                                      1,
                                      render_buf[1].data(),
                                      &in_len,
                                      out_right + curr_out_pos,
                                      &out_len);
    }

    if (in_len < input_len) {
        // Case 1: The input buffer hasn't been fully consumed; we have
        // leftover input samples that we need to keep for the next Process()
        // call.
        //
        if (in_len > 0) {
            render_buf[0].erase(render_buf[0].begin(), render_buf[0].begin() + in_len);
            render_buf[1].erase(render_buf[1].begin(), render_buf[1].begin() + in_len);
        }

    } else {
        // Case 3: All input samples have been consumed and the output buffer
        // has been completely filled.
        //
        render_buf[0].clear();
        render_buf[1].clear();
    }
}
