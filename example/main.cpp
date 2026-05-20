#ifdef HUI_USE_SDL1
#include <SDL.h>
#else
#include <SDL.h>
#endif

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

#ifdef HUI_USE_SDL1
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        return 1;
    }
    SDL_Surface* screen = SDL_SetVideoMode(640, 480, 32, SDL_SWSURFACE);
    if (!screen) {
        SDL_Quit();
        return 1;
    }
    
    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = false;
            }
        }
        SDL_Delay(16);
    }
    SDL_Quit();
#else
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        return 1;
    }
    SDL_Window* window = SDL_CreateWindow("LHCUI Example", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, SDL_WINDOW_SHOWN);
    if (!window) {
        SDL_Quit();
        return 1;
    }
    
    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = false;
            }
        }
        SDL_Delay(16);
    }
    SDL_DestroyWindow(window);
    SDL_Quit();
#endif
    return 0;
}
