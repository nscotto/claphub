#pragma once
#include "clap/clap.h"
#include "pffft.h"
#include "window.h"
#include <array>
#include <cstdint>

// Parameters.
#define P_COUNT (0)

constexpr size_t gfft_size = 1024;
constexpr size_t pfft_buf_size = gfft_size * 4;
constexpr size_t ghop_size = gfft_size / 2;
constexpr float overlap_factor = 2.f;
consteval auto generate_window = [] { return generate_hann_window<float, gfft_size, overlap_factor>(); };
constexpr size_t gbuffer_size = gfft_size * 16;

struct MyPlugin {
    clap_plugin_t plugin;
    const clap_host_t* host;

    float sampleRate;
    float parameters[P_COUNT], mainParameters[P_COUNT];
    bool changed[P_COUNT], mainChanged[P_COUNT];

    MyPlugin();
    ~MyPlugin();

    bool process;
    // DSP data
    float ibufL[gbuffer_size];
    float obufL[gbuffer_size];
    float ibufR[gbuffer_size];
    float obufR[gbuffer_size];
    uint32_t i_idx;
    uint32_t o_idx;
    uint32_t hop_counter;

    float* fft_buf;

    void process_fft(const float* input, uint32_t istart, float* output,
        uint32_t iend);

    PFFFT_Setup* pfft_setup;
};

extern const clap_plugin_descriptor_t pluginDescriptor;

extern const clap_plugin_posix_fd_support_t extensionPOSIXFDSupport;
