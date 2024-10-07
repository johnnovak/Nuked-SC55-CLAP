#include <cassert>

#include "nuked_sc55.h"

//////////////////////////////////////////////////////////////////////////////
// Plugin descriptors
//////////////////////////////////////////////////////////////////////////////

// Number of plugins in this dynamic library
constexpr auto NumPlugins = 4;

constexpr auto Vendor  = "John Novak";
constexpr auto Url     = "https://github.com/johnnovak/Nuked-SC55-CLAP";
constexpr auto Version = "0.1.0";

constexpr auto Features = (const char*[]){CLAP_PLUGIN_FEATURE_INSTRUMENT,
                                          CLAP_PLUGIN_FEATURE_SYNTHESIZER,
                                          CLAP_PLUGIN_FEATURE_STEREO,
                                          nullptr};

static const clap_plugin_descriptor_t plugin_descriptor_sc55_v1_20 = {
    .clap_version = CLAP_VERSION_INIT,
    .id           = "net.johnnovak.nuked_sc55.sc55_v1_20",
    .name         = "Nuked SC-55 — Roland SC-55 v1.20",
    .vendor       = Vendor,
    .url          = Url,
    .manual_url   = Url,
    .support_url  = Url,
    .version      = Version,
    .description  = "Roland SC-55 v1.20 MIDI sound module emulation",
    .features     = Features};

static const clap_plugin_descriptor_t plugin_descriptor_sc55_v1_21 = {
    .clap_version = CLAP_VERSION_INIT,
    .id           = "net.johnnovak.nuked_sc55.sc55_v1_21",
    .name         = "Nuked SC-55 — Roland SC-55 v1.21",
    .vendor       = Vendor,
    .url          = Url,
    .manual_url   = Url,
    .support_url  = Url,
    .version      = Version,
    .description  = "Roland SC-55 v1.21 MIDI sound module emulation",
    .features     = Features};

static const clap_plugin_descriptor_t plugin_descriptor_sc55_v2_00 = {
    .clap_version = CLAP_VERSION_INIT,
    .id           = "net.johnnovak.nuked_sc55.sc55_v2_00",
    .name         = "Nuked SC-55 — Roland SC-55 v2.00",
    .vendor       = Vendor,
    .url          = Url,
    .manual_url   = Url,
    .support_url  = Url,
    .version      = Version,
    .description  = "Roland SC-55 v2.00 MIDI sound module emulation",
    .features     = Features};

static const clap_plugin_descriptor_t plugin_descriptor_sc55mk2_v1_01 = {
    .clap_version = CLAP_VERSION_INIT,
    .id           = "net.johnnovak.nuked_sc55.sc55mk2_v1_01",
    .name         = "Nuked SC-55 — Roland SC-55mk2 v1.01",
    .vendor       = Vendor,
    .url          = Url,
    .manual_url   = Url,
    .support_url  = Url,
    .version      = Version,
    .description  = "Roland SC-55mk2 v1.01 MIDI sound module emulation",
    .features     = Features};

//////////////////////////////////////////////////////////////////////////////
// Extensions
//////////////////////////////////////////////////////////////////////////////

static const clap_plugin_note_ports_t extension_note_ports = {
    .count = [](const clap_plugin_t* plugin, bool is_input) -> uint32_t {
        return is_input ? 1 : 0;
    },

    .get = [](const clap_plugin_t* plugin, uint32_t index, bool is_input,
              clap_note_port_info_t* info) -> bool {
        if (!is_input || index) {
            return false;
        }

        info->id = 0;

        // We don't support CLAP_NOTE_DIALECT_CLAP because we want to force
        // the sending of RAW MIDI messages at all times.
        info->supported_dialects = CLAP_NOTE_DIALECT_MIDI;
        info->preferred_dialect  = CLAP_NOTE_DIALECT_MIDI;

        snprintf(info->name, sizeof(info->name), "%s", "Note Port");

        return true;
    }};

static const clap_plugin_audio_ports_t extension_audio_ports = {
    .count = [](const clap_plugin_t* plugin, bool is_input) -> uint32_t {
        return is_input ? 0 : 1;
    },

    .get = [](const clap_plugin_t* plugin, uint32_t index, bool is_input,
              clap_audio_port_info_t* info) -> bool {
        if (is_input || index) {
            return false;
        }

        info->id            = 0;
        info->channel_count = 2; // stereo
        info->flags         = CLAP_AUDIO_PORT_IS_MAIN;
        info->port_type     = CLAP_PORT_STEREO;
        info->in_place_pair = CLAP_INVALID_ID;

        snprintf(info->name, sizeof(info->name), "%s", "Audio Output");

        return true;
    }};

static const clap_plugin_state_t extension_state = {
    .save = [](const clap_plugin_t* plugin, const clap_ostream_t* stream) -> bool {
        auto the_plugin = (NukedSc55*)plugin->plugin_data;
        return the_plugin->SaveState(stream);
    },

    .load = [](const clap_plugin_t* plugin, const clap_istream_t* stream) -> bool {
        auto the_plugin = (NukedSc55*)plugin->plugin_data;
        return the_plugin->LoadState(stream);
    }};

//////////////////////////////////////////////////////////////////////////////
// Plugin classes
//////////////////////////////////////////////////////////////////////////////

static const void* get_extension(const clap_plugin* plugin, const char* id)
{
    if (strcmp(id, CLAP_EXT_NOTE_PORTS) == 0) {
        return &extension_note_ports;

    } else if (strcmp(id, CLAP_EXT_AUDIO_PORTS) == 0) {
        return &extension_audio_ports;

    } else if (strcmp(id, CLAP_EXT_STATE) == 0) {
        return &extension_state;

    } else {
        return nullptr;
    }
}

//----------------------------------------------------------------------------
// SC-55 v1.20
//----------------------------------------------------------------------------
static const clap_plugin_t my_plugin_class_sc55_v1_20 = {

    .desc = &plugin_descriptor_sc55_v1_20,

    .plugin_data = nullptr,

    .init = [](const clap_plugin* plugin) -> bool {
        auto the_plugin = (NukedSc55*)plugin->plugin_data;
        return the_plugin->Init(plugin);
    },

    .destroy =
        [](const clap_plugin* plugin) {
            auto the_plugin = (NukedSc55*)plugin->plugin_data;
            the_plugin->Shutdown();
            delete the_plugin;
        },

    .activate = [](const clap_plugin* plugin, double sample_rate,
                   uint32_t min_frame_count, uint32_t max_frame_count) -> bool {
        auto the_plugin = (NukedSc55*)plugin->plugin_data;
        return the_plugin->Activate(sample_rate, min_frame_count, max_frame_count);
    },

    .deactivate = [](const clap_plugin* plugin) {},

    .start_processing = [](const clap_plugin* plugin) -> bool { return true; },

    .stop_processing = [](const clap_plugin* plugin) {},

    .reset = [](const clap_plugin* plugin) {},

    .process = [](const clap_plugin* plugin,
                  const clap_process_t* process) -> clap_process_status {
        auto the_plugin = (NukedSc55*)plugin->plugin_data;
        return the_plugin->Process(process);
    },

    .get_extension = [](const clap_plugin* plugin, const char* id) -> const void* {
        return get_extension(plugin, id);
    },

    .on_main_thread = [](const clap_plugin* plugin) {}};

//----------------------------------------------------------------------------
// SC-55 v1.21
//----------------------------------------------------------------------------
static const clap_plugin_t my_plugin_class_sc55_v1_21 = {

    .desc = &plugin_descriptor_sc55_v1_21,

    .plugin_data = nullptr,

    .init = [](const clap_plugin* plugin) -> bool {
        auto the_plugin = (NukedSc55*)plugin->plugin_data;
        return the_plugin->Init(plugin);
    },

    .destroy =
        [](const clap_plugin* plugin) {
            auto the_plugin = (NukedSc55*)plugin->plugin_data;
            the_plugin->Shutdown();
            delete the_plugin;
        },

    .activate = [](const clap_plugin* plugin, double sample_rate,
                   uint32_t min_frame_count, uint32_t max_frame_count) -> bool {
        auto the_plugin = (NukedSc55*)plugin->plugin_data;
        return the_plugin->Activate(sample_rate, min_frame_count, max_frame_count);
    },

    .deactivate = [](const clap_plugin* plugin) {},

    .start_processing = [](const clap_plugin* plugin) -> bool { return true; },

    .stop_processing = [](const clap_plugin* plugin) {},

    .reset = [](const clap_plugin* plugin) {},

    .process = [](const clap_plugin* plugin,
                  const clap_process_t* process) -> clap_process_status {
        auto the_plugin = (NukedSc55*)plugin->plugin_data;
        return the_plugin->Process(process);
    },

    .get_extension = [](const clap_plugin* plugin, const char* id) -> const void* {
        return get_extension(plugin, id);
    },

    .on_main_thread = [](const clap_plugin* plugin) {}};

//----------------------------------------------------------------------------
// SC-55 v2.00
//----------------------------------------------------------------------------
static const clap_plugin_t my_plugin_class_sc55_v2_00 = {

    .desc = &plugin_descriptor_sc55_v2_00,

    .plugin_data = nullptr,

    .init = [](const clap_plugin* plugin) -> bool {
        auto the_plugin = (NukedSc55*)plugin->plugin_data;
        return the_plugin->Init(plugin);
    },

    .destroy =
        [](const clap_plugin* plugin) {
            auto the_plugin = (NukedSc55*)plugin->plugin_data;
            the_plugin->Shutdown();
            delete the_plugin;
        },

    .activate = [](const clap_plugin* plugin, double sample_rate,
                   uint32_t min_frame_count, uint32_t max_frame_count) -> bool {
        auto the_plugin = (NukedSc55*)plugin->plugin_data;
        return the_plugin->Activate(sample_rate, min_frame_count, max_frame_count);
    },

    .deactivate = [](const clap_plugin* plugin) {},

    .start_processing = [](const clap_plugin* plugin) -> bool { return true; },

    .stop_processing = [](const clap_plugin* plugin) {},

    .reset = [](const clap_plugin* plugin) {},

    .process = [](const clap_plugin* plugin,
                  const clap_process_t* process) -> clap_process_status {
        auto the_plugin = (NukedSc55*)plugin->plugin_data;
        return the_plugin->Process(process);
    },

    .get_extension = [](const clap_plugin* plugin, const char* id) -> const void* {
        return get_extension(plugin, id);
    },

    .on_main_thread = [](const clap_plugin* plugin) {}};

//----------------------------------------------------------------------------
// SC-55 mk2 v1.01
//----------------------------------------------------------------------------
static const clap_plugin_t my_plugin_class_sc55mk2_v1_01 = {

    .desc = &plugin_descriptor_sc55mk2_v1_01,

    .plugin_data = nullptr,

    .init = [](const clap_plugin* plugin) -> bool {
        auto the_plugin = (NukedSc55*)plugin->plugin_data;
        return the_plugin->Init(plugin);
    },

    .destroy =
        [](const clap_plugin* plugin) {
            auto the_plugin = (NukedSc55*)plugin->plugin_data;
            the_plugin->Shutdown();
            delete the_plugin;
        },

    .activate = [](const clap_plugin* plugin, double sample_rate,
                   uint32_t min_frame_count, uint32_t max_frame_count) -> bool {
        auto the_plugin = (NukedSc55*)plugin->plugin_data;
        return the_plugin->Activate(sample_rate, min_frame_count, max_frame_count);
    },

    .deactivate = [](const clap_plugin* plugin) {},

    .start_processing = [](const clap_plugin* plugin) -> bool { return true; },

    .stop_processing = [](const clap_plugin* plugin) {},

    .reset = [](const clap_plugin* plugin) {},

    .process = [](const clap_plugin* plugin,
                  const clap_process_t* process) -> clap_process_status {
        auto the_plugin = (NukedSc55*)plugin->plugin_data;
        return the_plugin->Process(process);
    },

    .get_extension = [](const clap_plugin* plugin, const char* id) -> const void* {
        return get_extension(plugin, id);
    },

    .on_main_thread = [](const clap_plugin* plugin) {}};

//////////////////////////////////////////////////////////////////////////////
// Plugin factory
//////////////////////////////////////////////////////////////////////////////

static const clap_plugin_factory_t plugin_factory = {

    .get_plugin_count = [](const clap_plugin_factory* factory) -> uint32_t {
        return NumPlugins;
    },

    .get_plugin_descriptor = [](const clap_plugin_factory* factory,
                                uint32_t index) -> const clap_plugin_descriptor_t* {
        if (index == 0) {
            return &plugin_descriptor_sc55_v1_20;

        } else if (index == 1) {
            return &plugin_descriptor_sc55_v1_21;

        } else if (index == 2) {
            return &plugin_descriptor_sc55_v2_00;

        } else if (index == 3) {
            return &plugin_descriptor_sc55mk2_v1_01;

        } else {
            return nullptr;
        }
    },

    .create_plugin = [](const clap_plugin_factory* factory, const clap_host_t* host,
                        const char* plugin_id) -> const clap_plugin_t* {
        if (!clap_version_is_compatible(host->clap_version)) {
            return nullptr;
        }

        NukedSc55* the_plugin = nullptr;

        if (strcmp(plugin_id, plugin_descriptor_sc55_v1_20.id) == 0) {
            the_plugin = new NukedSc55(my_plugin_class_sc55_v1_20,
                                       host,
                                       NukedSc55::Model::Sc55_v1_20);

        } else if (strcmp(plugin_id, plugin_descriptor_sc55_v1_21.id) == 0) {
            the_plugin = new NukedSc55(my_plugin_class_sc55_v1_21,
                                       host,
                                       NukedSc55::Model::Sc55_v1_21);

        } else if (strcmp(plugin_id, plugin_descriptor_sc55_v2_00.id) == 0) {
            the_plugin = new NukedSc55(my_plugin_class_sc55_v2_00,
                                       host,
                                       NukedSc55::Model::Sc55_v2_00);

        } else if (strcmp(plugin_id, plugin_descriptor_sc55mk2_v1_01.id) == 0) {
            the_plugin = new NukedSc55(my_plugin_class_sc55mk2_v1_01,
                                       host,
                                       NukedSc55::Model::Sc55mk2_v1_01);
        } else {
            return nullptr;
        }

        return the_plugin->GetPluginClass();
    }};

//////////////////////////////////////////////////////////////////////////////
// Dynamic library definition
//////////////////////////////////////////////////////////////////////////////

const char* plugin_path = nullptr;

extern "C" const clap_plugin_entry_t clap_entry = {
    .clap_version = CLAP_VERSION_INIT,

    .init = [](const char* _plugin_path) -> bool {
        plugin_path = _plugin_path;
        return true;
    },

    .deinit = []() {},

    .get_factory = [](const char* factory_id) -> const void* {
        return strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID) ? nullptr : &plugin_factory;
    }};
