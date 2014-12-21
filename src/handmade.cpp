#include <math.h>
#include "handmade.hpp"

static void renderWeirdGradient(OffScreenBuffer *buf, int blueOffset, int greenOffset) {
    Pixel *pixels = buf->pixels;

    for (uint32_t y = 0; y < buf->height; y++) {

        Pixel *currPixel = pixels; 

        for (uint32_t x = 0; x < buf->width; x++) {
            currPixel->b = x + blueOffset;
            currPixel->g = y + greenOffset;

            currPixel++;
        }

        pixels += buf->width;
    }
}


static void outputSound(GameSoundOutput* sb, uint32_t tone) {

    real32_t period = SOUND_FREQ / tone;



    for (uint32_t i = 0; i < sb->numSamples; i++) {

        sb->t += 2 * pi32 / period;
        real32_t sineVal = sinf(sb->t);

        int16_t sample = sineVal * sb->volume;

        sb->samples[i].leftChannel = sample;  //left channel 
        sb->samples[i].rightChannel = sample; //right channel

    }

}

void gameUpdateAndRender(OffScreenBuffer *buf, GameSoundOutput* sb, const ControllerInput* ci) {
    static int blueOffset = 0;
    static int greenOffset = 0;
    static uint32_t tone = 0;
    
    tone = 512 + (int)(256.0f*ci->avgY);
    blueOffset += ci->avgX * 4;
    greenOffset += ci->avgY * 4 ;

    outputSound(sb, tone);
    renderWeirdGradient(buf, blueOffset, greenOffset);
}
