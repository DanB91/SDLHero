#pragma once

#include "handmade.hpp"
#include <SDL.h>

#if !defined(MAP_ANONYMOUS)
    
    #if defined(MAP_ANON)
        #define MAP_ANONYMOUS MAP_ANON
    #endif

#endif

#define MAX_SDL_CONTROLLERS 4
#define DEFAULT_REFRESH_RATE 60


//TODO: get rid of this struct
struct Texture {
    Pixel* pixels = nullptr;
    uint32_t sizeInBytes = 0; //size of pixels in bytes
    SDL_Texture* sdlTexture = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
};

struct SDLInputContext {
    SDL_GameController* controllers[MAX_SDL_CONTROLLERS] = {};
};

typedef void GameUpdateAndRenderFunc(GameMemory* memory, OffScreenBuffer *buffer, GameSoundOutput* sb, const InputContext* ci, real32_t secsSinceLastFrame);

struct SDLSoundRingBuffer {
    uint32_t sampleToPlay = 0;
    uint32_t runningIndex = 0;
    Sample samples[SOUND_FREQ];
};


#define GAME_LIB_PATH "./game.so" 

struct GameCode {
    time_t dateLastModified = 0;  //time the library file was last modified
    void* libraryHandle = nullptr;
    GameUpdateAndRenderFunc* guarf = nullptr;
};
#define GAME_INPUT_PATH "game_input.bin"
#define GAME_STATE_PATH "game_state.bin"

struct PlatformState {
    bool running = true;
    bool isRecording = false;
    bool isPlayingBack = false;
    FILE* inputRecordFile = nullptr;
    uint64_t gameMemorySize = 0;
    void* memoryBlock;
};
