#include "plugin.h"
#include "clap/clap.h"
#include "clap/ext/params.h"
#include "clap/id.h"
#include "clap/plugin-features.h"
#include "pffft.h"
#include "window.h"
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <print>
#include <sys/types.h>

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

#define DEFAULT_SR 48000

constexpr size_t gmodulo_mask = gbuffer_size - 1;

MyPlugin::MyPlugin()
    : i_idx(0)
    , o_idx(0)
    , hop_counter(0)
    , fft_buf((float*)pffft_aligned_malloc(pfft_buf_size))
    , pfft_setup(pffft_new_setup(gfft_size, PFFFT_REAL))
{
}

MyPlugin::~MyPlugin()
{
    pffft_aligned_free(fft_buf);
    pffft_destroy_setup(pfft_setup);
}

static void PluginProcessEvent(MyPlugin* plugin,
    const clap_event_header_t* event)
{
    if (event->type == CLAP_EVENT_PARAM_VALUE) {
        const clap_event_param_value_t* valueEvent = (const clap_event_param_value_t*)event;
        uint32_t i = (uint32_t)valueEvent->param_id;
        // MutexAcquire(plugin->syncParameters);
        plugin->parameters[i] = valueEvent->value;
        plugin->changed[i] = true;
        // MutexRelease(plugin->syncParameters);
    }
}

void MyPlugin::process_fft(const float* input, uint32_t istart, float* output,
    uint32_t ostart)
{
    constexpr auto window = generate_hann_window<float, gfft_size, overlap_factor>();

    uint32_t idx = (istart + gbuffer_size - gfft_size) & (gbuffer_size - 1);
    for (int i = 0; i < gfft_size; i++) {
        fft_buf[i] = input[idx++] * window[i];
        idx &= (gbuffer_size - 1);
    }

    pffft_transform_ordered(pfft_setup, fft_buf, fft_buf, NULL, PFFFT_FORWARD);
    // TODO: something
    pffft_transform_ordered(pfft_setup, fft_buf, fft_buf, NULL, PFFFT_BACKWARD);

    idx = ostart;
    for (int i = 0; i < gfft_size; i++) {
        output[idx++] += window[i] * fft_buf[i] / gfft_size;
        idx &= (gbuffer_size - 1);
    }
}

static void PluginProcessAudio(MyPlugin* plugin, uint32_t start, uint32_t end,
    const float* inputL, const float* inputR,
    float* outputL, float* outputR)
{
    // Add this check in PluginProcessAudio
    if (plugin->hop_counter >= ghop_size) {
        printf("Hop counter overflow: %u\n", plugin->hop_counter);
    }
    static uint32_t last_end = 0;
    if (start != last_end) {
        printf("Discontinuity in processing: last_end=%u, start=%u\n", last_end, start);
        assert(false);
    }
    printf("Processing chunk: start=%u, end=%u, size=%u\n", start, end, end - start);
    last_end = end;

    for (auto i = start; i < end; ++i) {
        const float inL = *inputL++;
        const float inR = *inputR++;

        // Store input in the circular buffer
        const auto i_idx = plugin->i_idx++;
        plugin->ibufL[i_idx] = inL;
        plugin->ibufR[i_idx] = inR;
        plugin->i_idx &= (gbuffer_size - 1);

        // read output  sample
        const auto o_idx = plugin->o_idx++;
        float outL = plugin->obufL[o_idx];
        float outR = plugin->obufR[o_idx];
        // scale output down by the overlap factor
        // NOTE: done by the window

        // clear ouutput sample in the buffer for next overlap-add
        plugin->obufL[o_idx] = 0.f;
        plugin->obufR[o_idx] = 0.f;
        plugin->o_idx &= (gbuffer_size - 1);

        // increment the hop counter ad start a new fft if we've reached the hop
        // size
        if (++plugin->hop_counter == ghop_size) {
            plugin->hop_counter = 0;
            plugin->process_fft(plugin->ibufL, plugin->i_idx, plugin->obufL,
                plugin->o_idx);
            plugin->process_fft(plugin->ibufR, plugin->i_idx, plugin->obufR,
                plugin->o_idx);

            // update the output buffer index to start at the next hop
            plugin->o_idx += ghop_size;
            plugin->o_idx &= (gbuffer_size - 1);
        }

        outputL[i] = outL;
        outputR[i] = outR;
    }
}

static void PluginSyncMainToAudio(MyPlugin* plugin,
    const clap_output_events_t* out)
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
    .id = "me.FFTFX",
    .name = "FFTFX",
    .vendor = "MEMEME",
    .url = "https://o",
    .manual_url = "https://nakst.gitlab.io",
    .support_url = "https://nakst.gitlab.io",
    .version = "1.0.0",
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
    .count = [](const clap_plugin_t* plugin, bool isInput) -> uint32_t {
        return 1;
    },

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

    .get_value = [](const clap_plugin_t* _plugin, clap_id id,
                     double* value) -> bool {
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

            for (uint32_t eventIndex = 0; eventIndex < eventCount; eventIndex++) {
                PluginProcessEvent(plugin, in->get(in, eventIndex));
            }
        },
};

static const clap_plugin_state_t extensionState = {
    .save = [](const clap_plugin_t* _plugin,
                const clap_ostream_t* stream) -> bool {
        MyPlugin* plugin = (MyPlugin*)_plugin->plugin_data;
        PluginSyncAudioToMain(plugin);
        return sizeof(float) * P_COUNT == stream->write(stream, plugin->mainParameters, sizeof(float) * P_COUNT);
    },

    .load = [](const clap_plugin_t* _plugin,
                const clap_istream_t* stream) -> bool {
        MyPlugin* plugin = (MyPlugin*)_plugin->plugin_data;
        // MutexAcquire(plugin->syncParameters);
        bool success = sizeof(float) * P_COUNT == stream->read(stream, plugin->mainParameters, sizeof(float) * P_COUNT);
        for (uint32_t i = 0; i < P_COUNT; i++)
            plugin->mainChanged[i] = true;
        // MutexRelease(plugin->syncParameters);
        return success;
    },
};

static const clap_plugin_latency_t latency_extension = {
    .get = [](const clap_plugin_t* plugin) -> uint32_t { return gbuffer_size; },
};

static const clap_plugin_t pluginClass = {
    .desc = &pluginDescriptor,
    .plugin_data = nullptr,

    .init = [](const clap_plugin* _plugin) -> bool {
        MyPlugin* plugin = (MyPlugin*)_plugin->plugin_data;

        // MutexInitialise(plugin->syncParameters);

        for (uint32_t i = 0; i < P_COUNT; i++) {
            clap_param_info_t information = {};
            extensionParams.get_info(_plugin, i, &information);
            plugin->mainParameters[i] = plugin->parameters[i] = information.default_value;
        }

        return true;
    },

    .destroy =
        [](const clap_plugin* _plugin) {
            MyPlugin* plugin = (MyPlugin*)_plugin->plugin_data;
            // MutexDestroy(plugin->syncParameters);
            delete plugin;
        },

    .activate = [](const clap_plugin* _plugin, double sampleRate,
                    uint32_t minimumFramesCount,
                    uint32_t maximumFramesCount) -> bool {
        MyPlugin* plugin = (MyPlugin*)_plugin->plugin_data;
        plugin->sampleRate = sampleRate;
        return true;
    },

    .deactivate = [](const clap_plugin* _plugin) {},

    .start_processing = [](const clap_plugin* _plugin) -> bool {
        MyPlugin* plugin = (MyPlugin*)_plugin->plugin_data;
        plugin->process = true;
        return true;
    },

    .stop_processing = [](const clap_plugin* _plugin) {
        MyPlugin* plugin = (MyPlugin*)_plugin->plugin_data;
        plugin->process = false; },

    .reset =
        [](const clap_plugin* _plugin) {
            MyPlugin* plugin = (MyPlugin*)_plugin->plugin_data;
        },

    .process = [](const clap_plugin* _plugin,
                   const clap_process_t* process) -> clap_process_status {
        MyPlugin* plugin = (MyPlugin*)_plugin->plugin_data;

        assert(process->audio_outputs_count == 1);
        assert(process->audio_inputs_count == 1);

        if (!plugin->process)
            return CLAP_PROCESS_SLEEP;

        // At the start of your plugin's process function, verify buffer indices
        if (plugin->i_idx >= gbuffer_size || plugin->o_idx >= gbuffer_size) {
            printf("Buffer index error: i_idx=%u, o_idx=%u\n", plugin->i_idx, plugin->o_idx);
            assert(false);
        }

        const uint32_t frameCount = process->frames_count;
        const uint32_t inputEventCount = process->in_events->size(process->in_events);
        uint32_t eventIndex = 0;
        uint32_t nextEventFrame = inputEventCount ? 0 : frameCount;

        PluginSyncMainToAudio(plugin, process->out_events);

        for (uint32_t i = 0; i < frameCount;) {
            while (eventIndex < inputEventCount && nextEventFrame == i) {
                const clap_event_header_t* event = process->in_events->get(process->in_events, eventIndex);

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

    .get_extension = [](const clap_plugin* plugin,
                         const char* id) -> const void* {
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
    .get_plugin_count = [](const clap_plugin_factory* factory) -> uint32_t {
        return 1;
    },

    .get_plugin_descriptor =
        [](const clap_plugin_factory* factory,
            uint32_t index) -> const clap_plugin_descriptor_t* {
        return index == 0 ? &pluginDescriptor : nullptr;
    },

    .create_plugin = [](const clap_plugin_factory* factory,
                         const clap_host_t* host,
                         const char* pluginID) -> const clap_plugin_t* {
        if (!clap_version_is_compatible(host->clap_version) || strcmp(pluginID, pluginDescriptor.id)) {
            return nullptr;
        }

        // MyPlugin *plugin = (MyPlugin *)calloc(1, sizeof(MyPlugin));
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
