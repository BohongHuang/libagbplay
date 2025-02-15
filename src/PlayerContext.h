#pragma once

#include <vector>

#include "SequenceReader.h"
#include "SoundMixer.h"
#include "Rom.h"
#include "ConfigManager.h"

/* Instead of defining lots of global objects, we define
 * a context with all the things we need. So anything which
 * needs anything out of this context only needs a reference
 * to a PlayerContext */

struct PlayerContext {
public:
    PlayerContext(std::shared_ptr<Rom> rom, int8_t maxLoops, uint8_t maxTracks, EnginePars pars, std::shared_ptr<ConfigManager> config);
    PlayerContext(const PlayerContext&) = delete;
    PlayerContext& operator=(const PlayerContext&) = delete;

    void Process(std::vector<std::vector<sample>>& trackAudio);
    void InitSong(size_t songPos);
    bool HasEnded() const;

    const std::shared_ptr<Rom> &GetRom() const;
    const std::shared_ptr<ConfigManager> &GetConfig() const;

    SequenceReader reader;
    SoundMixer mixer;
    Sequence seq;
    SoundBank bnk;
    EnginePars pars;

    // sound channels
    std::list<SoundChannel> sndChannels;
    std::list<SquareChannel> sq1Channels;
    std::list<SquareChannel> sq2Channels;
    std::list<WaveChannel> waveChannels;
    std::list<NoiseChannel> noiseChannels;
private:
    std::shared_ptr<Rom> rom;
    std::shared_ptr<ConfigManager> config;
};
