#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <stdint.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "handmade.hpp"
#include "sdl_main.hpp"


static bool running = true;

static Texture gTexture;
static OffScreenBuffer gOsb;

static inline uint64_t
_rdtsc(void)
{
	uint32_t eax = 0, edx;

	__asm__ __volatile__("cpuid;"
			     "rdtsc;"
				: "+a" (eax), "=d" (edx)
				:
				: "%rcx", "%rbx", "memory");

	__asm__ __volatile__("xorl %%eax, %%eax;"
			     "cpuid;"
				:
				:
				: "%rax", "%rbx", "%rcx", "%rdx", "memory");

	return (((uint64_t)edx << 32) | eax);
}
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

static void updateSDLSoundBuffer(SDLSoundRingBuffer* dest, const GameSoundOutput* src, uint32_t start, uint32_t end) {

    Sample* region1 = dest->samples + start;
    uint32_t region1Len = (start <= end) ? end - start : ARRAY_SIZE(dest->samples) - start ;
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
    uint32_t ringBufferLen = ARRAY_SIZE(buf->samples);

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

#ifndef NDEBUG
    //TODO: Take out eventually
    for(size_t i = 0; i < len / sizeof(Sample); i++){
        assert(((Sample*)stream)[i].rightChannel == buf->samples[(i + buf->sampleToPlay) % ringBufferLen].rightChannel);
    }
#endif

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

static void initInput(SDLInputContext* sdlIC) {
    for (int i = 0; i < MAX_SDL_CONTROLLERS; i++) {
        if (SDL_IsGameController(i)) {
            if(!(sdlIC->controllers[i] = SDL_GameControllerOpen(i))){
                printSDLErrorAndExit();
            }
        }
    }
}

static void initSDL(SDL_Window** window, SDL_Renderer** renderer, OffScreenBuffer* osb, SDLInputContext* sdlIC, SDLSoundRingBuffer* srb) {
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
    initInput(sdlIC);

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

static void processKeyPress(ButtonState* buttonThatKeyCorrespondsTo, bool isDown) {
    buttonThatKeyCorrespondsTo->isEndedDown = isDown;
    buttonThatKeyCorrespondsTo->halfTransitionCount += (isDown) ? 1 : 0; 
}


static void processEvent(SDL_Event* e, ControllerInput* keyboardController) {
    ControllerInput controllerInputStates;
    
    switch (e->type) {
        case SDL_QUIT:
            running = false;
            break;
        case SDL_WINDOWEVENT:
            processWindowEvent(&e->window);
            break;

        case SDL_KEYDOWN:
        case SDL_KEYUP:
            bool isDown = e->key.state == SDL_PRESSED;
            isDown = e->key.state != SDL_RELEASED;
            if(e->key.repeat == 0) {
                switch(e->key.keysym.sym) {
                    case SDLK_w:
                        processKeyPress(&keyboardController->directionUp, isDown);
                        break;
                    case SDLK_s:
                        processKeyPress(&keyboardController->directionDown, isDown);
                        break;
                    case SDLK_a:
                        processKeyPress(&keyboardController->directionLeft, isDown);
                        break;
                    case SDLK_d:
                        processKeyPress(&keyboardController->directionRight, isDown);
                        break;
                }
            }
            break;
    }

}

static void processControllerButtonInput(ButtonState* newState, const ButtonState* oldState, bool* isAnalog, SDL_GameController* controller, SDL_GameControllerButton sdlButton) {
    newState->isEndedDown = SDL_GameControllerGetButton(controller, sdlButton);
    if(newState->isEndedDown) {

        switch(sdlButton) {
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
            case SDL_CONTROLLER_BUTTON_DPAD_UP:
                *isAnalog = false;
                break;
            default:
                break;
        }
    }

    if(newState->isEndedDown != oldState->isEndedDown) {
        newState->halfTransitionCount +=  1;
    }



}

static void processControllerStickInput(ButtonState* newState, const ButtonState* oldState, bool* isAnalog, SDL_GameController* controller, SDL_GameControllerAxis sdlButton) {



}

static real32_t normalizeStickInput(int16_t stickInput, uint16_t deadZone) {
    real32_t value = 0.f;

    if(stickInput > deadZone) {
        value = stickInput / 32767.0;
    }
    else if(stickInput < -deadZone){
        value = stickInput / 32768.0;
    }

    return value;
}

static ControllerInput* getContoller(InputContext* sdlIC, int index) {
    assert(index < ARRAY_SIZE(sdlIC->controllers));

    return &sdlIC->controllers[index];
}

int main(void) {

    SDL_Event e;
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDLInputContext sdlIC;
    SDLSoundRingBuffer srb;
    GameSoundOutput sb;
    GameMemory memory;

    //controller input state
    InputContext inputStates[2]; //contains old and new state
    InputContext* newInputState = &inputStates[0];
    InputContext* oldInputState = &inputStates[1];


    sb.volume = 2500;

    memory.permanentStorageSize = MB(64);
    memory.transientStorageSize = GB(1);
    memory.transientStorage = mmap(nullptr, memory.transientStorageSize + memory.permanentStorageSize, PROT_READ | PROT_WRITE,
            MAP_ANONYMOUS | MAP_PRIVATE ,
            -1,
            0);
    memory.permanentStorage = (uint8_t*)(memory.transientStorage) + memory.transientStorageSize;


    initSDL(&window, &renderer, &gOsb, &sdlIC, &srb);
   
    
    uint64_t countFreq = SDL_GetPerformanceFrequency();
    uint64_t startCount = SDL_GetPerformanceCounter();
    uint64_t startCycles = _rdtsc();

    debugFileWrite("test.out", (void*)"HELLO!!!", strlen("HELLO!!!") + 1);
    FileContents file = debugFileRead("test.out");

    printf("%s\n", file.contents);
    
    debugFreeFileContents(&file);

    SDL_PauseAudio(0);
    while(running) {

        //keyboard input
        ControllerInput* newKeyInput = getContoller(newInputState, 0);
        
        ControllerInput* oldKeyInput = getContoller(oldInputState, 0);
        *newKeyInput = {};

        for(size_t i = 0; i < ARRAY_SIZE(oldKeyInput->buttons); i++) {
            newKeyInput->buttons[i] = oldKeyInput->buttons[i];
        }

        while(SDL_PollEvent(&e)) {
            processEvent(&e, newKeyInput);
        }


        //controller input
        for(int i = 0; i < MAX_SDL_CONTROLLERS; i++) {
            if(sdlIC.controllers[i] != nullptr && SDL_GameControllerGetAttached(sdlIC.controllers[i])) {

                ControllerInput* newCIState = getContoller(newInputState, i+1);
                ControllerInput* oldCIState = getContoller(oldInputState, i+1);
                

                int16_t xVal = SDL_GameControllerGetAxis(sdlIC.controllers[i], SDL_CONTROLLER_AXIS_LEFTX);
                int16_t yVal = SDL_GameControllerGetAxis(sdlIC.controllers[i], SDL_CONTROLLER_AXIS_LEFTY);

                newCIState->avgX = normalizeStickInput(xVal, LEFT_THUMB_DEADZONE);
                newCIState->avgY = normalizeStickInput(yVal, LEFT_THUMB_DEADZONE);

                if(newCIState->avgX != 0 || newCIState->avgY != 0) {
                    newCIState->isAnalog = true;
                }

                processControllerButtonInput(&newCIState->actionDown, &oldCIState->actionDown, &newCIState->isAnalog, sdlIC.controllers[i], SDL_CONTROLLER_BUTTON_A);
                processControllerButtonInput(&newCIState->actionUp, &oldCIState->actionUp, &newCIState->isAnalog, sdlIC.controllers[i], SDL_CONTROLLER_BUTTON_Y);
                processControllerButtonInput(&newCIState->actionLeft, &oldCIState->actionLeft, &newCIState->isAnalog, sdlIC.controllers[i], SDL_CONTROLLER_BUTTON_X);
                processControllerButtonInput(&newCIState->actionRight, &oldCIState->actionRight, &newCIState->isAnalog, sdlIC.controllers[i], SDL_CONTROLLER_BUTTON_B);

                processControllerButtonInput(&newCIState->directionDown, &oldCIState->directionDown, &newCIState->isAnalog, sdlIC.controllers[i], SDL_CONTROLLER_BUTTON_DPAD_DOWN);
                processControllerButtonInput(&newCIState->directionUp, &oldCIState->directionUp, &newCIState->isAnalog, sdlIC.controllers[i], SDL_CONTROLLER_BUTTON_DPAD_UP);
                processControllerButtonInput(&newCIState->directionLeft, &oldCIState->directionLeft, &newCIState->isAnalog, sdlIC.controllers[i], SDL_CONTROLLER_BUTTON_DPAD_LEFT);
                processControllerButtonInput(&newCIState->directionRight, &oldCIState->directionRight, &newCIState->isAnalog, sdlIC.controllers[i], SDL_CONTROLLER_BUTTON_DPAD_RIGHT);

                oldCIState->isAnalog = newCIState->isAnalog;


            }
            else {
                //TODO: Logging
            }
        }


        //calculate audio buffers' indicies and sizes
        SDL_LockAudioDevice(1);
        uint32_t startIndex = srb.runningIndex % ARRAY_SIZE(srb.samples);
        uint32_t endIndex = (srb.sampleToPlay + SOUND_LATENCY) % ARRAY_SIZE(srb.samples);
        uint32_t samplesToGetFromGame = (startIndex <= endIndex) ? endIndex - startIndex : (ARRAY_SIZE(srb.samples) - startIndex) + endIndex; 
        sb.numSamples = samplesToGetFromGame;
        SDL_UnlockAudioDevice(1);



        gameUpdateAndRender(&memory, &gOsb, &sb, newInputState);

        updateSDLSoundBuffer(&srb, &sb, startIndex, endIndex);
        updateWindow(window, gTexture);

        InputContext* temp = newInputState;
        newInputState = oldInputState;
        oldInputState = temp;
     
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


FileContents debugFileRead(const char* fileName) {
    FileContents contents;
    struct stat fileStats;

    stat(fileName, &fileStats);
    contents.contents = malloc(fileStats.st_size);

    contents.contentsSize = fileStats.st_size;

    FILE* f;

    if((f = fopen(fileName, "rb"))) {
        if(fread(contents.contents, contents.contentsSize, 1, f) < 1) {
            //TODO logging
        }

        fclose(f);
    }
    else {
        //TODO logging
    }



    return contents;
}


void debugFileWrite(const char* fileName, void* dataToWrite, uint64_t numBytesToWrite) {
    FILE* f;

    if((f = fopen(fileName, "wb"))) {
        if(fwrite(dataToWrite, numBytesToWrite, 1, f) < 1) {
            //TODO logging
        }

        fclose(f);
    }
    else {
        //TODO logging
    }

}
void debugFreeFileContents(FileContents* contents) {
    free(contents->contents);
    memset(contents, 0, sizeof(*contents));
}
