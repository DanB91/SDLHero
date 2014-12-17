#include <stdint.h>
#include <stdio.h>

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480

#define SOUND_FREQ 48000
#define NUM_CHANNELS 2
#define SOUND_LATENCY (SOUND_FREQ / 15)

#define MAX_CONTROLLERS 4

#define pi32 3.14159265358979f

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
    int16_t leftChannel;
    int16_t rightChannel;
};

struct SoundBuffer {
    uint32_t tone = 0;
    uint32_t volume = 0;
    real32_t t = 0; //used for argument in sine
    Sample samples[SOUND_FREQ];
    uint32_t numSamples = 0;
};

struct OffScreenBuffer{
    Pixel* pixels = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t pitch = 0;
};
void gameUpdateAndRender(OffScreenBuffer *buffer, int blueOffset, int greenOffset, SoundBuffer* sb);
