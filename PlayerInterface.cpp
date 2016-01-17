#include <thread>
#include <chrono>

#include "PlayerInterface.h"
#include "MyException.h"
#include "Debug.h"
#include "Util.h"

using namespace std;
using namespace agbplay;

/*
 * public PlayerInterface
 */

PlayerInterface::PlayerInterface(Rom& _rom, TrackviewGUI *trackUI, long initSongPos, EnginePars pars) 
    : rom(_rom), seq(initSongPos, 16, _rom) {
    this->trackUI = trackUI;
    this->playerState = State::THREAD_DELETED;
    this->pars = pars;
    sg = new StreamGenerator(seq, EnginePars(ENG_VOL, ENG_REV, ENG_RATE), 1);
}

PlayerInterface::~PlayerInterface() 
{
    // stop and deallocate player thread if required
    Stop();
    delete sg;
}

void PlayerInterface::LoadSong(long songPos, uint8_t trackLimit)
{
    bool play = playerState == State::PLAYING;
    Stop();
    seq = Sequence(songPos, trackLimit, rom);
    trackUI->SetState(seq);
    delete sg;
    sg = new StreamGenerator(seq, EnginePars(ENG_VOL, ENG_REV, ENG_RATE), 1);
    if (play)
        Play();
}

void PlayerInterface::Play()
{
    switch (playerState) {
        case State::RESTART:
            // --> handled by worker
            break;
        case State::PLAYING:
            // restart song if player is running
            playerState = State::RESTART;
            break;
        case State::PAUSED:
            // continue paused playback
            playerState = State::PLAYING;
            break;
        case State::TERMINATED:
            // thread needs to be deleted before restarting
            Stop();
            Play();
            break;
        case State::SHUTDOWN:
            // --> handled by worker
            break;
        case State::THREAD_DELETED:
            playerState = State::PLAYING;
            playerThread = new boost::thread(&PlayerInterface::threadWorker, this);
            __print_debug("Started new player thread");
            // start thread and play back song
            break;
    }
}

void PlayerInterface::Pause()
{
    switch (playerState) {
        case State::RESTART:
            // --> handled by worker
            break;
        case State::PLAYING:
            playerState = State::PAUSED;
            break;
        case State::PAUSED:
            playerState = State::PLAYING;
            break;
        case State::TERMINATED:
            // ingore this
            break;
        case State::SHUTDOWN:
            // --> handled by worker
            break;
        case State::THREAD_DELETED:
            // ignore this
            break;
    }
}

void PlayerInterface::Stop()
{
    switch (playerState) {
        case State::RESTART:
            // wait until player has initialized and quit then
            while (playerState != State::PLAYING) {
                this_thread::sleep_for(chrono::milliseconds(5));
            }
            Stop();
            break;
        case State::PLAYING:
            playerState = State::SHUTDOWN;
            Stop();
            break;
        case State::PAUSED:
            playerState = State::SHUTDOWN;
            Stop();
            break;
        case State::TERMINATED:
            playerThread->join();
            delete playerThread;
            playerState = State::THREAD_DELETED;
            break;
        case State::SHUTDOWN:
            // incase stop needs to be done force stop
            playerThread->join();
            delete playerThread;
            playerState = State::THREAD_DELETED;
            break;            
        case State::THREAD_DELETED:
            // ignore this
            break;
    }
}

bool PlayerInterface::IsPlaying()
{
    return playerState != State::THREAD_DELETED && playerState != State::TERMINATED;
}

void PlayerInterface::UpdateView()
{
    if (playerState != State::THREAD_DELETED && playerState != State::SHUTDOWN)
        trackUI->SetState(sg->GetWorkingSequence());
}

/*
 * private PlayerInterface
 */

void PlayerInterface::threadWorker()
{
    // TODO add fixed mode rate variable
    PaError err;
    uint32_t nBlocks = sg->GetBufferUnitCount();
    uint32_t outSampleRate = sg->GetRenderSampleRate();
    if ((err = Pa_OpenDefaultStream(&audioStream, 0, 2, paFloat32, outSampleRate, nBlocks, NULL, NULL)) != paNoError)
        throw MyException(FormatString("PA Error: %s", Pa_GetErrorText(err)));
    if ((err = Pa_StartStream(audioStream)) != paNoError)
        throw MyException(FormatString("PA Error: %s", Pa_GetErrorText(err)));

    vector<float> silence(nBlocks * N_CHANNELS, 0.0f);

    while (playerState != State::SHUTDOWN) {
        switch (playerState) {
            case State::RESTART:
                delete sg;
                sg = new StreamGenerator(seq, EnginePars(ENG_VOL, ENG_REV, ENG_RATE), 1);
                playerState = State::PLAYING;
            case State::PLAYING:
                {
                    float *processedAudio = sg->ProcessAndGetAudio();
                    if (processedAudio == nullptr){
                        playerState = State::SHUTDOWN;
                        break;
                    }
                    if ((err = Pa_WriteStream(audioStream, processedAudio, nBlocks)) != paNoError) {
                        __print_debug(FormatString("PA Error: %s", Pa_GetErrorText(err)));
                    };
                }
                break;
            case State::PAUSED:
                if ((err = Pa_WriteStream(audioStream, &silence[0], nBlocks)) != paNoError) {
                    __print_debug(FormatString("PA Error: %s", Pa_GetErrorText(err)));
                };
                break;
            default:
                throw MyException("Internal PlayerInterface error");
        }
    }

    if ((err = Pa_StopStream(audioStream)) != paNoError)
        throw MyException(FormatString("PA Error: %s", Pa_GetErrorText(err)));
    if ((err = Pa_CloseStream(audioStream)) != paNoError)
        throw MyException(FormatString("PA Error: %s", Pa_GetErrorText(err)));
    playerState = State::TERMINATED;
}
