#include <grrlib.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <wiiuse/wpad.h>
#include <asndlib.h>
#include <stdbool.h>
#include <ogc/gx.h> 

// --- TUS INCLUDES ---
#include "player_idle_png.h"
#include "player_jump_png.h"
#include "player_walk1_png.h"
#include "player_walk2_png.h"
#include "spikes_png.h" 

// --- SONIDOS ---
#include "sfx_jump_wav.h"
#include "sfx_death_wav.h"
#include "sfx_spawn_wav.h"       
#include "sfx_walk_start_wav.h"  
#include "sfx_step_wav.h"
#include "sfx_trap_wav.h"

// --- COLORES ---
#define COL_DEVIL_RED    0x581308FF 
#define COL_DEVIL_EYES   0xCC4A26FF
#define COL_MENU_YELLOW  0xFDBA4DFF
#define COL_LOADING_BG   0x2E0B05FF
#define COL_LOADING_BAR  0xCC4A26FF
#define COL_BTN_1P_BG    0xFFF176FF
#define COL_BTN_2P_BG    0xE57373FF

#define COLOR_HITBOX     0x00000000 
#define COLOR_BG_OUTER   0x9C710AFF
#define COLOR_LEVEL_BG   0xFDBA4DFF
#define COLOR_FLOOR      0x9C710AFF
#define COLOR_DOOR_FRAME 0x9C710AFF 
#define COLOR_DOOR_HOLE  0x909090FF
#define COLOR_NO_TINT    0xFFFFFFFF
#define COLOR_PARTICLE   0x000000FF

// --- ESTADOS ---
enum { SCENE_LOADING, SCENE_MENU, SCENE_TRANSITION_IN, SCENE_GAMEPLAY };

// --- CONFIGURACION ---
#define TRAP_SPEED       6.0f
#define ANIM_SPEED       0.15f
#define SPEED            3.0f
#define GAME_SPAWNING    0 
#define GAME_PLAYING     1 
#define GAME_DYING       2 
#define GAME_WAITING     3 
#define ST_IDLE    0
#define ST_RUN     1
#define ST_JUMP    2
#define DIR_RIGHT  0
#define DIR_LEFT   1
#define GRAVITY 0.35f  
#define JUMP_FORCE -6.5f
#define PLAYER_SCALE 1.0f
#define NUM_BLOQUES 12
#define NUM_PARTICLES 12 
#define NUM_SPIKES 1 
#define AJUSTE_Y  -16.0f
#define AJUSTE_X_DERECHA  -17.0f
#define AJUSTE_X_IZQUIERDA -15.0f

// --- STRUCTS ---
typedef struct { float x, y, vy; int w, h; bool isGrounded; int state; int direction; float animTimer; float stepTimer; } Player;
typedef struct { float x, y, w, h; bool active; bool isVisible; float original_y; } Block;
typedef struct { float x, y; int w, h; bool active; } Spike;
typedef struct { float x, y, vx, vy, w, h; bool active; float targetX, targetY, startX, startY; int lifeTime; } Particle;
typedef struct { float x, y, w, h; u32 colorBg; bool selected; float scale; } Button;

// --- GLOBALES ---
Particle particles[NUM_PARTICLES];
float g_box_x = 0, g_box_w = 0;
int currentScene = SCENE_LOADING;
float transitionProgress = 0.0f;
Button btn1P, btn2P;
float loadingBarProgress = 0.0f;
int menuSelection = 0; // 0 = 1P, 1 = 2P (Controlado por flechas o IR)

// --- FUNCIONES AUXILIARES ---

void DrawTriangle(float x1, float y1, float x2, float y2, float x3, float y3, u32 color) {
    GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 3);
        GX_Position3f32(x1, y1, 0); GX_Color1u32(color);
        GX_Position3f32(x2, y2, 0); GX_Color1u32(color);
        GX_Position3f32(x3, y3, 0); GX_Color1u32(color);
    GX_End();
}

void PlaySound(const u8 *snd, u32 len) {
    int voice = ASND_GetFirstUnusedVoice();
    if (voice >= 0) ASND_SetVoice(voice, VOICE_MONO_16BIT_LE, 48000, 0, (void*)(snd + 44), len - 44, 255, 255, NULL);
}

bool CheckCollisionGeneric(float px, float py, int pw, int ph, float bx, float by, int bw, int bh) {
    return (px < bx + bw && px + pw > bx && py < by + bh && py + ph > by);
}
bool CheckPointCollision(float px, float py, float bx, float by, float bw, float bh) {
    return (px >= bx && px <= bx + bw && py >= by && py <= by + bh);
}

void InitExplosion(float startX, float startY) {
    float visualY = (startY > 440) ? 440 : startY - 10; 
    for(int i=0; i<NUM_PARTICLES; i++) {
        particles[i].active = true; 
        particles[i].w = 3 + (rand() % 4); particles[i].h = 3 + (rand() % 4);
        particles[i].x = startX+16+((rand()%16)-8); particles[i].y = visualY+16+((rand()%16)-8);
        if (g_box_w > 0) {
            if(particles[i].x < g_box_x + 5) particles[i].x = g_box_x + 5;
            if(particles[i].x > g_box_x + g_box_w - 10) particles[i].x = g_box_x + g_box_w - 10;
        }
        particles[i].vy = -((rand()%100) + 120) / 10.0f; particles[i].vx = ((rand()%200)-100) / 10.0f; 
        particles[i].lifeTime = 60; 
    }
}

void InitSpawnParticles(float targetX, float targetY) {
    for(int i=0; i<NUM_PARTICLES; i++) {
        particles[i].active = true; 
        particles[i].w = 3 + (rand() % 4); particles[i].h = 3 + (rand() % 4);
        particles[i].targetX = targetX+12+(rand()%8); particles[i].targetY = targetY+12+(rand()%8);
        particles[i].x = targetX+((rand()%140)-70); particles[i].y = targetY+((rand()%140)-70);
        particles[i].startX = particles[i].x; particles[i].startY = particles[i].y;
        particles[i].lifeTime = 1000; 
    }
}

// --- DIBUJADO ARTE ---
void DrawDevilEyes(float xOffset, float yOffset) {
    DrawTriangle(180+xOffset, 150+yOffset, 280+xOffset, 200+yOffset, 160+xOffset, 100+yOffset, COL_DEVIL_EYES);
    GRRLIB_Rectangle(200+xOffset, 150+yOffset, 100, 40, COL_DEVIL_EYES, 1);
    
    DrawTriangle(460+xOffset, 150+yOffset, 360+xOffset, 200+yOffset, 480+xOffset, 100+yOffset, COL_DEVIL_EYES);
    GRRLIB_Rectangle(340+xOffset, 150+yOffset, 100, 40, COL_DEVIL_EYES, 1);
}

// NUEVA LOGICA DE DIENTES: Mas irregulares y menos espacio rojo
void DrawIrregularTeeth(float yPos, bool pointingDown) {
    srand(42); // Semilla fija para que no parpadee
    float currentX = 0;
    
    while(currentX < 640) {
        // Ancho MUY variado (Fino vs Grueso)
        float tWidth = 8 + (rand() % 35); 
        // Alto MUY variado (Cortos vs Largos tipo aguja)
        float tHeight = 20 + (rand() % 90); 

        float x1 = currentX;
        float x2 = currentX + tWidth;
        // Punta irregular
        float x3 = currentX + (tWidth / 2.0f) + ((rand()%14)-7); 

        float yBase = yPos;
        float yTip = pointingDown ? (yPos + tHeight) : (yPos - tHeight);

        DrawTriangle(x1, yBase, x2, yBase, x3, yTip, COL_DEVIL_RED);
        
        currentX += tWidth;
    }
    srand(time(NULL)); 
}

void DrawLoadingScreen() {
    GRRLIB_FillScreen(COL_DEVIL_RED);
    DrawDevilEyes(0, 0);
    float barW = 300, barH = 20;
    float barX = (640 - barW) / 2, barY = 380;
    GRRLIB_Rectangle(barX, barY, barW, barH, COL_LOADING_BG, 1); 
    GRRLIB_Rectangle(barX, barY, barW * loadingBarProgress, barH, COL_LOADING_BAR, 1);
}

void DrawStyledButton(Button btn, bool isTwoPlayer) {
    float s = btn.scale;
    float w = btn.w * s;
    float h = btn.h * s;
    float x = btn.x - (w - btn.w)/2; 
    float y = btn.y - (h - btn.h)/2;

    float borderSize = 6.0f;
    GRRLIB_Rectangle(x - borderSize, y - borderSize, w + (borderSize*2), h + (borderSize*2), COL_DEVIL_RED, 1);

    float bottomBarHeight = 30.0f * s;
    GRRLIB_Rectangle(x, y, w, h - bottomBarHeight, btn.colorBg, 1);
    GRRLIB_Rectangle(x, y + h - bottomBarHeight, w, bottomBarHeight, COL_DEVIL_RED, 1);

    if (!isTwoPlayer) {
        GRRLIB_Rectangle(x + (35*s), y + (20*s), 30*s, 30*s, 0x000000FF, 1); 
        GRRLIB_Rectangle(x + (20*s), y + (50*s), 60*s, 30*s, 0x000000FF, 1); 
        GRRLIB_Rectangle(x + (10*s), y + h - (20*s), 10*s, 10*s, COL_BTN_1P_BG, 1); 
        GRRLIB_Rectangle(x + (25*s), y + h - (20*s), 40*s, 10*s, COL_BTN_1P_BG, 1);
        GRRLIB_Rectangle(x + (70*s), y + h - (20*s), 20*s, 10*s, COL_BTN_1P_BG, 1);
    } else {
        GRRLIB_Rectangle(x + (15*s), y + (25*s), 20*s, 20*s, 0x5D4037FF, 1); 
        GRRLIB_Rectangle(x + (5*s), y + (45*s), 40*s, 20*s, 0x5D4037FF, 1);
        GRRLIB_Rectangle(x + (65*s), y + (25*s), 20*s, 20*s, 0x3E2723FF, 1); 
        GRRLIB_Rectangle(x + (55*s), y + (45*s), 40*s, 20*s, 0x3E2723FF, 1);
        GRRLIB_Rectangle(x + (10*s), y + h - (20*s), 15*s, 10*s, COL_BTN_2P_BG, 1); 
        GRRLIB_Rectangle(x + (30*s), y + h - (20*s), 40*s, 10*s, COL_BTN_2P_BG, 1);
        GRRLIB_Rectangle(x + (75*s), y + h - (20*s), 15*s, 10*s, COL_BTN_2P_BG, 1);
    }
}

void DrawMenuScreen() {
    GRRLIB_FillScreen(COL_MENU_YELLOW);
    GRRLIB_Rectangle(0, 0, 640, 240, COL_DEVIL_RED, 1);

    srand(100); 
    for(int i=0; i<640; i+=20) {
        DrawTriangle(i, 240, i+20, 240, i+10, 210 + (rand()%15), COL_MENU_YELLOW);
    }
    srand(time(NULL));

    GRRLIB_Rectangle(120, 50, 400, 80, 0x00000040, 1); 
    GRRLIB_Rectangle(140, 60, 360, 60, COL_DEVIL_EYES, 1); 

    DrawStyledButton(btn1P, false);
    DrawStyledButton(btn2P, true);
}

void DrawTransitionEffect() {
    // Calculo para que la boca cierre justo al centro y no tape de mas con bloque rojo
    float screenH = 480.0f;
    float halfH = screenH / 2.0f; // 240
    
    // Progreso: 0.0 -> Abierto, 1.0 -> Cerrado al centro
    float currentClose = halfH * transitionProgress; 
    
    // Dibujamos el bloque rojo superior, pero solo lo necesario
    // Top Jaw
    float jawTopY = currentClose - 300; // -300 para que el bloque rojo este mayormente fuera
    if (jawTopY > 0) jawTopY = 0; // Limite
    
    GRRLIB_Rectangle(0, 0, 640, currentClose, COL_DEVIL_RED, 1); // Bloque solido hasta el cierre
    DrawDevilEyes(0, currentClose - 250); // Ojos bajando
    DrawIrregularTeeth(currentClose, true); // Dientes en el borde

    // Bottom Jaw
    float bottomY = screenH - currentClose;
    GRRLIB_Rectangle(0, bottomY, 640, currentClose, COL_DEVIL_RED, 1);
    DrawIrregularTeeth(bottomY, false); 
}

void DrawCursor(float x, float y) {
    DrawTriangle(x+2, y+2, x+2, y+22, x+17, y+17, 0x00000080);
    DrawTriangle(x, y, x, y+20, x+15, y+15, 0xFFFFFFFF);
}

// ================= MAIN =================
int main(int argc, char **argv) {
    GRRLIB_Init();
    WPAD_Init();
    WPAD_SetDataFormat(WPAD_CHAN_0, WPAD_FMT_BTNS_ACC_IR);
    ASND_Init();
    ASND_Pause(0);
    srand(time(NULL)); 

    GRRLIB_texImg *tex_idle  = GRRLIB_LoadTexture(player_idle_png);
    GRRLIB_texImg *tex_jump  = GRRLIB_LoadTexture(player_jump_png);
    GRRLIB_texImg *tex_walk1 = GRRLIB_LoadTexture(player_walk1_png);
    GRRLIB_texImg *tex_walk2 = GRRLIB_LoadTexture(player_walk2_png);
    GRRLIB_texImg *tex_spikes = GRRLIB_LoadTexture(spikes_png);

    if (tex_idle)  GRRLIB_SetHandle(tex_idle, 16, 16);
    if (tex_jump)  GRRLIB_SetHandle(tex_jump, 16, 16);
    if (tex_walk1) GRRLIB_SetHandle(tex_walk1, 16, 16);
    if (tex_walk2) GRRLIB_SetHandle(tex_walk2, 16, 16);
    if (tex_spikes) GRRLIB_SetHandle(tex_spikes, 24, 16);

    float box_w = 600, box_h = 180;
    float box_x = (640 - box_w)/2, box_y = (480 - box_h)/2;
    g_box_x = box_x; g_box_w = box_w;
    float floor_h = 40;
    float floor_y = box_y + box_h - floor_h;

    Player p = {box_x + 30, box_y + 50, 0, 10, 32, false, ST_IDLE, DIR_RIGHT, 0.0f, 0.0f};

    Block nivel[NUM_BLOQUES];
    Spike spikes[NUM_SPIKES];
    bool trap1_triggered = false, trap2_triggered = false;

    nivel[0] = (Block){box_x, floor_y, 330, floor_h, true, true, floor_y};
    nivel[1] = (Block){box_x+330, floor_y, 40, floor_h, true, true, floor_y}; 
    nivel[2] = (Block){box_x+370, floor_y, 50, floor_h, true, true, floor_y};
    nivel[3] = (Block){box_x+420, floor_y, 40, floor_h, true, true, floor_y}; 
    nivel[4] = (Block){box_x+460, floor_y, 140, floor_h, true, true, floor_y};
    nivel[5] = (Block){box_x-20, box_y, 20, box_h, true, true, box_y}; 
    nivel[6] = (Block){box_x+box_w, box_y, 20, box_h, true, true, box_y}; 
    nivel[7] = (Block){box_x, box_y-20, box_w, 20, true, true, box_y-20}; 
    nivel[8] = (Block){box_x, floor_y+floor_h, 330, 400, true, false, 0};
    nivel[9] = (Block){box_x+370, floor_y+floor_h, 50, 400, true, false, 0};
    nivel[10] = (Block){box_x+460, floor_y+floor_h, 140, 400, true, false, 0};
    nivel[11] = (Block){0, 580, 640, 50, true, false, 0}; 
    spikes[0] = (Spike){ 105.0f, 281.0f, 59, 12, true };

    int gameState = GAME_SPAWNING;
    int spawnTimer = 0;

    btn1P.w = 110; btn1P.h = 110; btn1P.x = 180; btn1P.y = 280; 
    btn1P.colorBg = COL_BTN_1P_BG; btn1P.scale = 1.0f;
    btn2P.w = 110; btn2P.h = 110; btn2P.x = 360; btn2P.y = 280; 
    btn2P.colorBg = COL_BTN_2P_BG; btn2P.scale = 1.0f;

    ir_t ir; 

    while(1) {
        WPAD_ScanPads();
        u32 down = WPAD_ButtonsDown(0);
        u32 held = WPAD_ButtonsHeld(0);
        WPAD_IR(0, &ir); 

        if (down & WPAD_BUTTON_HOME) break;

        if (currentScene == SCENE_LOADING) {
            loadingBarProgress += 0.002f; 
            DrawLoadingScreen();
            if (loadingBarProgress >= 1.0f) {
                loadingBarProgress = 1.0f;
                currentScene = SCENE_MENU;
            }
        }
        else if (currentScene == SCENE_MENU) {
            DrawMenuScreen();
            
            // --- LOGICA DE SELECCION UNIFICADA (IR + PAD) ---
            bool usingIR = false;
            if (ir.valid) {
                // Si el IR apunta a un boton, tomamos control
                float m = 10.0f;
                if (CheckPointCollision(ir.x, ir.y, btn1P.x - m, btn1P.y - m, btn1P.w + m*2, btn1P.h + m*2)) {
                    menuSelection = 0; usingIR = true;
                }
                if (CheckPointCollision(ir.x, ir.y, btn2P.x - m, btn2P.y - m, btn2P.w + m*2, btn2P.h + m*2)) {
                    menuSelection = 1; usingIR = true;
                }
                DrawCursor(ir.x, ir.y);
            }

            // El pad sobrescribe si se pulsa
            if (down & WPAD_BUTTON_LEFT) menuSelection = 0;
            if (down & WPAD_BUTTON_RIGHT) menuSelection = 1;

            // Feedback visual
            if (menuSelection == 0) btn1P.scale = 1.1f; else btn1P.scale = 1.0f;
            if (menuSelection == 1) btn2P.scale = 1.1f; else btn2P.scale = 1.0f;

            // Click Logic (Boton A)
            if (down & WPAD_BUTTON_A) {
                if (menuSelection == 0) {
                    btn1P.scale = 0.9f; 
                    PlaySound(sfx_spawn_wav, sfx_spawn_wav_size); 
                    // TRANSICION INMEDIATA
                    currentScene = SCENE_TRANSITION_IN;
                    transitionProgress = 0.0f;
                }
                if (menuSelection == 1) { 
                    btn2P.scale = 0.9f; 
                    // 2 Players no hace nada
                }
            }
        }
        else if (currentScene == SCENE_TRANSITION_IN) {
            DrawMenuScreen(); 
            // Velocidad de animacion boca
            transitionProgress += 0.02f; 
            DrawTransitionEffect();

            // Si cierra (valor > 1.0), empezamos juego
            if (transitionProgress >= 1.2f) {
                currentScene = SCENE_GAMEPLAY;
                gameState = GAME_SPAWNING;
                InitSpawnParticles(p.x, p.y);
                PlaySound(sfx_spawn_wav, sfx_spawn_wav_size);
                transitionProgress = 1.0f; 
            }
        }
        else if (currentScene == SCENE_GAMEPLAY) {
             if (gameState == GAME_SPAWNING) {
                spawnTimer++;
                if(spawnTimer < 25) { for(int i=0;i<NUM_PARTICLES;i++) { particles[i].x+=((rand()%3)-1)*0.5f; particles[i].y+=((rand()%3)-1)*0.5f; } }
                else { bool ok=true; for(int i=0;i<NUM_PARTICLES;i++){ particles[i].x+=(particles[i].targetX-particles[i].x)*0.25f; particles[i].y+=(particles[i].targetY-particles[i].y)*0.25f; if(abs(particles[i].x-particles[i].targetX)>2)ok=false; } if(spawnTimer>45&&ok) gameState=GAME_PLAYING; }
            }
            else if (gameState == GAME_PLAYING) {
                if (p.isGrounded) p.state = ST_IDLE;
                bool isMoving = false;
                if (held & WPAD_BUTTON_DOWN) { p.x += SPEED; p.direction = DIR_RIGHT; if (p.isGrounded) p.state = ST_RUN; isMoving = true; }
                if (held & WPAD_BUTTON_UP) { p.x -= SPEED; p.direction = DIR_LEFT; if (p.isGrounded) p.state = ST_RUN; isMoving = true; }
                if ((down & WPAD_BUTTON_2) && p.isGrounded) { p.vy = JUMP_FORCE; p.isGrounded = false; PlaySound(sfx_jump_wav, sfx_jump_wav_size); }

                if (isMoving && p.isGrounded) { p.stepTimer+=1.0f; if(p.stepTimer>15.0f){PlaySound(sfx_step_wav, sfx_step_wav_size); p.stepTimer=0;} } else p.stepTimer=15.0f;
                if (p.x < box_x) p.x = box_x; 
                if (p.x > box_x + box_w - p.w) p.x = box_x + box_w - p.w;
                p.vy += GRAVITY; p.y += p.vy; if (!p.isGrounded) p.state = ST_JUMP;

                if (p.x > (box_x+290) && !trap1_triggered) { trap1_triggered = true; PlaySound(sfx_trap_wav, sfx_trap_wav_size); }
                if (p.x > (box_x+420) && p.x < (box_x+460) && !trap2_triggered) { trap2_triggered = true; PlaySound(sfx_trap_wav, sfx_trap_wav_size); }
                if (trap1_triggered) { nivel[1].y += TRAP_SPEED; if(nivel[1].y>600) nivel[1].active=false; }
                if (trap2_triggered) { nivel[3].y += TRAP_SPEED; if(nivel[3].y>600) nivel[3].active=false; }

                p.isGrounded = false;
                for(int i=0; i<NUM_BLOQUES; i++) {
                    if(nivel[i].active && CheckCollisionGeneric(p.x, p.y, p.w, p.h, nivel[i].x, nivel[i].y, nivel[i].w, nivel[i].h)) {
                        if (p.vy > 0 && p.y + p.h < nivel[i].y + p.vy + 10) { p.y = nivel[i].y - p.h; p.vy = 0; p.isGrounded = true; }
                        else if (p.vy < 0 && p.y > nivel[i].y + nivel[i].h + p.vy - 5) { p.y = nivel[i].y + nivel[i].h; p.vy = 0; }
                        else { if (p.x+p.w/2 < nivel[i].x+nivel[i].w/2) p.x=nivel[i].x-p.w; else p.x=nivel[i].x+nivel[i].w; }
                    }
                }
                bool spikeHit = false;
                for(int i=0; i<NUM_SPIKES; i++) {
                    if(spikes[i].active && CheckCollisionGeneric(p.x, p.y, p.w, p.h, spikes[i].x, spikes[i].y, spikes[i].w, spikes[i].h)) { spikeHit = true; break; }
                }
                if (p.y > 480 || spikeHit) { PlaySound(sfx_death_wav, sfx_death_wav_size); InitExplosion(p.x, p.y); gameState = GAME_DYING; spawnTimer = 0; }
            }
            else if (gameState == GAME_DYING) {
                spawnTimer++;
                for(int i=0; i<NUM_PARTICLES; i++) {
                    if (!particles[i].active) continue;
                    particles[i].vy += GRAVITY; particles[i].y += particles[i].vy;
                    if(particles[i].y > 600) particles[i].active = false;
                    particles[i].x += particles[i].vx;
                }
                if (spawnTimer > 180) gameState = GAME_WAITING;
            }
            else if (gameState == GAME_WAITING) {
                if (down & (WPAD_BUTTON_A | WPAD_BUTTON_B | WPAD_BUTTON_1 | WPAD_BUTTON_2 | WPAD_BUTTON_PLUS)) {
                    PlaySound(sfx_walk_start_wav, sfx_walk_start_wav_size); PlaySound(sfx_spawn_wav, sfx_spawn_wav_size); 
                    p.x = box_x + 30; p.y = box_y + 50; p.vy = 0; p.state = ST_IDLE; trap1_triggered = false; trap2_triggered = false; nivel[1].y = nivel[1].original_y; nivel[1].active = true; nivel[3].y = nivel[3].original_y; nivel[3].active = true; InitSpawnParticles(p.x, p.y); gameState = GAME_SPAWNING; spawnTimer = 0;
                }
            }

            GRRLIB_FillScreen(COLOR_BG_OUTER);
            GRRLIB_Rectangle(box_x, box_y, box_w + 20, box_h, COLOR_LEVEL_BG, 1);
            if (trap1_triggered && nivel[1].y - floor_y > 0) GRRLIB_Rectangle(nivel[1].x, floor_y, nivel[1].w, nivel[1].y - floor_y, COLOR_LEVEL_BG, 1);
            if (trap2_triggered && nivel[3].y - floor_y > 0) GRRLIB_Rectangle(nivel[3].x, floor_y, nivel[3].w, nivel[3].y - floor_y, COLOR_LEVEL_BG, 1);
            for(int i=0; i<NUM_BLOQUES; i++) if (nivel[i].isVisible && nivel[i].active && nivel[i].y < 550) GRRLIB_Rectangle(nivel[i].x, nivel[i].y, nivel[i].w, nivel[i].h, COLOR_FLOOR, 1);
            for(int i=0; i<NUM_SPIKES; i++) if (spikes[i].active) { GRRLIB_Rectangle(spikes[i].x, spikes[i].y, spikes[i].w, spikes[i].h, COLOR_HITBOX, 1); if (tex_spikes) GRRLIB_DrawImg(spikes[i].x + 5.0f, spikes[i].y - 7.0f, tex_spikes, 0, 1.5f, 1.5f, COLOR_NO_TINT); }
            
            float door_x = (box_x + box_w) - 90;
            GRRLIB_Rectangle(door_x + 4, floor_y - 52, 27, 2, COLOR_DOOR_FRAME, 1);
            GRRLIB_Rectangle(door_x + 2, floor_y - 50, 31, 2, COLOR_DOOR_FRAME, 1);
            GRRLIB_Rectangle(door_x, floor_y - 48, 35, 48, COLOR_DOOR_FRAME, 1);
            GRRLIB_Rectangle(door_x + 8, floor_y - 46, 19, 2, COLOR_DOOR_HOLE, 1);
            GRRLIB_Rectangle(door_x + 6, floor_y - 44, 23, 2, COLOR_DOOR_HOLE, 1);
            GRRLIB_Rectangle(door_x + 4, floor_y - 42, 27, 42, COLOR_DOOR_HOLE, 1);

            if (gameState == GAME_PLAYING) {
                GRRLIB_texImg *curr = tex_idle;
                if (p.state == ST_JUMP) curr = tex_jump; else if (p.state == ST_RUN) { p.animTimer += ANIM_SPEED; curr = (fmod(p.animTimer, 2.0f) < 1.0f) ? tex_walk1 : tex_walk2; } else { p.animTimer = 0; curr = tex_idle; }
                if (curr) { float sx = (p.direction == DIR_RIGHT) ? PLAYER_SCALE : -PLAYER_SCALE; GRRLIB_DrawImg(p.x+(p.w/2)+(p.direction==DIR_RIGHT?AJUSTE_X_DERECHA:AJUSTE_X_IZQUIERDA), p.y+(p.h/2)+AJUSTE_Y, curr, 0, sx, PLAYER_SCALE, COLOR_NO_TINT); }
            }
            if (gameState != GAME_PLAYING) for(int i=0; i<NUM_PARTICLES; i++) if(particles[i].active) GRRLIB_Rectangle(particles[i].x, particles[i].y, particles[i].w, particles[i].h, COLOR_PARTICLE, 1);
        }

        GRRLIB_Render();
    }
    
    if(tex_idle) GRRLIB_FreeTexture(tex_idle); 
    if(tex_jump) GRRLIB_FreeTexture(tex_jump); 
    if(tex_walk1) GRRLIB_FreeTexture(tex_walk1); 
    if(tex_walk2) GRRLIB_FreeTexture(tex_walk2); 
    if(tex_spikes) GRRLIB_FreeTexture(tex_spikes);
    
    GRRLIB_Exit();
    return 0;
}