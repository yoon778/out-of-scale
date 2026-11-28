#include "Audio.h"

#include "../../../../rooms/home/HomeSound.h"

#include <windows.h>
#include <xaudio2.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "ole32.lib")

namespace {
constexpr float musicGain = 0.27f;
constexpr float crossfadeSeconds = 1.0f;
constexpr float duckGain = 0.45f;
constexpr float duckHoldSeconds = 0.12f;
constexpr float duckRestoreSeconds = 0.35f;
constexpr float effectGain = 0.85f;
constexpr float mechanismGain = 0.85f;
constexpr float mechanismAttackSeconds = 0.04f;
constexpr float mechanismReleaseSeconds = 0.06f;
constexpr float mechanismDuckGain = 0.75f;
constexpr float mechanismDuckRestoreSeconds = 0.18f;
constexpr std::size_t maximumWaveBytes = 64u * 1024u * 1024u;
constexpr std::array musicFiles{L"school.wav", L"military.wav", L"office.wav", L"home.wav", L"finale.wav"};

struct WaveData {
    WAVEFORMATEX format{};
    std::vector<BYTE> samples;
};

std::uint32_t littleEndian(std::span<const BYTE> bytes, std::size_t offset, unsigned count) {
    std::uint32_t value = 0;
    for (unsigned index = 0; index < count; ++index)
        value |= static_cast<std::uint32_t>(bytes[offset + index]) << (index * 8);
    return value;
}

bool chunkIs(std::span<const BYTE> bytes, std::size_t offset, const char* id) {
    return std::memcmp(bytes.data() + offset, id, 4) == 0;
}

std::optional<WaveData> parseWave(std::span<const BYTE> bytes, bool music) {
    if (bytes.size() < 12 || bytes.size() > maximumWaveBytes ||
        !chunkIs(bytes, 0, "RIFF") || !chunkIs(bytes, 8, "WAVE")) return std::nullopt;
    const std::size_t declaredSize = littleEndian(bytes, 4, 4);
    if (declaredSize < 4 || declaredSize > bytes.size() - 8) return std::nullopt;
    const std::size_t end = declaredSize + 8;
    WaveData wave;
    bool foundFormat = false;
    bool foundSamples = false;
    std::size_t dataOffset = 0;
    std::size_t dataBytes = 0;
    for (std::size_t cursor = 12; cursor < end;) {
        if (end - cursor < 8) return std::nullopt;
        const std::size_t length = littleEndian(bytes, cursor + 4, 4);
        const std::size_t payload = cursor + 8;
        if (length > end - payload) return std::nullopt;
        const std::size_t padded = length + (length & 1u);
        if (padded > end - payload) return std::nullopt;
        if (chunkIs(bytes, cursor, "fmt ")) {
            if (foundFormat || length < 16 || littleEndian(bytes, payload, 2) != WAVE_FORMAT_PCM)
                return std::nullopt;
            wave.format.wFormatTag = WAVE_FORMAT_PCM;
            wave.format.nChannels = static_cast<WORD>(littleEndian(bytes, payload + 2, 2));
            wave.format.nSamplesPerSec = littleEndian(bytes, payload + 4, 4);
            wave.format.nAvgBytesPerSec = littleEndian(bytes, payload + 8, 4);
            wave.format.nBlockAlign = static_cast<WORD>(littleEndian(bytes, payload + 12, 2));
            wave.format.wBitsPerSample = static_cast<WORD>(littleEndian(bytes, payload + 14, 2));
            foundFormat = true;
        } else if (chunkIs(bytes, cursor, "data")) {
            if (foundSamples || length == 0) return std::nullopt;
            dataOffset = payload;
            dataBytes = length;
            foundSamples = true;
        }
        cursor = payload + padded;
    }
    const auto& format = wave.format;
    const DWORD minimumRate = music ? 24000u : 22050u;
    if (!foundFormat || !foundSamples || format.nChannels < 1 || format.nChannels > 2 ||
        format.wBitsPerSample != 16 || format.nSamplesPerSec < minimumRate || format.nSamplesPerSec > 48000 ||
        format.nBlockAlign != format.nChannels * 2 ||
        format.nAvgBytesPerSec != format.nSamplesPerSec * format.nBlockAlign ||
        dataBytes % format.nBlockAlign != 0) return std::nullopt;
    wave.samples.assign(bytes.begin() + dataOffset, bytes.begin() + dataOffset + dataBytes);
    return wave;
}

std::filesystem::path musicDirectory() {
    std::vector<wchar_t> path(512);
    while (path.size() <= 32768) {
        const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (length == 0) return {};
        if (length < path.size())
            return std::filesystem::path{std::wstring{path.data(), length}}.parent_path() / L"assets/audio/music";
        path.resize(path.size() * 2);
    }
    return {};
}

std::optional<WaveData> readWave(const std::filesystem::path& path, bool music = true) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    const auto length = file.tellg();
    if (!file || length < 12 || length > static_cast<std::streamoff>(maximumWaveBytes)) return std::nullopt;
    std::vector<BYTE> bytes(static_cast<std::size_t>(length));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
        return std::nullopt;
    return parseWave(bytes, music);
}

int musicIndex(RoomMusic room) {
    const int value = static_cast<int>(room);
    return value >= 1 && value <= static_cast<int>(musicFiles.size()) ? value - 1 : -1;
}

void destroySource(IXAudio2SourceVoice*& voice) {
    if (!voice) return;
    voice->Stop();
    voice->FlushSourceBuffers();
    voice->DestroyVoice();
    voice = nullptr;
}
}

struct Audio::State {
    struct Music {
        std::optional<WaveData> wave;
        IXAudio2SourceVoice* voice = nullptr;
        float weight = 0.0f;
        float fromWeight = 0.0f;
        float sceneGain = 1.0f;
    };

    struct Effect {
        std::optional<WaveData> wave;
        IXAudio2SourceVoice* voice = nullptr;
    };

    struct Mechanism : Effect {
        float gain = 0.0f;
        float target = 0.0f;
        float fromGain = 0.0f;
        float elapsed = 0.0f;
    };

    IXAudio2* engine = nullptr;
    IXAudio2MasteringVoice* master = nullptr;
    IXAudio2SourceVoice* clickVoice = nullptr;
    std::optional<WaveData> click;
    std::array<Music, musicFiles.size()> tracks;
    std::array<Effect, 4> effects;
    std::array<Mechanism, 2> mechanisms;
    int selected = -1;
    float fadeElapsed = crossfadeSeconds;
    float duckElapsed = duckHoldSeconds + duckRestoreSeconds;
    float mechanismDuck = 1.0f;
    bool ownsCom = false;

    ~State() {
        for (auto& track : tracks) destroySource(track.voice);
        for (auto& effect : effects) destroySource(effect.voice);
        for (auto& mechanism : mechanisms) destroySource(mechanism.voice);
        destroySource(clickVoice);
        if (master) master->DestroyVoice();
        if (engine) engine->Release();
        if (ownsCom) CoUninitialize();
    }

    bool startVoice(IXAudio2SourceVoice*& voice, const WaveData& wave, bool loop, float volume = -1.0f) {
        if (voice) {
            if (FAILED(voice->Stop()) || FAILED(voice->FlushSourceBuffers())) {
                destroySource(voice);
                return false;
            }
        } else if (FAILED(engine->CreateSourceVoice(&voice, &wave.format, XAUDIO2_VOICE_NOPITCH))) {
            return false;
        }
        XAUDIO2_BUFFER buffer{};
        buffer.AudioBytes = static_cast<UINT32>(wave.samples.size());
        buffer.pAudioData = wave.samples.data();
        buffer.Flags = XAUDIO2_END_OF_STREAM;
        buffer.LoopCount = loop ? XAUDIO2_LOOP_INFINITE : 0;
        if (volume < 0.0f) volume = loop ? 0.0f : 1.0f;
        if (FAILED(voice->SetVolume(volume)) ||
            FAILED(voice->SubmitSourceBuffer(&buffer)) || FAILED(voice->Start())) {
            destroySource(voice);
            return false;
        }
        return true;
    }

    void applyMusicVolumes() {
        const float restore = std::clamp((duckElapsed - duckHoldSeconds) / duckRestoreSeconds, 0.0f, 1.0f);
        const float duck = std::min(duckGain + (1.0f - duckGain) * restore, mechanismDuck);
        for (auto& track : tracks)
            if (track.voice) track.voice->SetVolume(musicGain * track.weight * track.sceneGain * duck);
    }
};

Audio& Audio::instance() {
    static Audio audio;
    return audio;
}

Audio::~Audio() = default;

bool Audio::initialize() {
    if (state_) return true;
    auto state = std::make_unique<State>();
    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(com) && com != RPC_E_CHANGED_MODE) return false;
    state->ownsCom = SUCCEEDED(com);
    if (FAILED(XAudio2Create(&state->engine)) || FAILED(state->engine->CreateMasteringVoice(&state->master)))
        return false;
    const auto directory = musicDirectory();
    if (!directory.empty()) {
        for (std::size_t index = 0; index < state->tracks.size(); ++index)
            state->tracks[index].wave = readWave(directory / musicFiles[index]);
        const auto effectsDirectory = directory.parent_path() / L"sfx";
        constexpr const wchar_t* effectNames[]{L"footstep.wav", L"land.wav", L"grab.wav", L"place.wav"};
        for (std::size_t index = 0; index < state->effects.size(); ++index)
            state->effects[index].wave = readWave(effectsDirectory / effectNames[index], false);
        constexpr const wchar_t* mechanismNames[]{L"bookcase.wav", L"lift.wav"};
        for (std::size_t index = 0; index < state->mechanisms.size(); ++index)
            state->mechanisms[index].wave = readWave(effectsDirectory / mechanismNames[index], false);
    }
    const auto clickBytes = home_sound::detail::makeDoorFitWave();
    state->click = parseWave(clickBytes, false);
    state_ = std::move(state);
    return true;
}

void Audio::shutdown() {
    state_.reset();
}

void Audio::selectMusic(RoomMusic room) {
    if (!state_) return;
    const int selected = musicIndex(room);
    auto& state = *state_;
    if (selected == state.selected) return;
    state.selected = selected;
    state.fadeElapsed = 0.0f;
    for (auto& track : state.tracks) track.fromWeight = track.weight;
    if (selected >= 0) {
        auto& track = state.tracks[static_cast<std::size_t>(selected)];
        if (!track.voice && track.wave) state.startVoice(track.voice, *track.wave, true);
    }
    state.applyMusicVolumes();
}

void Audio::update(float elapsed) {
    if (!state_ || !std::isfinite(elapsed) || elapsed <= 0.0f) return;
    auto& state = *state_;
    state.fadeElapsed = std::min(crossfadeSeconds, state.fadeElapsed + elapsed);
    state.duckElapsed = std::min(duckHoldSeconds + duckRestoreSeconds, state.duckElapsed + elapsed);
    const float blend = state.fadeElapsed / crossfadeSeconds;
    for (std::size_t index = 0; index < state.tracks.size(); ++index) {
        auto& track = state.tracks[index];
        const float target = static_cast<int>(index) == state.selected ? 1.0f : 0.0f;
        track.weight = track.fromWeight + (target - track.fromWeight) * blend;
        if (target == 0.0f && blend >= 1.0f) destroySource(track.voice);
    }
    bool movingMechanism = false;
    for (auto& mechanism : state.mechanisms) {
        const float duration = mechanism.target > mechanism.fromGain
            ? mechanismAttackSeconds : mechanismReleaseSeconds;
        mechanism.elapsed = std::min(duration, mechanism.elapsed + elapsed);
        const float amount = mechanism.elapsed / duration;
        mechanism.gain = mechanism.fromGain + (mechanism.target - mechanism.fromGain) * amount;
        if (mechanism.voice) {
            mechanism.voice->SetVolume(mechanismGain * mechanism.gain);
            if (mechanism.target == 0.0f && amount >= 1.0f) {
                destroySource(mechanism.voice);
            } else {
                movingMechanism = true;
            }
        }
    }
    state.mechanismDuck = movingMechanism ? mechanismDuckGain
        : std::min(1.0f, state.mechanismDuck +
                         elapsed * (1.0f - mechanismDuckGain) / mechanismDuckRestoreSeconds);
    state.applyMusicVolumes();
}

void Audio::playDoorFitClick() {
    if (!state_ || !state_->click) return;
    auto& state = *state_;
    destroySource(state.clickVoice);
    if (state.startVoice(state.clickVoice, *state.click, false)) {
        state.duckElapsed = 0.0f;
        state.applyMusicVolumes();
    }
}

void Audio::playEffect(SoundEffect effect, float gain) {
    if (!state_ || !std::isfinite(gain)) return;
    const int index = static_cast<int>(effect);
    if (index < 0 || index >= static_cast<int>(state_->effects.size())) return;
    gain = std::clamp(gain, 0.0f, 1.0f);
    auto& sample = state_->effects[static_cast<std::size_t>(index)];
    if (gain > 0.0f && sample.wave)
        state_->startVoice(sample.voice, *sample.wave, false, effectGain * gain);
}

void Audio::setMechanismSound(MechanismSound mechanism, float gain) {
    if (!state_) return;
    const int index = static_cast<int>(mechanism);
    if (index < 0 || index >= static_cast<int>(state_->mechanisms.size())) return;
    if (!std::isfinite(gain)) gain = 0.0f;
    gain = std::clamp(gain, 0.0f, 1.0f);
    auto& sound = state_->mechanisms[static_cast<std::size_t>(index)];
    if (gain > 0.0f && !sound.voice) {
        if (!sound.wave || !state_->startVoice(sound.voice, *sound.wave, true, mechanismGain * sound.gain)) return;
    }
    if (sound.target != gain) {
        sound.fromGain = sound.gain;
        sound.elapsed = 0.0f;
        sound.target = gain;
    }
    if (sound.voice && gain > 0.0f) {
        state_->mechanismDuck = mechanismDuckGain;
        state_->applyMusicVolumes();
    }
}

void Audio::setSceneMusicGain(float gain) {
    if (!state_ || state_->selected < 0) return;
    if (!std::isfinite(gain)) gain = 0.0f;
    auto& track = state_->tracks[static_cast<std::size_t>(state_->selected)];
    track.sceneGain = std::clamp(gain, 0.0f, 1.0f);
    state_->applyMusicVolumes();
}
