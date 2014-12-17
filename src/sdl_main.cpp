#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <stdint.h>
#include <x86intrin.h>
#include <assert.h>

#include "handmade.hpp"

#if !defined(MAP_ANONYMOUS)
    
    #if defined(MAP_ANON)
        #define MAP_ANONYMOUS MAP_ANON
    #endif

#endif


//TODO: get rid of this struct
struct Texture {
    Pixel* pixels = nullptr;
    uint32_t sizeInBytes = 0; //size of pixels in bytes
    SDL_Texture* sdlTexture = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
};

struct InputContext {
    SDL_GameController* controllers[MAX_CONTROLLERS] = {};
};

struct SDLSoundRingBuffer {
    uint32_t sampleToPlay = 0;
    uint32_t runningIndex = 0;
    Sample samples[SOUND_FREQ];
};

static bool running = true;

static Texture gTexture;
static OffScreenBuffer gOsb;

static void printGeneralErrorAndExit(const char* message) {
    fprintf(stderr, "Fatal Error: %s\n", message);
    exit(1);

}


//TODO: This function can both init and resize a texture.  Rename
//to something better.  Or, refactor
static void resizeTexture(Texture* texture, OffScreenBuffer* osb, int newWidth, int newHeight,
        SDL_Renderer* renderer) {
    int sizeOfBuffer = newWidth * newHeight * sizeof(Pixel);

    if (texture->pixels) {
        munmap(texture->pixels, texture->sizeInBytes);

    }

    if(!(texture->pixels = (Pixel*)mmap(NULL,
            sizeOfBuffer,
            PROT_READ | PROT_WRITE,
            MAP_ANONYMOUS | MAP_PRIVATE ,
            -1,
            0))) {
        printGeneralErrorAndExit("Cannot allocate memeory\n");

    }

    texture->sizeInBytes = sizeOfBuffer;

    if (texture->sdlTexture) {
        SDL_DestroyTexture(texture->sdlTexture);

       
    }
    texture->sdlTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                SDL_TEXTUREACCESS_STREAMING, newWidth, newHeight);


    texture->width = newWidth;
    texture->height = newHeight;

    //quick hack before i get rid of texture struct
    osb->pixels = texture->pixels;
    osb->height = texture->height;
    osb->width = texture->width;
}

static void printSDLErrorAndExit(void) {
    fprintf(stderr, "Fatal SDL error. Error: %s\n", SDL_GetError());
    exit(1);
}

/*
 *---------------------------------------------------------------
 *              |                                |               |                
 *              |                                |               |
 *              |                                |               |                
 *              |                                |               |                
 * Region 2     |                                | Region 1      |
 *              |                                |               |
 *              |                                |               |
 *              |                                |               |
 *---------------------------------------------------------------
 *           end                               start       numSamples
 */

static void updateSDLSoundBuffer(SDLSoundRingBuffer* dest, const SoundBuffer* src, uint32_t start, uint32_t end) {

    Sample* region1 = dest->samples + start;
    uint32_t region1Len = (start <= end) ? end - start : arraySize(dest->samples) - start ;
    Sample* region2 = dest->samples;
    uint32_t region2Len = (start <= end) ? 0 : end;

    memcpy(region1, src->samples, region1Len * sizeof(Sample));  
    memcpy(region2, src->samples + region1Len, region2Len * sizeof(Sample));

    dest->runningIndex += region1Len + region2Len;


}

static void SDLAudioCallBack(void* userData, uint8_t* stream, int len) {
    
    SDLSoundRingBuffer* buf = (SDLSoundRingBuffer*)userData;


    static Sample firstSample = {};
    firstSample = *buf->samples;
    uint32_t samplesRequested = len / sizeof(Sample);
    uint32_t ringBufferLen = arraySize(buf->samples);

    assert(len % sizeof(Sample) == 0);

    uint32_t region1Len = samplesRequested; 
    uint32_t region2Len = 0; 

    if (ringBufferLen - buf->sampleToPlay < samplesRequested) {
        region1Len = ringBufferLen - buf->sampleToPlay;
        region2Len = samplesRequested - region1Len;
    }

    assert((region1Len + region2Len) * sizeof(Sample) == len);

    memcpy(stream, buf->samples + buf->sampleToPlay, region1Len * sizeof(Sample));
    memcpy(stream + (region1Len * sizeof(Sample)), buf->samples, region2Len * sizeof(Sample));

    //TODO: Take out eventually
    for(size_t i = 0; i < len / sizeof(Sample); i++){
        assert(((Sample*)stream)[i].rightChannel == buf->samples[(i + buf->sampleToPlay) % ringBufferLen].rightChannel);
    }

    buf->sampleToPlay = (buf->sampleToPlay + region1Len + region2Len) % ringBufferLen;
}

static void initAudio(SDLSoundRingBuffer* srb) {
    SDL_AudioSpec desiredAudio;
    desiredAudio.channels = NUM_CHANNELS;
    desiredAudio.samples = 2048;
    desiredAudio.freq = SOUND_FREQ;
    desiredAudio.format = AUDIO_S16LSB;
    desiredAudio.callback = SDLAudioCallBack;
    desiredAudio.userdata = srb; 



    if (SDL_OpenAudio(&desiredAudio, NULL) < 0) {
        printSDLErrorAndExit();
    }
}

static void initInput(InputContext* ic) {
    for (int i = 0; i < MAX_CONTROLLERS; i++) {
        if (SDL_IsGameController(i)) {
            if(!(ic->controllers[i] = SDL_GameControllerOpen(i))){
                printSDLErrorAndExit();
            }
        }
    }
}

static void initSDL(SDL_Window** window, SDL_Renderer** renderer, OffScreenBuffer* osb, InputContext* ic, SDLSoundRingBuffer* srb) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        printSDLErrorAndExit();
    }

    if(!(*window = SDL_CreateWindow("Handmade Hero",
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            SCREEN_WIDTH,
            SCREEN_HEIGHT,
            SDL_WINDOW_RESIZABLE))) {
        printSDLErrorAndExit();
    }

    if(!(*renderer = SDL_CreateRenderer(*window, -1, 0))) {
        printSDLErrorAndExit();
    }

    resizeTexture(&gTexture, osb, SCREEN_WIDTH, SCREEN_HEIGHT, *renderer);

    initAudio(srb);
    initInput(ic);

}

static void cleanUp(void) {
    SDL_CloseAudio();
    SDL_Quit();
}

static void updateWindow(SDL_Window* window, Texture texture) {
    SDL_Renderer* renderer = SDL_GetRenderer(window);
    SDL_RenderClear(renderer);
    
    if(SDL_UpdateTexture(texture.sdlTexture, NULL, texture.pixels, 
                texture.width * sizeof(Pixel)) != 0) {
        printSDLErrorAndExit();
    }

    if(SDL_RenderCopy(renderer, texture.sdlTexture, NULL, NULL) != 0) {
        printSDLErrorAndExit();
    }

    SDL_RenderPresent(renderer);
}

static void processWindowEvent(SDL_WindowEvent* we){
    switch (we->event) {
        case SDL_WINDOWEVENT_RESIZED:
            printf("SDL_WINDOWEVENT_RESIZED (%d, %d)\n", we->data1, we->data2);
           resizeTexture(&gTexture, &gOsb, we->data1, we->data2,
                    SDL_GetRenderer(SDL_GetWindowFromID(we->windowID)));
            break;
    }
}

static void processEvent(SDL_Event* e) {
    switch (e->type) {
        case SDL_QUIT:
            running = false;
            break;
        case SDL_WINDOWEVENT:
            processWindowEvent(&e->window);
            break;
    }
}


int main(void) {

    SDL_Event e;
    SDL_Window *window;
    SDL_Renderer *renderer;
    InputContext ic;
    SDLSoundRingBuffer srb;
    SoundBuffer sb;

    sb.volume = 5000;
    sb.tone = 256;

    int32_t xOffset = 0;
    int32_t yOffset = 0;

    initSDL(&window, &renderer, &gOsb, &ic, &srb);
    
    
    uint64_t countFreq = SDL_GetPerformanceFrequency();
    uint64_t startCount = SDL_GetPerformanceCounter();
    uint64_t startCycles = _rdtsc();

    SDL_PauseAudio(0);
    while(running) {
        SDL_PollEvent(&e);
        processEvent(&e);

        //input
        int16_t xVal = SDL_GameControllerGetAxis(ic.controllers[0], SDL_CONTROLLER_AXIS_LEFTX);
        int16_t yVal = SDL_GameControllerGetAxis(ic.controllers[0], SDL_CONTROLLER_AXIS_LEFTY);

        xOffset += xVal / 4096;
        yOffset += yVal / 4096;

        sb.tone = 512 + (int)(256.0f*((real32_t)yVal / 30000.0f));



        SDL_LockAudioDevice(1);
        uint32_t startIndex = srb.runningIndex % arraySize(srb.samples);
        uint32_t endIndex = (srb.sampleToPlay + SOUND_LATENCY) % arraySize(srb.samples);


        uint32_t samplesToGetFromGame = (startIndex <= endIndex) ? endIndex - startIndex : (arraySize(srb.samples) - startIndex) + endIndex; 
        sb.numSamples = samplesToGetFromGame;
        
        SDL_UnlockAudioDevice(1);

        //printf("start: %d end: %d samples to get from game: %d\n",startIndex, endIndex, samplesToGetFromGame);
        gameUpdateAndRender(&gOsb, xOffset, yOffset, &sb);

        updateSDLSoundBuffer(&srb, &sb, startIndex, endIndex);
        updateWindow(window, gTexture);

     
        //benchmark stuff
        /*
        uint64_t endCount = SDL_GetPerformanceCounter();
        uint64_t endCycles = __rdtsc();

        real64_t secPerFrame = (real64_t)(endCount - startCount)/countFreq;
        real64_t fpsCount =  ((1./secPerFrame));
        real64_t mcPerFrame = (real64_t)(endCycles-startCycles) / (1000 * 1000 );

        printf("TPF: %.2fms FPS: %.2f MCPF: %.2f\n", secPerFrame*1000, fpsCount, mcPerFrame);


        startCount = endCount;
        startCycles = endCycles;
*/
    }

    cleanUp();
    return 0;
}
