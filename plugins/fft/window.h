#pragma once
#include <array>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <numbers>

template <std::floating_point T, std::size_t N, T overlap_factor>
consteval auto generate_hann_window()
{
    std::array<T, N> result;
    for (auto n = 0uz; n < N; ++n) {
        const T window = 0.5f * (1 - std::cos(2 * std::numbers::pi_v<T> * static_cast<T>(n) / static_cast<T>(N - 1)));
        result[n] = window / overlap_factor;
    }

    return result;
}
