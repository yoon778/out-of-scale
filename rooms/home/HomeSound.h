#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace home_sound {
namespace detail {

inline constexpr std::uint32_t sampleRate = 22050;
inline constexpr std::size_t sampleCount = sampleRate / 10;
using DoorFitWave = std::array<unsigned char, 44 + sampleCount * 2>;

inline DoorFitWave makeDoorFitWave() {
    DoorFitWave wave{};
    const auto tag = [&](std::size_t offset, const char* text) {
        for (std::size_t index = 0; index < 4; ++index)
            wave[offset + index] = static_cast<unsigned char>(text[index]);
    };
    const auto word = [&](std::size_t offset, std::uint16_t value) {
        wave[offset] = static_cast<unsigned char>(value & 0xffu);
        wave[offset + 1] = static_cast<unsigned char>(value >> 8);
    };
    const auto dword = [&](std::size_t offset, std::uint32_t value) {
        for (std::size_t index = 0; index < 4; ++index)
            wave[offset + index] = static_cast<unsigned char>((value >> (index * 8)) & 0xffu);
    };

    tag(0, "RIFF");
    dword(4, static_cast<std::uint32_t>(wave.size() - 8));
    tag(8, "WAVE");
    tag(12, "fmt ");
    dword(16, 16);
    word(20, 1);  // PCM
    word(22, 1);  // Mono
    dword(24, sampleRate);
    dword(28, sampleRate * 2);
    word(32, 2);
    word(34, 16);
    tag(36, "data");
    dword(40, static_cast<std::uint32_t>(sampleCount * 2));

    // Original short wooden knock with a brass latch transient.
    constexpr double tau = 6.283185307179586;
    std::uint32_t noiseState = 0x64b39a17u;
    for (std::size_t index = 0; index < sampleCount; ++index) {
        noiseState ^= noiseState << 13;
        noiseState ^= noiseState >> 17;
        noiseState ^= noiseState << 5;
        const double noise = static_cast<double>(noiseState & 0xffffu) / 32767.5 - 1.0;
        const double time = static_cast<double>(index) / sampleRate;
        const double attack = 1.0 - std::exp(-time / 0.0004);
        const double wood = 0.45 * std::sin(tau * 430.0 * time) * std::exp(-time / 0.012);
        const double brass = (0.19 * std::sin(tau * 2650.0 * time) +
                              0.09 * std::sin(tau * 4170.0 * time)) * std::exp(-time / 0.018);
        const double transient = noise * 0.28 * std::exp(-time / 0.003);
        const double fade = static_cast<double>(sampleCount - 1 - index) / sampleCount;
        const auto sample = static_cast<std::int16_t>(10000.0 * attack * (wood + brass + transient) * fade);
        word(44 + index * 2, static_cast<std::uint16_t>(sample));
    }
    return wave;
}

}  // namespace detail

}  // namespace home_sound
