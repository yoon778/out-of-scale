#pragma once

#include <memory>

enum class RoomMusic { None, School, Military, Office, Home };

class Audio {
public:
    static Audio& instance();
    ~Audio();

    bool initialize();
    void shutdown();
    void selectMusic(RoomMusic room);
    void update(float elapsed);
    void playDoorFitClick();
    void setSceneMusicGain(float gain);

    Audio(const Audio&) = delete;
    Audio& operator=(const Audio&) = delete;

private:
    Audio() = default;
    struct State;
    std::unique_ptr<State> state_;
};
