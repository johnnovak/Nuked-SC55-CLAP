#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ranges>
#include <string>
#include <string_view>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#endif

#include "nuked_sc55.h"
#include "nuked-sc55/common/rom_loader.h"

static std::string get_env_var(const char* var_name);

// #define DEBUG

//----------------------------------------------------------------------------
// Simple debug logging
#ifdef DEBUG

#include <cstdarg>

static FILE* logfile = nullptr;

static void log_init()
{
    if (logfile) {
        return;
    }

#ifdef _WIN32
    if (fopen_s(&logfile, "D:\\nuked-sc55-clap.log", "wb") != 0) {
        logfile = nullptr;
    }
#else
    logfile = fopen("/Users/jnovak/nuked-sc55-clap.log", "wb");
#endif
}

static void _log(const char* fmt, ...)
{
    if (!logfile) {
        return;
    }

    va_list args;
    va_start(args, fmt);

    vfprintf(logfile, fmt, args);
    fprintf(logfile, "\n");
    fflush(logfile);

    va_end(args);
}

static void log_shutdown()
{
    if (logfile) {
        fclose(logfile);
        logfile = nullptr;
    }
}

#define log(...) _log(__VA_ARGS__)
#else

static void log_init() {}
static void log_shutdown() {}

    #define log(...)
#endif

//----------------------------------------------------------------------------

// Get the environment variable value from the provided name, 
// if the variable exists. Returns an empty string if the 
// variable does not exist, or is empty
static std::string get_env_var(const char* var_name)
{
    std::string env_var = {};
#ifdef _WIN32
    auto size = GetEnvironmentVariableA(var_name, nullptr, 0);
    if (size > 0) {
        // Note, 'size' includes the null terminator
        env_var.resize(size - 1);
        GetEnvironmentVariableA(var_name, env_var.data(), size);
    }
#else
    const char* env_var_c_str = getenv(var_name);
    if (env_var_c_str) {
        env_var = env_var_c_str;
    }
#endif
    return env_var;
}

#ifdef _WIN32
    constexpr auto PathSeparator = std::string_view(";");
#else
    constexpr auto PathSeparator = std::string_view(":");
#endif

extern std::string plugin_path;

NukedSc55::NukedSc55(const clap_plugin_t _plugin_class,
                     const clap_host_t* _host, const Model _model)
{
    log_init();

    path = plugin_path;
    log("Plugin path: %s", path.string().c_str());

    plugin_class = _plugin_class;

    plugin_class.plugin_data = this;

    host  = _host;
    model = _model;
}

const clap_plugin_t* NukedSc55::GetPluginClass()
{
    return &plugin_class;
}

// Get a list of potential ROM directories from an environment variable 
// if it is set to a non-empty value. The paths must be absolute directory paths.
// Entries must be separated by the OS PATH separator
std::vector<std::filesystem::path> NukedSc55::GetRomEnvDirs()
{
    constexpr char env_rom_dir_name[] = "SOUNDCANVAS_ROM_PATH";
    std::vector<std::filesystem::path> paths = {};
    const auto env_dir_list = get_env_var(env_rom_dir_name);
    if (env_dir_list.empty()) {
        return paths;
    }
    for (const auto env_dir : std::views::split(env_dir_list, PathSeparator)) {
        auto dir = std::filesystem::path(std::string(env_dir.data(), env_dir.size()));
        if (dir.is_relative()) {
            log("Error: path is relative: %s", dir.string().c_str());
            continue;
        }
        std::error_code ec;
        if (!std::filesystem::is_directory(dir, ec)) {
            if (ec) {
                log("Error getting directory status: %s", ec.message().c_str());
            }
            continue;
        }
        paths.push_back(dir);
    }
    return paths;
}

std::vector<std::filesystem::path> NukedSc55::GetRomBasePaths()
{
    auto paths = GetRomEnvDirs();

    const char* default_rom_dir = "ROMs";

    // Try the Resources folder inside the application bundle first on macOS
#ifdef __APPLE__
    paths.push_back(path / "Resources" / default_rom_dir);
#endif

    const char* resources_dir = "Nuked-SC55-Resources";
    paths.push_back(path.parent_path() / resources_dir / default_rom_dir);

    return paths;
}

bool NukedSc55::Init(const clap_plugin* _plugin_instance)
{
    log("Init");

    plugin_instance = _plugin_instance;

    emu = std::make_unique<Emulator>();

    const EMU_Options opts = {.lcd_backend = nullptr, .nvram_filename = std::filesystem::path{}};
    if (!emu->Init(opts)) {
        log("emu->Init failed");
        emu.reset(nullptr);
        return false;
    }

    auto rom_paths = GetRomBasePaths();
    for (const auto& base_path : rom_paths) {
        auto rom_path = base_path;
        auto romset = "mk1";

        switch (model) {
        case Model::Sc55_v1_00: rom_path /= "SC-55-v1.00"; break;
        case Model::Sc55_v1_10: rom_path /= "SC-55-v1.10"; break;
        case Model::Sc55_v1_20: rom_path /= "SC-55-v1.20"; break;
        case Model::Sc55_v1_21: rom_path /= "SC-55-v1.21"; break;
        case Model::Sc55_v2_00: rom_path /= "SC-55-v2.00"; break;
        case Model::Sc55mk2_v1_01:
            romset = "mk2";
            rom_path /= "SC-55mk2-v1.01";
            break;
        default: assert(false);
        }

        log("Trying ROM dir: %s", rom_path.string().c_str());

        AllRomsetInfo romset_info = {};
        common::LoadRomsetResult load_result = {};
        common::RomOverrides rom_overrides;
        common::LoadRomsetError err = common::LoadRomset(romset_info, rom_path, romset, false, rom_overrides, load_result);
        if (err != common::LoadRomsetError{}) {
            log("emu->LoadRomset failed. Trying next directory");
            continue;
        }
        RomLocationSet loaded = {};
        if (!emu->LoadRoms(load_result.romset, romset_info, &loaded)) {
            log("emu->LoadRoms failed");
            emu.reset(nullptr);
            return false;
        }
        return true;
    }
    log("Init failed, tried all ROM directories");
    emu.reset(nullptr);
    return false;
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
#ifdef DEBUG
    log("Activate: requested_sample_rate: %g, min_frame_count: %d, max_frame_count: %d",
        requested_sample_rate,
        min_frame_count,
        max_frame_count);
#else
    (min_frame_count);
#endif

    emu->Reset();
    emu->GetPCM().enable_oversampling = false;
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
	assert(out_left && out_right);

	assert(render_buf.size() == 2);
	assert(render_buf[0].size() >= num_frames);
	assert(render_buf[1].size() >= num_frames);

        for (size_t i = 0; i < num_frames; ++i) {
            out_left[i]  = render_buf[0][i];
            out_right[i] = render_buf[1][i];
        }

        render_buf[0].clear();
        render_buf[1].clear();
    }

    return CLAP_PROCESS_CONTINUE;
}

bool NukedSc55::LoadState(const clap_istream_t* stream)
{
    if (!emu) {
        return false;
    }

    (stream); // TODO return true once implemented
    return false;
}

bool NukedSc55::SaveState(const clap_ostream_t* stream)
{
    if (!emu) {
        return 0;
    }

    (stream); // TODO return actual number of bytes written once implemented
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

    (out);
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

[[maybe_unused]] static const char* status_to_string(const uint8_t status)
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

[[maybe_unused]] static void log_midi_message(const clap_event_midi_t* event)
{
    const uint8_t status = static_cast<uint8_t>(event->data[0] & 0xf0U);

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
            0, //channel,
            status_to_string(status));
        break;

    default:
        log("MIDI event: %02x %02x    | Ch %d, %s",
            event->data[0],
            event->data[1],
            0, //channel,
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
            const auto status = midi_event->data[0] & 0xf0;

            switch (status) {
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

    const auto input_len  = static_cast<spx_uint32_t>(render_buf[0].size());
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

        in_len  = static_cast<spx_uint32_t>(render_buf[0].size());
        out_len = num_out_frames_remaining;

        speex_resampler_process_float(resampler,
                                      0,
                                      render_buf[0].data(),
                                      &in_len,
                                      out_left + curr_out_pos,
                                      &out_len);

        in_len  = static_cast<spx_uint32_t>(render_buf[1].size());
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
