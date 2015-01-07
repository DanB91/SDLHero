#pragma once

#include "handmade.hpp"
#include <SDL.h>

#if !defined(MAP_ANONYMOUS)
    
    #if defined(MAP_ANON)
        #define MAP_ANONYMOUS MAP_ANON
    #endif

#endif

#define MAX_SDL_CONTROLLERS 4


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

struct SDLSoundRingBuffer {
    uint32_t sampleToPlay = 0;
    uint32_t runningIndex = 0;
    Sample samples[SOUND_FREQ];
};

