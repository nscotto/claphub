#pragma once
#include "clap/clap.h"

// Parameters.
#define P_COUNT (0)

struct MyPlugin {
    clap_plugin_t plugin;
    const clap_host_t* host;

    float sampleRate;
    float parameters[P_COUNT], mainParameters[P_COUNT];
    bool changed[P_COUNT], mainChanged[P_COUNT];
};
