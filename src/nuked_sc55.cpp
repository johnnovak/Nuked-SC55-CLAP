#include <algorithm>
#include <cassert>
#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
#include "nuked-sc55/backend/diagnostics.h"
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
    logfile = fopen("/Users/jnovak/nuked-sc55-clap.log", "wb");
    //logfile = fopen("D:\\nuked-sc55-clap.log", "wb");
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
// Release-build diagnostics
//
// Unlike the DEBUG-only `log()` above, these are active in release builds so
// that a MIDI data-loss event leaves a trail instead of being silent. Written
// to stderr with a fixed prefix. Repetitive events are rate-limited with a
// plain static counter: the 1st occurrence is logged, then every 100th.

static void diag_logf(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "Nuked-SC55-CLAP: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    fflush(stderr);
    va_end(args);
}

// Returns true if this occurrence should be logged (1st, then every 100th).
static bool rate_limited(uint64_t& counter)
{
    const bool emit = (counter % 100) == 0;
    ++counter;
    return emit;
}

//----------------------------------------------------------------------------
// Fix 2 intake-queue / ring parameters.

// The cap must exceed the largest legal single message. DOSBox permits SysEx
// up to 20 KB, so a 16 KB cap would make a legal 20 KB SysEx permanently
// un-admittable under message-atomic admission. 32 KB is ~10 s of wire-rate
// MIDI, reachable only by a pathological flood.
constexpr size_t MidiQueueCapBytes = 32 * 1024;

// The firmware RX ring is uart_buffer_size (8192) bytes. Stop feeding while it
// still holds this many unread bytes, so the unguarded MCU_PostUART() write
// pointer can never lap the read pointer from our side.
constexpr uint32_t RingHighWaterBytes = 4096;
constexpr uint32_t RingNearFullBytes  = 7680;

// 31250 baud, 10 bits per byte (8 data + start + stop) => 320 us per byte.
constexpr double MidiWireBitsPerByte = 10.0;
constexpr double MidiWireBaud        = 31250.0;

//----------------------------------------------------------------------------
// D4: minimal no-op LCD backend. We never call StartLCD() or LCD_Render(), so
// none of these methods are ever invoked. The instance exists solely so
// lcd.cpp stops short-circuiting LCD_Write() and keeps the firmware's
// character RAM (lcd.LCD_Data) current, letting us watch for "Buff. Full!".
class NullLcdBackend : public LCD_Backend {
public:
    bool Start(const lcd_t&) override { return true; }
    void Stop() override {}
    void Render() override {}
};

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

    // Disable jcmoyer/Nuked-SC55's diagnostics output (defaults to stderr) - a plugin has no
    // attached console, and some of these calls are reachable from the audio thread.
    Diag_SetCallback(nullptr);

    emu = std::make_unique<Emulator>();

    // D4: pass a no-op LCD backend so lcd.cpp maintains the character RAM we
    // scan for the firmware's "Buff. Full!" warning. StartLCD() is never
    // called, so no rendering happens.
    lcd_watcher = std::make_unique<NullLcdBackend>();

    const EMU_Options opts = {.lcd_backend   = lcd_watcher.get(),
                              .nvram_filename = std::filesystem::path{}};
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

        common::LoadRomsetResult load_result = {};
        common::RomOverrides rom_overrides;
        common::LoadRomsetError err =
            common::LoadRomset(rom_path, romset, common::RomLoader::Hashing, rom_overrides, load_result);
        if (err != common::LoadRomsetError{}) {
            log("emu->LoadRomset failed. Trying next directory");
            continue;
        }
        RomLocationSet loaded = {};
        if (!emu->LoadRoms(load_result.romset, load_result.romset_info, &loaded)) {
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
    log("Activate: requested_sample_rate: %g, min_frame_count: %d, max_frame_count: %d",
        requested_sample_rate,
        min_frame_count,
        max_frame_count);

    emu->Reset();
    emu->GetPCM().enable_oversampling = false;

    // Clear the intake queue and pacing/watchdog state so a host cycling the
    // plugin never replays stale bytes. (The GS reset below is posted directly
    // to the ring during bootup, not through the paced queue.)
    midi_queue.clear();
    midi_byte_deadline     = 0.0;
    frames_rendered_total  = 0;
    ring_above_highwater   = false;
    ring_above_nearfull    = false;
    buff_full_seen         = false;

    emu->PostSystemReset(EMU_SystemReset::GS_RESET);

    // Speed up the devices' bootup delay
    const size_t num_steps = (model == Model::Sc55mk2_v1_01) ? 9'500'000 : 700'000;

    for (size_t i = 0; i < num_steps; i++) {
        MCU_Step(emu->GetMCU());
    }

    emu->SetSampleCallback(receive_sample, this);

    render_sample_rate_hz = PCM_GetOutputFrequency(emu->GetPCM());

    // Wire pacing clock uses rendered frames, not MCU cycles: one wire byte
    // every 320 us equals this many render frames (10.24 @ 32 kHz).
    samples_per_byte = render_sample_rate_hz * (MidiWireBitsPerByte / MidiWireBaud);

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

    CheckBuffFull();

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
    ++frames_rendered_total;
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

// Total length in bytes (status byte included) of the MIDI message introduced
// by `status`, or 0 if the byte carries nothing we should forward to the
// device: the undefined System Common statuses (0xF4/0xF5), the SysEx
// delimiters (0xF0/0xF7, which reach us via CLAP_EVENT_MIDI_SYSEX), or a data
// byte the host wrongly placed in the status slot (< 0x80).
static int MidiMessageLength(const uint8_t status)
{
    if (status < 0x80) {
        return 0; // data byte in the status slot (host bug)
    }
    if (status < 0xf0) {
        // Channel voice message
        switch (status & 0xf0) {
        case ProgramChange:   // 0xCn
        case ChannelPressure: // 0xDn
            return 2;
        default: // NoteOff / NoteOn / PolyKeyPressure / ControlChange / PitchBend
            return 3;
        }
    }
    // System Common / System Real Time
    switch (status) {
    case 0xf2: // Song Position Pointer
        return 3;
    case 0xf1: // MTC Quarter Frame
    case 0xf3: // Song Select
        return 2;
    case 0xf6: // Tune Request
    case 0xf8: // Timing Clock
    case 0xf9: // (undefined real time)
    case 0xfa: // Start
    case 0xfb: // Continue
    case 0xfc: // Stop
    case 0xfd: // (undefined real time)
    case 0xfe: // Active Sensing
    case 0xff: // System Reset
        return 1;
    default: // 0xf0, 0xf4, 0xf5, 0xf7
        return 0;
    }
}

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
    const auto status = event->data[0] & 0xf0;

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

            const uint8_t status = midi_event->data[0];
            const int len        = MidiMessageLength(status);

            // Enqueue exactly the bytes this status carries; the paced feed in
            // RenderAudio() drains the queue into the device at wire rate.
            // Framing exactness (see MidiMessageLength) avoids appending a junk
            // data byte to 1-byte System Real Time messages or truncating 0xF2.
            if (len > 0) {
                EnqueueMidiMessage(midi_event->data, static_cast<size_t>(len), status);
            } else if (status < 0x80) {
                // D5: a data byte in the status slot means the host mis-framed
                // the stream. Legitimately zero-length statuses (SysEx
                // delimiters, undefined System Common) are ignored silently.
                static uint64_t d5_count = 0;
                if (rate_limited(d5_count)) {
                    diag_logf("D5: data byte 0x%02x in status slot ignored (host framing bug)",
                              status);
                }
            }
#ifdef DEBUG
            log_midi_message(midi_event);
#endif
        } break;

        case CLAP_EVENT_MIDI_SYSEX: {
            const auto sysex_event = reinterpret_cast<const clap_event_midi_sysex*>(
                event);

            // Enqueue the whole SysEx so ordering with channel messages is
            // preserved; message-atomic admission drops it wholesale if it
            // cannot fit the cap (D1).
            EnqueueMidiMessage(sysex_event->buffer, sysex_event->size, 0xf0);

            log("SysEx message, length: %d", sysex_event->size);
        } break;
        }
    }
}

// Message-atomic admission into the intake queue: enqueue the whole message or
// drop the whole message. Individual bytes are never dropped, which would
// desync the byte stream - the very failure Fix 2 exists to prevent.
bool NukedSc55::EnqueueMidiMessage(const uint8_t* bytes, const size_t len,
                                   const uint8_t status)
{
    if (midi_queue.size() + len > MidiQueueCapBytes) {
        // D1: intake queue overflow.
        static uint64_t d1_count = 0;
        if (rate_limited(d1_count)) {
            diag_logf("D1: intake queue full (cap=%zu), dropped message status=0x%02x len=%zu depth=%zu",
                      MidiQueueCapBytes, status, len, midi_queue.size());
        }
        return false;
    }
    midi_queue.insert(midi_queue.end(), bytes, bytes + len);
    return true;
}

// Unread bytes in the firmware's UART RX ring. uart_buffer_size is a power of
// two, so the unsigned wrap of (write - read) gives the correct occupancy.
uint32_t NukedSc55::RingUnreadBytes()
{
    const auto& mcu = emu->GetMCU();
    return (mcu.uart_write_ptr - mcu.uart_read_ptr) % uart_buffer_size;
}

void NukedSc55::UpdateRingWatermarks(const uint32_t unread)
{
    if (unread >= RingHighWaterBytes) {
        if (!ring_above_highwater) {
            ring_above_highwater = true;
            // D2: ring high-water. Pacing is active yet the ring is half full,
            // so something upstream is flooding.
            static uint64_t d2_count = 0;
            if (rate_limited(d2_count)) {
                diag_logf("D2: UART RX ring high-water crossed: %u/%u unread bytes",
                          unread, uart_buffer_size);
            }
        }
    } else {
        ring_above_highwater = false;
    }

    if (unread >= RingNearFullBytes) {
        if (!ring_above_nearfull) {
            ring_above_nearfull = true;
            // D3: ring near-full. Unreachable given the headroom guard below
            // unless Fix 2 is bypassed - which is exactly what it would prove.
            static uint64_t d3_count = 0;
            if (rate_limited(d3_count)) {
                diag_logf("D3: UART RX ring near-full: %u/%u unread bytes (imminent wrap)",
                          unread, uart_buffer_size);
            }
        }
    } else {
        ring_above_nearfull = false;
    }
}

// Feed queued bytes into the device at wire rate, at most one byte per
// samples_per_byte render frames. max(deadline, now) grants no burst credit
// after an idle gap; the ring headroom guard keeps the unguarded ring safe.
void NukedSc55::FeedQueuedMidi()
{
    const double now = static_cast<double>(frames_rendered_total);

    while (!midi_queue.empty() && now >= midi_byte_deadline) {
        const uint32_t unread = RingUnreadBytes();
        UpdateRingWatermarks(unread);

        if (unread >= RingHighWaterBytes) {
            // Ring busy; retry on a later step without advancing the deadline
            // (relevant for the mk2, whose sub-MCU drains slower than we feed).
            break;
        }

        emu->PostMIDI(midi_queue.front());
        midi_queue.pop_front();

        midi_byte_deadline = std::max(midi_byte_deadline, now) + samples_per_byte;
    }
}

// Case-sensitive substring search over a byte buffer.
static bool buffer_contains(const uint8_t* hay, size_t hay_len, std::string_view needle)
{
    if (needle.empty() || needle.size() > hay_len) {
        return false;
    }
    for (size_t i = 0; i + needle.size() <= hay_len; ++i) {
        if (memcmp(&hay[i], needle.data(), needle.size()) == 0) {
            return true;
        }
    }
    return false;
}

// Case-insensitive (ASCII) substring search over a byte buffer.
static bool buffer_contains_ci(const uint8_t* hay, size_t hay_len, std::string_view needle)
{
    if (needle.empty() || needle.size() > hay_len) {
        return false;
    }
    for (size_t i = 0; i + needle.size() <= hay_len; ++i) {
        size_t j = 0;
        for (; j < needle.size(); ++j) {
            const int a = std::tolower(hay[i + j]);
            const int b = std::tolower(static_cast<unsigned char>(needle[j]));
            if (a != b) {
                break;
            }
        }
        if (j == needle.size()) {
            return true;
        }
    }
    return false;
}

// D4: watch the firmware's LCD character RAM for the "MIDI Buff. Full!"
// overflow warning - the only direct signal that the firmware itself discarded
// received MIDI.
//
// The SC-55's dot-matrix panel is driven as a *character* device: the firmware
// can only write character codes to DD RAM (LCD_Data) and up to 8 custom
// glyphs to CG RAM - there is no pixel-addressable path. lcd.cpp renders each
// cell via lcd_font[ch - 16] for ch >= 16 (the standard ASCII font; 0x20 is
// blank, 0x42 is 'B') or CG RAM for ch < 16. The warning is 16 chars of plain
// text that cannot fit CG RAM's 8 glyphs, so it must use the font path and
// therefore lands in LCD_Data verbatim as ASCII. DD RAM maps linearly to
// LCD_Data, so the text is always a contiguous substring.
//
// We scan the whole 80-byte buffer (not just the top text line) and fire on
// either the exact warning text or a looser case-insensitive match (contains
// "buff" and " full"), so formatting differences across firmware revisions or
// models still trip it. A false match is a harmless extra log line; a miss
// would defeat the diagnostic. std::size() keeps the scan covering the full
// buffer even if the member's type is ever changed (a decayed pointer would
// not compile).
//
// LCD_Data is written only from MCU_Step(), i.e. earlier in this same
// Process() call on the audio thread, so this read needs no lock.
//
// Edge-triggered (log on the absent -> present transition) and additionally
// rate-limited, so a firmware repaint that briefly clears then rewrites the
// line cannot flood the log.
void NukedSc55::CheckBuffFull()
{
    const auto& lcd_data = emu->GetLCD().LCD_Data;
    const size_t n       = std::size(lcd_data);

    const bool exact = buffer_contains(lcd_data, n, "MIDI Buff. Full!");
    const bool loose = buffer_contains_ci(lcd_data, n, "buff") &&
                       buffer_contains_ci(lcd_data, n, " full");
    const bool found = exact || loose;

    if (found && !buff_full_seen) {
        buff_full_seen = true;
        static uint64_t d4_count = 0;
        if (rate_limited(d4_count)) {
            diag_logf("D4: firmware LCD shows overflow warning [%s] - firmware discarded MIDI data",
                      exact ? "exact" : "loose");
        }
    } else if (!found) {
        buff_full_seen = false;
    }
}

void NukedSc55::RenderAudio(const uint32_t num_frames)
{
    const auto start_size = render_buf[0].size();

    log("RenderAudio: num_frames: %d, start_size: %d", num_frames, start_size);

    while (render_buf[0].size() - start_size < num_frames) {
        FeedQueuedMidi();
        MCU_Step(emu->GetMCU());
    }
    FeedQueuedMidi();

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
