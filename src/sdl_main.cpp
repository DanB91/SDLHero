#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <stdint.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dlfcn.h>
#include "handmade.hpp"
#include "sdl_main.hpp"



static Texture gTexture;
static OffScreenBuffer gOsb;

static void printGeneralErrorAndExit(const char* message) {
    fprintf(stderr, "Fatal Error: %s\n", message);
    exit(1);

}

static time_t getCreateTimeOfFile(const char* fileName) {

   //get date created
   struct stat fileStats;
   if(stat(fileName, &fileStats) == 0) {
       return fileStats.st_mtim.tv_sec;
   }
   else {
       printGeneralErrorAndExit("Could not get modify date of game lib");
       //TODO: Logging
   }

   return 0;
}

static GameCode loadGameCode() {
   GameCode ret;
    
   void* gameLib = dlopen(GAME_LIB_PATH, RTLD_LAZY);

   if(!gameLib) {
       //TODO: Logging
       printGeneralErrorAndExit(dlerror());
   }
   
   assert(gameLib);

   void* gameUpdateAndRenderPtr = dlsym(gameLib, "gameUpdateAndRender");

   if(!gameUpdateAndRenderPtr) {
       //TODO: Logging
       printGeneralErrorAndExit(dlerror());
   }

   ret.libraryHandle = gameLib;
   ret.guarf = (GameUpdateAndRenderFunc*) gameUpdateAndRenderPtr;

   ret.dateLastModified = getCreateTimeOfFile(GAME_LIB_PATH);

   return ret;
}

static void closeGameCode(GameCode* gameCode) {
    if(dlclose(gameCode->libraryHandle) == 0) {
        *gameCode = {};
    }
    else {
        //TODO: Logging
    }
}

static void reloadGameCode(GameCode* gameCode) {
    closeGameCode(gameCode);
    *gameCode = loadGameCode();
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

static void cleanUp(PlatformState* state, GameCode* gameCode) {
    closeGameCode(gameCode);
    munmap(state->memoryBlock, state->gameMemorySize);
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

static void beginRecording(PlatformState* state) {


    //write out the state
    FILE* stateFile;

    if((stateFile = fopen(GAME_STATE_PATH, "w"))) {
        if(fwrite(state->memoryBlock, state->gameMemorySize, 1, stateFile) == 1) {
            fclose(stateFile);

            //set isRecording to true
            state->isRecording = true;

            if((state->inputRecordFile = fopen(GAME_INPUT_PATH, "w"))){
                //NOTE: Opened game input file succesfully
            }
            else {
                assert(false);
                //TODO: Logging
            }
        }
        else {
            assert(false);
            //TODO: Logging
        }

    }
    else {
        assert(false);
        //TODO: Logging
    }


}

static void recordInput(InputContext* inputToRecord, FILE* fileToRecordTo) {
    if(fwrite(inputToRecord, sizeof(InputContext), 1, fileToRecordTo) != 1) {
        //TODO: Logging
    }
}

static void stopRecording(PlatformState* state) {
    if(fclose(state->inputRecordFile) != 0) {
        //TODO: Logging
    }

    state->isRecording = false;
    state->inputRecordFile = nullptr;

}

static void beginPlayback(PlatformState* state) {
    FILE* stateFile;

    if((stateFile = fopen(GAME_STATE_PATH, "r"))) {
        if(fread(state->memoryBlock, state->gameMemorySize, 1, stateFile) == 1) { //read state

            state->isPlayingBack = true;
            fclose(stateFile);

            if((state->inputRecordFile = fopen(GAME_INPUT_PATH, "r"))){
                //NOTE: Opened game input file succesfully
            }
            else {
                assert(false);
                //TODO: Logging
            }

        }
        else {
            assert(false);
            //TODO: Logging
        }

    }
    else {
        //We didn't record anything.  don't quit
        
        //TODO: Logging
    }

}

static void playInput(InputContext* inputToRecord, FILE* fileToPlay) {
    if(fread(inputToRecord, sizeof(InputContext), 1, fileToPlay) != 1) {
        rewind(fileToPlay);
    }
}

static void stopPlayback(PlatformState* state) {
    if(fclose(state->inputRecordFile) != 0) {
        //TODO: Logging
    }

    state->isPlayingBack = false;
    state->inputRecordFile = nullptr;

}

static ControllerInput* getContoller(InputContext* sdlIC, uint32_t index) {
    assert(index < ARRAY_SIZE(sdlIC->controllers));

    return &sdlIC->controllers[index];
}

static void processEvent(SDL_Event* e, InputContext* inputState, PlatformState* state) {
    ControllerInput* keyboardController = getContoller(inputState, 0);
    switch (e->type) {
        case SDL_QUIT:
            state->running = false;
            break;
        case SDL_WINDOWEVENT:
            processWindowEvent(&e->window);
            break;

        case SDL_KEYDOWN:
        case SDL_KEYUP:
            bool isDown = e->key.state == SDL_PRESSED;
            isDown &= e->key.state != SDL_RELEASED;
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
                    case SDLK_l: //start/stop recording
                        if(isDown && !state->isPlayingBack) {
                            if(state->isRecording) {
                                stopRecording(state);
                            }
                            else {
                                beginRecording(state);
                            }
                        }
                        break;
                    case SDLK_p: //start/stop playback
                        if(isDown && !state->isRecording) {
                            if(state->isPlayingBack) {
                                stopPlayback(state);
                                *inputState = {};
                            }
                            else {
                                beginPlayback(state);
                            }
                        }
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


static uint32_t getRefreshRate(SDL_Window* window) {
    int displayIndex = SDL_GetWindowDisplayIndex(window);
    SDL_DisplayMode displayMode; //stores refresh rate

    if(SDL_GetDesktopDisplayMode(displayIndex, &displayMode) != 0 
            && displayMode.refresh_rate != 0) {

        return displayMode.refresh_rate;
    }

    return DEFAULT_REFRESH_RATE;
}

static real32_t secondsForCountRange(uint64_t start, uint64_t end) {
    uint64_t countFreq = SDL_GetPerformanceFrequency();
    return (real32_t)(end - start) / countFreq;
}


int main(void) {

    PlatformState state;
    SDL_Event e;
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDLInputContext sdlIC;
    SDLSoundRingBuffer srb;
    GameSoundOutput sb;
    GameMemory gameMemory;

    //controller input state
    InputContext inputStates[2]; //contains old and new state
    InputContext* newInputState = &inputStates[0];
    InputContext* oldInputState = &inputStates[1];
    real32_t secsSinceLastFrame = 0;


    sb.volume = 2500;

    gameMemory.permanentStorageSize = MB(64);
    gameMemory.transientStorageSize = MB(64);
    state.gameMemorySize = gameMemory.transientStorageSize + gameMemory.permanentStorageSize;
    state.memoryBlock = mmap(nullptr, state.gameMemorySize, PROT_READ | PROT_WRITE,
            MAP_ANONYMOUS | MAP_PRIVATE ,
            -1,
            0);
    gameMemory.permanentStorage = state.memoryBlock;
    gameMemory.transientStorage = (uint8_t*)(gameMemory.transientStorage) + gameMemory.transientStorageSize;


    initSDL(&window, &renderer, &gOsb, &sdlIC, &srb);

    GameCode gameCode = loadGameCode();

    assert(gameCode.guarf);

    uint64_t startCount = SDL_GetPerformanceCounter();
    real32_t targetFrameSeconds = 1./getRefreshRate(window);

    SDL_PauseAudio(0);
    while(state.running) {

        if(getCreateTimeOfFile(GAME_LIB_PATH) != gameCode.dateLastModified) {
            reloadGameCode(&gameCode);
        }

        //keyboard input
        ControllerInput* newKeyInput = getContoller(newInputState, 0);

        //TODO: figure out why this is special
        ControllerInput* oldKeyInput = getContoller(oldInputState, 0);
        *newKeyInput = {};

        for(size_t i = 0; i < ARRAY_SIZE(oldKeyInput->buttons); i++) {
            newKeyInput->buttons[i] = oldKeyInput->buttons[i];
        }
        while(SDL_PollEvent(&e)) {
            processEvent(&e, newInputState, &state);
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


        //TODO: Do this instead of processing input, not after
        //process recording/playback
        assert(!(state.isRecording && state.isPlayingBack));

        if(state.isRecording) {
            recordInput(newInputState, state.inputRecordFile);
        }

        if(state.isPlayingBack) {
            playInput(newInputState, state.inputRecordFile);
        }

        //calculate audio buffers' indicies and sizes
        SDL_LockAudioDevice(1);
        uint32_t startIndex = srb.runningIndex % ARRAY_SIZE(srb.samples);
        uint32_t endIndex = (srb.sampleToPlay + SOUND_LATENCY) % ARRAY_SIZE(srb.samples);
        uint32_t samplesToGetFromGame = (startIndex <= endIndex) ? endIndex - startIndex : (ARRAY_SIZE(srb.samples) - startIndex) + endIndex; 
        sb.numSamples = samplesToGetFromGame;
        SDL_UnlockAudioDevice(1);



        gameCode.guarf(&gameMemory, &gOsb, &sb, newInputState, secsSinceLastFrame);

        updateSDLSoundBuffer(&srb, &sb, startIndex, endIndex);
        updateWindow(window, gTexture);

        InputContext* temp = newInputState;
        newInputState = oldInputState;
        oldInputState = temp;

        //benchmark stuff

        real32_t secsElapsed = secondsForCountRange(startCount, SDL_GetPerformanceCounter());

        //sleep to lock frame rate
        if(secsElapsed < targetFrameSeconds) {


            //NOTE: .5 denotes the amount we will spin manually since
            //      SDL_Delay is not 100% accurate
            real32_t timeToSleep = (targetFrameSeconds - secsElapsed)*1000 - .5;
            SDL_Delay(timeToSleep);
            secsElapsed = secondsForCountRange(startCount, SDL_GetPerformanceCounter());
            
            //This assert will fire if the window is moved
            //assert(secondsForCountRange(startCount, SDL_GetPerformanceCounter()) < targetFrameSeconds); 
            
            while(secondsForCountRange(startCount, SDL_GetPerformanceCounter()) < targetFrameSeconds) {
                //wait
            }
            secsElapsed = secondsForCountRange(startCount, SDL_GetPerformanceCounter());
        }
        uint64_t endCount = SDL_GetPerformanceCounter();
        real32_t fpsCount =  ((1./secsElapsed));
        real32_t mcPerFrame = (real32_t)(endCount-startCount) / (1000 * 1000 );


        printf("TPF: %.2fms FPS: %.2f MCPF: %.2f\n", secsElapsed*1000, fpsCount, mcPerFrame);

        startCount = endCount;
        secsSinceLastFrame = secsElapsed;
    }

    cleanUp(&state, &gameCode);
    return 0;
}


#ifndef NDBUG
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
#endif
