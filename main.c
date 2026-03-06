// main.c - Simple 2D side-scrolling endless runner (SDL2)
// Linux Mint: sudo apt install build-essential libsdl2-dev
// Build: gcc -O2 -Wall -Wextra main.c -o scroll_game $(sdl2-config --cflags --libs)

#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define WIN_W 960
#define WIN_H 540

#define GROUND_Y (WIN_H - 90)
#define PLAYER_W 40
#define PLAYER_H 56

#define GRAVITY 2200.0f
#define JUMP_VEL 820.0f

#define SCROLL_SPEED 420.0f
#define OBSTACLE_W 36
#define OBSTACLE_MIN_H 28
#define OBSTACLE_MAX_H 120
#define OBSTACLE_GAP_MIN 240
#define OBSTACLE_GAP_MAX 520
#define MAX_OBS 12

typedef struct {
    float x;
    int h;
    bool active;
} Obstacle;

typedef struct {
    float y;
    float vy;
    bool on_ground;
} Player;

static int irand(int a, int b) {
    // inclusive [a, b]
    return a + (rand() % (b - a + 1));
}

static bool rects_intersect(SDL_Rect a, SDL_Rect b) {
    return !(a.x + a.w <= b.x || b.x + b.w <= a.x ||
             a.y + a.h <= b.y || b.y + b.h <= a.y);
}

static void reset_game(Player *p, Obstacle obs[], float *score, float *best) {
    if (*score > *best) *best = *score;
    *score = 0.0f;

    p->y = (float)(GROUND_Y - PLAYER_H);
    p->vy = 0.0f;
    p->on_ground = true;

    // Place obstacles off-screen with random gaps
    float x = (float)(WIN_W + 200);
    for (int i = 0; i < MAX_OBS; i++) {
        obs[i].active = true;
        obs[i].x = x;
        obs[i].h = irand(OBSTACLE_MIN_H, OBSTACLE_MAX_H);
        x += (float)irand(OBSTACLE_GAP_MIN, OBSTACLE_GAP_MAX);
    }
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    srand((unsigned)time(NULL));

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *win = SDL_CreateWindow(
        "C Side Scroll (SDL2) - Space to Jump, R to Restart, Esc to Quit",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIN_W, WIN_H, SDL_WINDOW_SHOWN
    );
    if (!win) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *ren = SDL_CreateRenderer(
        win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!ren) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    Player player;
    Obstacle obs[MAX_OBS];
    float score = 0.0f;
    float best = 0.0f;
    bool running = true;
    bool game_over = false;

    reset_game(&player, obs, &score, &best);

    Uint64 prev = SDL_GetPerformanceCounter();
    const double freq = (double)SDL_GetPerformanceFrequency();

    // Simple parallax ground tiles
    float ground_scroll = 0.0f;

    while (running) {
        // --- timing ---
        Uint64 now = SDL_GetPerformanceCounter();
        float dt = (float)((double)(now - prev) / freq);
        if (dt > 0.05f) dt = 0.05f; // clamp (avoid huge jump after pause)
        prev = now;

        // --- input ---
        SDL_Event e;
        bool jump_pressed = false;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN && !e.key.repeat) {
                SDL_Keycode k = e.key.keysym.sym;
                if (k == SDLK_ESCAPE) running = false;
                if (k == SDLK_SPACE || k == SDLK_w || k == SDLK_UP) jump_pressed = true;
                if (k == SDLK_r && game_over) {
                    game_over = false;
                    reset_game(&player, obs, &score, &best);
                }
            }
        }

        // --- update ---
        if (!game_over) {
            // Jump
            if (jump_pressed && player.on_ground) {
                player.vy = -JUMP_VEL;
                player.on_ground = false;
            }

            // Physics
            player.vy += GRAVITY * dt;
            player.y += player.vy * dt;

            float floor_y = (float)(GROUND_Y - PLAYER_H);
            if (player.y >= floor_y) {
                player.y = floor_y;
                player.vy = 0.0f;
                player.on_ground = true;
            }

            // Scroll obstacles
            for (int i = 0; i < MAX_OBS; i++) {
                if (!obs[i].active) continue;
                obs[i].x -= SCROLL_SPEED * dt;

                // recycle obstacle
                if (obs[i].x + OBSTACLE_W < 0) {
                    // find current max x among obstacles
                    float maxx = 0.0f;
                    for (int j = 0; j < MAX_OBS; j++) {
                        if (obs[j].active && obs[j].x > maxx) maxx = obs[j].x;
                    }
                    obs[i].x = maxx + (float)irand(OBSTACLE_GAP_MIN, OBSTACLE_GAP_MAX);
                    obs[i].h = irand(OBSTACLE_MIN_H, OBSTACLE_MAX_H);
                }
            }

            // Score increases with time survived and speed
            score += dt * 10.0f;

            // Ground parallax
            ground_scroll += SCROLL_SPEED * dt;
            if (ground_scroll > 60.0f) ground_scroll -= 60.0f;

            // Collision check
            SDL_Rect pr = { 160, (int)player.y, PLAYER_W, PLAYER_H };
            for (int i = 0; i < MAX_OBS; i++) {
                SDL_Rect orc = { (int)obs[i].x, GROUND_Y - obs[i].h, OBSTACLE_W, obs[i].h };
                if (rects_intersect(pr, orc)) {
                    game_over = true;
                    if (score > best) best = score;
                    break;
                }
            }
        }

        // --- render ---
        // background
        SDL_SetRenderDrawColor(ren, 18, 22, 32, 255);
        SDL_RenderClear(ren);

        // simple distant "hills"
        SDL_SetRenderDrawColor(ren, 35, 48, 66, 255);
        for (int i = 0; i < 8; i++) {
            int x = (i * 140) - ((int)(ground_scroll * 0.35f) % 140);
            SDL_Rect hill = { x, GROUND_Y - 160, 220, 140 };
            SDL_RenderFillRect(ren, &hill);
        }

        // ground
        SDL_SetRenderDrawColor(ren, 28, 110, 62, 255);
        SDL_Rect ground = { 0, GROUND_Y, WIN_W, WIN_H - GROUND_Y };
        SDL_RenderFillRect(ren, &ground);

        // ground tiles for motion feel
        SDL_SetRenderDrawColor(ren, 20, 85, 48, 255);
        for (int i = 0; i < 20; i++) {
            int x = (i * 60) - ((int)ground_scroll % 60);
            SDL_Rect tile = { x, GROUND_Y + 18, 42, 12 };
            SDL_RenderFillRect(ren, &tile);
        }

        // obstacles
        SDL_SetRenderDrawColor(ren, 210, 70, 70, 255);
        for (int i = 0; i < MAX_OBS; i++) {
            SDL_Rect orc = { (int)obs[i].x, GROUND_Y - obs[i].h, OBSTACLE_W, obs[i].h };
            SDL_RenderFillRect(ren, &orc);
        }

        // player (rectangle)
        SDL_Rect pr = { 160, (int)player.y, PLAYER_W, PLAYER_H };
        if (game_over) {
            SDL_SetRenderDrawColor(ren, 240, 210, 90, 255); // "hit" color
        } else {
            SDL_SetRenderDrawColor(ren, 90, 180, 240, 255);
        }
        SDL_RenderFillRect(ren, &pr);

        // "UI" bars (score + best) without font: show as bars
        // Score bar
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 80);
        SDL_Rect frame1 = { 16, 16, 240, 14 };
        SDL_RenderDrawRect(ren, &frame1);
        int w1 = (int)( (score / 200.0f) * (frame1.w - 2) );
        if (w1 < 0) w1 = 0;
        if (w1 > frame1.w - 2) w1 = frame1.w - 2;
        SDL_Rect fill1 = { frame1.x + 1, frame1.y + 1, w1, frame1.h - 2 };
        SDL_RenderFillRect(ren, &fill1);

        // Best bar
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 80);
        SDL_Rect frame2 = { 16, 36, 240, 14 };
        SDL_RenderDrawRect(ren, &frame2);
        int w2 = (int)( (best / 200.0f) * (frame2.w - 2) );
        if (w2 < 0) w2 = 0;
        if (w2 > frame2.w - 2) w2 = frame2.w - 2;
        SDL_Rect fill2 = { frame2.x + 1, frame2.y + 1, w2, frame2.h - 2 };
        SDL_RenderFillRect(ren, &fill2);

        // Game over indicator: flashing banner rectangle
        if (game_over) {
            Uint32 t = SDL_GetTicks();
            if ((t / 300) % 2 == 0) {
                SDL_SetRenderDrawColor(ren, 0, 0, 0, 160);
                SDL_Rect banner = { 0, WIN_H / 2 - 44, WIN_W, 88 };
                SDL_RenderFillRect(ren, &banner);

                SDL_SetRenderDrawColor(ren, 255, 255, 255, 200);
                SDL_Rect inner = { 120, WIN_H / 2 - 20, WIN_W - 240, 40 };
                SDL_RenderDrawRect(ren, &inner);
                // no font: just a visual cue.
            }
        }

        SDL_RenderPresent(ren);
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}