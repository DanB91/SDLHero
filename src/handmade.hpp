#include <stdint.h>
#include <stdio.h>

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480

#define SOUND_FREQ 48000
#define NUM_CHANNELS 2
#define SOUND_LATENCY (SOUND_FREQ / 15)

#define MAX_CONTROLLERS 4

#define pi32 3.14159265358979f

#define NUM_BUTTONS 6

#define arraySize(array) (sizeof(array) / sizeof((array)[0]))

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

        };
    };

    ControllerInput() 
    :buttons{}
            
    {
    }

};
void gameUpdateAndRender(OffScreenBuffer *buffer, GameSoundOutput* sb, const ControllerInput* ci);
