#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "nuked_sc55.h"

// #define DEBUG

//----------------------------------------------------------------------------
// Simple debug logging
#ifdef DEBUG

static FILE* logfile = nullptr;

static void log_init()
{
    logfile = fopen("/Users/jnovak/nuked-sc55-clap.log", "wb");
}

static void _log(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    vfprintf(logfile, fmt, args);
    fprintf(logfile, "\n");

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
    log("Plugin path: %s", plugin_path);

    plugin_class             = _plugin_class;
    plugin_class.plugin_data = this;

    host  = _host;
    model = _model;
}

const clap_plugin_t* NukedSc55::GetPluginClass()
{
    return &plugin_class;
}

bool NukedSc55::Init(const clap_plugin* _plugin_instance)
{
    log("Init");

    plugin_instance = _plugin_instance;
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

    emu->PublishSample(out.left, out.right);
}

// TODO mk1 sample rate 32000 Hz
// TODO mk2 sample rate 33103 Hz

bool NukedSc55::Activate(const double sample_rate, const uint32_t min_frame_count,
                         const uint32_t max_frame_count)
{
    log("Activate");

    emu = std::make_unique<Emulator>();

    const EMU_Options opts = {.enable_lcd = false};
    if (!emu->Init(opts)) {
        log("emu->Init failed");
        emu.reset(nullptr);
        return false;
    }

    // TODO get path to plugin
    // https://forums.steinberg.net/t/get-path-to-resources-folder/828223/4

    const std::filesystem::path base_rom_dir = "/Users/jnovak/Library/Preferences/DOSBox/sc55-roms/";

    auto romset  = Romset::MK1;
    auto rom_dir = base_rom_dir;

    switch (model) {
    case Model::Sc55_v1_20: rom_dir += "sc55-v1.20"; break;
    case Model::Sc55_v1_21: rom_dir += "sc55-v1.21"; break;
    case Model::Sc55_v2_00: rom_dir += "sc55-v2.00"; break;
    case Model::Sc55_mk2_v1_01:
        romset = Romset::MK2;
        rom_dir += "sc55-mk2-v1.01";
        break;
    default: assert(false);
    }

    if (!emu->LoadRoms(romset, rom_dir)) {
        log("emu->LoadRoms failed");
        emu.reset(nullptr);
        return false;
    }

    emu->Reset();
    emu->GetPCM().disable_oversampling = true;
    emu->PostSystemReset(EMU_SystemReset::GS_RESET);

    // Speed up the devices' bootup delay
    const size_t num_steps = (model == Model::Sc55_mk2_v1_01) ? 9'500'000 : 700'000;

    for (size_t i = 0; i < num_steps; i++) {
        MCU_Step(emu->GetMCU());
    }

    emu->SetSampleCallback(receive_sample, this);

    render_sample_rate_hz = PCM_GetOutputFrequency(emu->GetPCM());

    log("render_sample_rate_hz: %g", render_sample_rate_hz);

    if (output_sample_rate_hz != render_sample_rate_hz) {
        do_resample = true;

        output_sample_rate_hz = sample_rate;

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

        render_buf[0].resize(max_render_buf_size);
        render_buf[1].resize(max_render_buf_size);

    } else {
        do_resample = false;

        output_sample_rate_hz = render_sample_rate_hz;
        resample_ratio        = 1.0;

        render_buf[0].resize(max_frame_count);
        render_buf[1].resize(max_frame_count);
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

void NukedSc55::PublishSample(const float left, const float right)
{
    render_buf[0].emplace_back(left);
    render_buf[1].emplace_back(right);
}

void NukedSc55::ProcessEvent(const clap_event_header_t* event)
{
    if (event->space_id == CLAP_CORE_EVENT_SPACE_ID) {

        switch (event->type) {

            // TODO probably best to get rid of CLAP_EVEN_NOTE_* handling and
            // only deal with MIDI messages
            /*
        case CLAP_EVENT_NOTE_ON:
        case CLAP_EVENT_NOTE_OFF: {
            log("CLAP_EVENT_NOTE_*");
            // "Note On" and "Note Off" MIDI events can be sent either as
            // CLAP_EVENT_NOTE_* or raw CLAP_EVENT_MIDI messages.
            //
            // The same event must not be sent twice; it is forbidden for
            // hosts to send the same note event encoded as both
            // CLAP_EVENT_NOTE_* and CLAP_EVENT_MIDI messages.
            //
            // The official advice is that hosts should prefer
            // CLAP_EVENT_NOTE_* messages, so we need to handle both.
            //
            const auto note_event = reinterpret_cast<const
        clap_event_note_t*>(event);

            if (note_event->port_index == -1 || note_event->channel == -1 ||
                note_event->key == -1) {
                break;
            }

            const auto status = static_cast<uint8_t>(
                (CLAP_EVENT_NOTE_OFF ? 0x80 : 0x90) + note_event->channel);

            const auto data1 = static_cast<uint8_t>(note_event->key);
            const auto data2 = static_cast<uint8_t>(note_event->velocity *
        127.0);

            emu->PostMIDI(status);
            emu->PostMIDI(data1);
            emu->PostMIDI(data2);
        } break;
*/

        case CLAP_EVENT_MIDI: {
            const auto midi_event = reinterpret_cast<const clap_event_midi_t*>(event);

            emu->PostMIDI(midi_event->data[0]);
            emu->PostMIDI(midi_event->data[1]);

            // 3-byte messages
            switch (const auto status = midi_event->data[0] & 0xf0) {
            case 0x80: // note off
            case 0x90: // note on
            case 0xa0: // poly aftertouch
            case 0xb0: // control change
            case 0xe0: // pitch wheel
                emu->PostMIDI(midi_event->data[2]);
                log("CLAP_EVENT_MIDI: %02x %02x %02x",
                    midi_event->data[0],
                    midi_event->data[1],
                    midi_event->data[2]);
                break;
            default:
                log("CLAP_EVENT_MIDI: %02x %02x",
                    midi_event->data[0],
                    midi_event->data[1]);
            }
        } break;

        case CLAP_EVENT_MIDI_SYSEX: {
            const auto sysex_event = reinterpret_cast<const clap_event_midi_sysex*>(
                event);

            emu->PostMIDI(std::span{sysex_event->buffer, sysex_event->size});

            log("CLAP_EVENT_MIDI_SYSEX, length: %d", sysex_event->size);
        } break;
        }
    }
}

void NukedSc55::RenderAudio(const uint32_t num_frames)
{
    log("RenderAudio: num_frames: %d", num_frames);

    const auto start_size = render_buf[0].size();

    while (render_buf[0].size() - start_size < num_frames) {
        MCU_Step(emu->GetMCU());
    }
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
