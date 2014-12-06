#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <stdint.h>

#if !defined(MAP_ANONYMOUS)
    
    #if defined(MAP_ANON)
        #define MAP_ANONYMOUS MAP_ANON
    #endif

#endif

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480

#define SOUND_FREQ 48000

#define pi32 3.14159265358979f
typedef union {
    struct {
        uint8_t a;
        uint8_t b;
        uint8_t g;
        uint8_t r; 
    };

    uint32_t value;

} Pixel;

typedef struct {
    Pixel* pixels;
    uint32_t sizeInBytes; //size of pixels in bytes
    SDL_Texture* sdlTexture;
    uint32_t width;
    uint32_t height;
} Texture;

typedef float real32_t;
typedef double real64_t;


static bool running = true;

static Texture gTexture;

static void printGeneralErrorAndExit(char* message) {
    fprintf(stderr, "Fatal Error: %s\n", message);
    exit(1);

}


static inline uint64_t
__rdtsc(void)
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

static void renderWeirdGradient(Texture* texture, int blueOffset, int greenOffset) {
    Pixel *pixels = texture->pixels;

    for (uint32_t y = 0; y < texture->height; y++) {

        Pixel *currPixel = pixels; 

        for (uint32_t x = 0; x < texture->width; x++) {
            currPixel->b = x + blueOffset;
            currPixel->g = y + greenOffset;

            currPixel++;
        }

        pixels += texture->width;
    }

}


//TODO: This function can both init and resize a texture.  Rename
//to something better.  Or, refactor
static void resizeTexture(Texture* texture, int newWidth, int newHeight,
        SDL_Renderer* renderer) {
    int sizeOfBuffer = newWidth * newHeight * sizeof(Pixel);

    if (texture->pixels) {
        munmap(texture->pixels, texture->sizeInBytes);

    }

    if(!(texture->pixels = mmap(NULL,
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
}

static void printSDLErrorAndExit(void) {
    fprintf(stderr, "Fatal SDL error. Error: %s\n", SDL_GetError());
    exit(1);
}

static void SDLAudioCallBack(void* userData, uint8_t* stream, int len) {
    int16_t* currSample = (int16_t*)stream;
    uint32_t tone = 250;
    uint32_t period = SOUND_FREQ / tone;
    uint32_t volume = 100;  
    static uint32_t runningIndex = 0;

    for (size_t i = 0; i < (len / sizeof(*currSample)); i+=2) {

        real32_t t = 2 * pi32 *  runningIndex++ / period;
        real32_t sineVal = sinf(t);

        //printf("%f\t%f\n", t, sineVal);

        int16_t halfSample = sineVal * volume;

        currSample[i] = halfSample * volume; //left channel 
        currSample[i+1] = halfSample * volume; //right channel
    }

}

static void initAudio(void) {
    SDL_AudioSpec desiredAudio = {};
    desiredAudio.channels = 2;
    desiredAudio.samples = 4096;
    desiredAudio.freq = SOUND_FREQ;
    desiredAudio.format = AUDIO_S16LSB;
    desiredAudio.callback = SDLAudioCallBack;

    if (SDL_OpenAudio(&desiredAudio, NULL) < 0) {
        printSDLErrorAndExit();
    }
}

static void initSDL(SDL_Window** window, SDL_Renderer** renderer) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
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

    resizeTexture(&gTexture, SCREEN_WIDTH, SCREEN_HEIGHT, *renderer);

    initAudio();

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
           // resizeTexture(&gTexture, we->data1, we->data2,
           //         SDL_GetRenderer(SDL_GetWindowFromID(we->windowID)));
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

    uint32_t xOffset = 0;
    uint32_t yOffset = 0;

    initSDL(&window, &renderer);
    uint64_t countFreq = SDL_GetPerformanceFrequency();
    uint64_t startCount = SDL_GetPerformanceCounter();
    uint64_t startCycles = __rdtsc();
   
    SDL_PauseAudio(0);
    while(running) {
        SDL_PollEvent(&e);
        processEvent(&e);
        renderWeirdGradient(&gTexture, xOffset, yOffset);
        updateWindow(window, gTexture);

        xOffset++;
        yOffset += 2;


        //benchmark stuff
        uint64_t endCount = SDL_GetPerformanceCounter();
        uint64_t endCycles = __rdtsc();

        real64_t secPerFrame = (real64_t)(endCount - startCount)/countFreq;
        real64_t fpsCount =  ((1./secPerFrame));
        real64_t mcPerFrame = (real64_t)(endCycles-startCycles) / (1000 * 1000 );

        //printf("TPF: %.2fms FPS: %.2f MCPF: %.2f\n", secPerFrame*1000, fpsCount, mcPerFrame);


        startCount = endCount;
        startCycles = endCycles;

    }

    cleanUp();
    return 0;
}
