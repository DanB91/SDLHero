#pragma once

#include <stdint.h>
#include <stdio.h>

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480

#define SOUND_FREQ 48000
#define NUM_CHANNELS 2
#define SOUND_LATENCY (SOUND_FREQ / 15)

#define LEFT_THUMB_DEADZONE  7849
#define RIGHT_THUMB_DEADZONE 8689

#define MAX_CONTROLLERS 5

#define pi32 3.14159265358979f

#define NUM_BUTTONS 12

//utility macros/inline functions
#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

inline uint64_t KB(uint64_t num) {
    return num*1024ll;
}

inline uint64_t MB(uint64_t num) {
    return num*KB(num);
}

inline uint64_t GB(uint64_t num) {
    return num*MB(num);
}

inline uint64_t TB(uint64_t num) {
    return num*GB(num);
}

typedef float real32_t;
typedef double real64_t;


union Pixel {
    struct {
        uint8_t a;
        uint8_t b;
        uint8_t g;
        uint8_t r; 
    };

    uint32_t value;

};

struct Sample{
    int16_t leftChannel = 0;
    int16_t rightChannel = 0;
};

struct GameSoundOutput {
    uint32_t volume = 0;
    real32_t t = 0; //used for argument in sine
    uint32_t numSamples = 0;
    Sample samples[SOUND_FREQ];
};

struct OffScreenBuffer{
    Pixel* pixels = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t pitch = 0;
};

struct ButtonState {
    uint32_t halfTransitionCount = 0;
    bool isEndedDown = false;
};

struct GameMemory {
    void* permanentStorage = nullptr;
    uint64_t permanentStorageSize = 0;
    void* transientStorage = nullptr;
    uint64_t transientStorageSize = 0;

};

struct GameState {
    bool isInited = false;
    int blueOffset = 0;
    int greenOffset = 0;
    uint32_t tone = 0;
};

struct ControllerInput {
    bool isAnalog = false;

    real32_t avgX = 0.f; //average x stick postion
    real32_t avgY = 0.f; //average y stick postion

    union {
        ButtonState buttons[NUM_BUTTONS];

        struct {
            ButtonState directionUp;
            ButtonState directionDown;
            ButtonState directionLeft;
            ButtonState directionRight;

            ButtonState actionUp; //Y
            ButtonState actionDown; //A
            ButtonState actionLeft; //X
            ButtonState actionRight; //B

            ButtonState leftShoulder;
            ButtonState rightShoulder;

            ButtonState start;
            ButtonState back;

        };
    };

    ControllerInput() 
    :buttons{}
            
    {
    }

};


struct FileContents {
    void* contents = nullptr;
    uint64_t contentsSize = 0;
};


struct InputContext {
    ControllerInput controllers[MAX_CONTROLLERS];
};

void gameUpdateAndRender(GameMemory* memory, OffScreenBuffer *buffer, GameSoundOutput* sb, const InputContext* ci);


#ifdef HANDMADE_INTERNAL
FileContents debugFileRead(const char* fileName);
void debugFileWrite(const char* fileName, void* dataToWrite, uint64_t numBytesToWrite);
void debugFreeFileContents(FileContents* contents);
#endif

