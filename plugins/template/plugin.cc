#include "plugin.h"
#include "clap/clap.h"
#include "clap/ext/params.h"
#include "clap/id.h"
#include "clap/plugin-features.h"
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <print>

#ifdef _WIN32
#include <windows.h>
typedef HANDLE Mutex;
#define MutexAcquire(mutex) WaitForSingleObject(mutex, INFINITE)
#define MutexRelease(mutex) ReleaseMutex(mutex)
#define MutexInitialise(mutex) (mutex = CreateMutex(nullptr, FALSE, nullptr))
#define MutexDestroy(mutex) CloseHandle(mutex)
#else
#include <pthread.h>
typedef pthread_mutex_t Mutex;
#define MutexAcquire(mutex) pthread_mutex_lock(&(mutex))
#define MutexRelease(mutex) pthread_mutex_unlock(&(mutex))
#define MutexInitialise(mutex) pthread_mutex_init(&(mutex), nullptr)
#define MutexDestroy(mutex) pthread_mutex_destroy(&(mutex))
#endif

static void PluginProcessEvent(
    MyPlugin* plugin, const clap_event_header_t* event)
{
    if (event->type == CLAP_EVENT_PARAM_VALUE) {
        const clap_event_param_value_t* valueEvent
            = (const clap_event_param_value_t*)event;
        uint32_t i = (uint32_t)valueEvent->param_id;
        // MutexAcquire(plugin->syncParameters);
        plugin->parameters[i] = valueEvent->value;
        plugin->changed[i] = true;
        // MutexRelease(plugin->syncParameters);
    }
}

static void PluginProcessAudio(MyPlugin* plugin, uint32_t start, uint32_t end,
    const float* inputL, const float* inputR, float* outputL, float* outputR)
{

    for (auto i = start; i < end; ++i) {
        const float inL = *inputL++;
        const float inR = *inputR++;

        outputL[i] = inL;
        outputR[i] = inR;
    }
}

static void PluginSyncMainToAudio(
    MyPlugin* plugin, const clap_output_events_t* out)
{
    // MutexAcquire(plugin->syncParameters);

    for (uint32_t i = 0; i < P_COUNT; i++) {
        if (plugin->mainChanged[i]) {
            plugin->parameters[i] = plugin->mainParameters[i];
            plugin->mainChanged[i] = false;

            clap_event_param_value_t event = {};
            event.header.size = sizeof(event);
            event.header.time = 0;
            event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
            event.header.type = CLAP_EVENT_PARAM_VALUE;
            event.header.flags = 0;
            event.param_id = i;
            event.cookie = NULL;
            event.note_id = -1;
            event.port_index = -1;
            event.channel = -1;
            event.key = -1;
            event.value = plugin->parameters[i];
            out->try_push(out, &event.header);
        }
    }

    // MutexRelease(plugin->syncParameters);
}

static bool PluginSyncAudioToMain(MyPlugin* plugin)
{
    bool anyChanged = false;
    // MutexAcquire(plugin->syncParameters);

    for (uint32_t i = 0; i < P_COUNT; i++) {
        if (plugin->changed[i]) {
            plugin->mainParameters[i] = plugin->parameters[i];
            plugin->changed[i] = false;
            anyChanged = true;
        }
    }

    // MutexRelease(plugin->syncParameters);
    return anyChanged;
}

const clap_plugin_descriptor_t pluginDescriptor = {
    .clap_version = CLAP_VERSION_INIT,
    .id = "me.TEMPLATEFX",
    .name = "TEMPLATEFX",
    .vendor = "MEMEME",
    .url = "https://me.me",
    .manual_url = "https://me.me",
    .support_url = "https://me.me",
    .version = "0.0.0",
    .description = "The best audio plugin ever.",

    .features = (const char*[]) {
        CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
        CLAP_PLUGIN_FEATURE_STEREO,
        CLAP_PLUGIN_FEATURE_UTILITY,
        CLAP_PLUGIN_FEATURE_MIXING,
        NULL,
    },
};

static const clap_plugin_audio_ports_t extensionAudioPorts = {
    .count
    = [](const clap_plugin_t* plugin, bool isInput) -> uint32_t { return 1; },

    .get = [](const clap_plugin_t* plugin, uint32_t index, bool isInput,
               clap_audio_port_info_t* info) -> bool {
        if (!info)
            return false;

        // Define audio ports
        if (isInput) {
            if (index != 0) // Only one input port
                return false;

            info->id = 0; // Unique ID for this port
            info->flags = CLAP_AUDIO_PORT_IS_MAIN; // Main audio port
            info->channel_count = 2; // Stereo
            info->port_type = CLAP_PORT_STEREO; // Standard stereo port
            info->in_place_pair = CLAP_INVALID_ID;
            snprintf(info->name, sizeof(info->name), "%s", "Audio Input");
        } else {
            if (index != 0) // Only one output port
                return false;

            info->id = 1; // Unique ID for this port
            info->flags = CLAP_AUDIO_PORT_IS_MAIN; // Main audio port
            info->channel_count = 2; // Stereo
            info->port_type = CLAP_PORT_STEREO; // Standard stereo port
            info->in_place_pair = CLAP_INVALID_ID;
            snprintf(info->name, sizeof(info->name), "%s", "Audio Output");
        }

        return true;
    },
};

static const clap_plugin_params_t extensionParams = {
    .count = [](const clap_plugin_t* plugin) -> uint32_t { return P_COUNT; },

    .get_info = [](const clap_plugin_t* _plugin, uint32_t index,
                    clap_param_info_t* information) -> bool {
        switch (index) {
        default:
            return false;
        }
    },

    .get_value
    = [](const clap_plugin_t* _plugin, clap_id id, double* value) -> bool {
        MyPlugin* plugin = (MyPlugin*)_plugin->plugin_data;
        uint32_t i = (uint32_t)id;
        if (i >= P_COUNT)
            return false;
        // MutexAcquire(plugin->syncParameters);
        *value = plugin->mainChanged[i] ? plugin->mainParameters[i]
                                        : plugin->parameters[i];
        // MutexRelease(plugin->syncParameters);
        return true;
    },

    .value_to_text =
        [](const clap_plugin_t* _plugin, clap_id id, double value,
            char* display, uint32_t size) {
            uint32_t i = (uint32_t)id;
            if (i >= P_COUNT)
                return false;
            snprintf(display, size, "%f", value);
            return true;
        },

    .text_to_value =
        [](const clap_plugin_t* _plugin, clap_id param_id, const char* display,
            double* value) {
            *value = atof(display);
            return false;
        },

    .flush =
        [](const clap_plugin_t* _plugin, const clap_input_events_t* in,
            const clap_output_events_t* out) {
            MyPlugin* plugin = (MyPlugin*)_plugin->plugin_data;
            const uint32_t eventCount = in->size(in);
            PluginSyncMainToAudio(plugin, out);

            for (uint32_t eventIndex = 0; eventIndex < eventCount;
                eventIndex++) {
                PluginProcessEvent(plugin, in->get(in, eventIndex));
            }
        },
};

static const clap_plugin_state_t extensionState = {
    .save
    = [](const clap_plugin_t* _plugin, const clap_ostream_t* stream) -> bool {
        MyPlugin* plugin = (MyPlugin*)_plugin->plugin_data;
        PluginSyncAudioToMain(plugin);
        return sizeof(float) * P_COUNT
            == stream->write(
                stream, plugin->mainParameters, sizeof(float) * P_COUNT);
    },

    .load
    = [](const clap_plugin_t* _plugin, const clap_istream_t* stream) -> bool {
        MyPlugin* plugin = (MyPlugin*)_plugin->plugin_data;
        // MutexAcquire(plugin->syncParameters);
        bool success = sizeof(float) * P_COUNT
            == stream->read(
                stream, plugin->mainParameters, sizeof(float) * P_COUNT);
        for (uint32_t i = 0; i < P_COUNT; i++)
            plugin->mainChanged[i] = true;
        // MutexRelease(plugin->syncParameters);
        return success;
    },
};

// static const clap_plugin_latency_t latency_extension = {
//     .get = [](const clap_plugin_t *plugin) -> uint32_t { return 0; },
// };

static const clap_plugin_t pluginClass = {
    .desc = &pluginDescriptor,
    .plugin_data = nullptr,

    .init = [](const clap_plugin* _plugin) -> bool {
        MyPlugin* plugin = (MyPlugin*)_plugin->plugin_data;

        // MutexInitialise(plugin->syncParameters);

        for (uint32_t i = 0; i < P_COUNT; i++) {
            clap_param_info_t information = {};
            extensionParams.get_info(_plugin, i, &information);
            plugin->mainParameters[i] = plugin->parameters[i]
                = information.default_value;
        }

        return true;
    },

    .destroy =
        [](const clap_plugin* _plugin) {
            MyPlugin* plugin = (MyPlugin*)_plugin->plugin_data;
            // MutexDestroy(plugin->syncParameters);
            delete plugin;
        },

    .activate
    = [](const clap_plugin* _plugin, double sampleRate,
          uint32_t minimumFramesCount, uint32_t maximumFramesCount) -> bool {
        MyPlugin* plugin = (MyPlugin*)_plugin->plugin_data;
        plugin->sampleRate = sampleRate;
        return true;
    },

    .deactivate = [](const clap_plugin* _plugin) {},

    .start_processing = [](const clap_plugin* _plugin) -> bool { return true; },

    .stop_processing = [](const clap_plugin* _plugin) {},

    .reset =
        [](const clap_plugin* _plugin) {
            MyPlugin* plugin = (MyPlugin*)_plugin->plugin_data;
        },

    .process = [](const clap_plugin* _plugin,
                   const clap_process_t* process) -> clap_process_status {
        MyPlugin* plugin = (MyPlugin*)_plugin->plugin_data;

        assert(process->audio_outputs_count == 1);
        assert(process->audio_inputs_count == 1);

        const uint32_t frameCount = process->frames_count;
        const uint32_t inputEventCount
            = process->in_events->size(process->in_events);
        uint32_t eventIndex = 0;
        uint32_t nextEventFrame = inputEventCount ? 0 : frameCount;

        PluginSyncMainToAudio(plugin, process->out_events);

        for (uint32_t i = 0; i < frameCount;) {
            while (eventIndex < inputEventCount && nextEventFrame == i) {
                const clap_event_header_t* event
                    = process->in_events->get(process->in_events, eventIndex);

                if (event->time != i) {
                    nextEventFrame = event->time;
                    break;
                }

                PluginProcessEvent(plugin, event);
                eventIndex++;

                if (eventIndex == inputEventCount) {
                    nextEventFrame = frameCount;
                    break;
                }
            }

            PluginProcessAudio(plugin, i, nextEventFrame,
                process->audio_inputs[0].data32[0],
                process->audio_inputs[0].data32[1],
                process->audio_outputs[0].data32[0],
                process->audio_outputs[0].data32[1]);
            i = nextEventFrame;
        }
        return CLAP_PROCESS_CONTINUE;
    },

    .get_extension
    = [](const clap_plugin* plugin, const char* id) -> const void* {
        if (0 == strcmp(id, CLAP_EXT_AUDIO_PORTS))
            return &extensionAudioPorts;
        if (0 == strcmp(id, CLAP_EXT_PARAMS))
            return &extensionParams;
        if (0 == strcmp(id, CLAP_EXT_STATE))
            return &extensionState;
        // if (0 == strcmp(id, CLAP_EXT_LATENCY))
        //   return &latency_extension;
        return nullptr;
    },

    .on_main_thread = [](const clap_plugin* _plugin) {},
};

static const clap_plugin_factory_t pluginFactory = {
    .get_plugin_count
    = [](const clap_plugin_factory* factory) -> uint32_t { return 1; },

    .get_plugin_descriptor
    = [](const clap_plugin_factory* factory,
          uint32_t index) -> const clap_plugin_descriptor_t* {
        return index == 0 ? &pluginDescriptor : nullptr;
    },

    .create_plugin
    = [](const clap_plugin_factory* factory, const clap_host_t* host,
          const char* pluginID) -> const clap_plugin_t* {
        if (!clap_version_is_compatible(host->clap_version)
            || strcmp(pluginID, pluginDescriptor.id)) {
            return nullptr;
        }

        MyPlugin* plugin = new MyPlugin;
        plugin->host = host;
        plugin->plugin = pluginClass;
        plugin->plugin.plugin_data = plugin;
        return &plugin->plugin;
    },
};

extern "C" const clap_plugin_entry_t clap_entry = {
    .clap_version = CLAP_VERSION_INIT,

    .init = [](const char* path) -> bool { return true; },

    .deinit = []() {},

    .get_factory = [](const char* factoryID) -> const void* {
        return strcmp(factoryID, CLAP_PLUGIN_FACTORY_ID) ? nullptr
                                                         : &pluginFactory;
    },
};
