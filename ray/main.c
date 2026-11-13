#include "raylib.h"
#include "motor.h"
#include <math.h>
#include <time.h>

// Layout com nova biblioteca 
#define LARGURA_JANELA 1376
#define ALTURA_JANELA  768
#define TAM            36                                 /* lado da célula em px */
#define TAB_W          (larguraTab * TAM)
#define TAB_H          (alturaTab * TAM)
#define TAB_X          ((LARGUR
    A_JANELA - TAB_W) / 2)
#define TAB_Y          ((ALTURA_JANELA - TAB_H) / 2)
#define PAINEL_X       980
#define PAINEL_Y       40
 
static const Color MASSA  = {214, 160, 70, 255};
static const Color QUEIJO = {245, 200, 60, 255};
static const Color PEPPERONI = {200, 40, 30, 255};

/* centro_graus: direção do meio da parte que existe (0 = direita, 180 = esquerda) */
static void desenhar_pizza(float cx, float cy, float raio, int pedacos, float centro_graus) {
    Vector2 c = {cx, cy};
    if (pedacos <= 0) return;

    float ini = 0.0f, fim = 360.0f;
    if (pedacos < PIZZA_INTEIRA) {
        ini = centro_graus - pedacos * 30.0f;
        fim = centro_graus + pedacos * 30.0f;
    }

    DrawCircleSector(c, raio, ini, fim, 32, MASSA);
    DrawCircleSector(c, raio * 0.82f, ini, fim, 32, QUEIJO);

    /* "pepperonis": um no meio de cada fatia */
    for (int i = 0; i < pedacos; i++) {
        float ang = (ini + i * 60.0f + 30.0f) * DEG2RAD;
        DrawCircle(cx + cosf(ang) * raio * 0.5f, cy + sinf(ang) * raio * 0.5f,
                   raio * 0.12f, PEPPERONI);
    }
}

static const Color CAIXA        = {200, 165, 120, 255};
static const Color CAIXA_ESCURA = {150, 115, 75, 255};

static void desenhar_caixa(float cx, float cy, float lado) {
    float x = cx - lado / 2.0f, y = cy - lado / 2.0f;
    DrawRectangle(x, y, lado, lado, CAIXA);
    DrawRectangle(x + lado * 0.1f, y + lado * 0.1f, lado * 0.8f, lado * 0.8f, CAIXA_ESCURA);
    DrawRectangleLinesEx((Rectangle){x, y, lado, lado}, 2, BROWN);
}







// MAIN 
int main(void) {
    InitWindow(LARGURA_JANELA, ALTURA_JANELA, "Pizzalign");
    SetExitKey(KEY_Q);
    SetTargetFPS(60);

    static const struct { int ped; float centro; } casos[] = {
        {1, 180}, {1, 0}, {3, 0}, {3, 180}, {5, 0}, {5, 180}, {6, 0}
    };

    while (!WindowShouldClose()) {
        BeginDrawing();
            ClearBackground((Color){30, 24, 20, 255});
            for (int i = 0; i < 7; i++)
                desenhar_pizza(150 + (i % 4) * 200, 150 + (i / 4) * 200, 60,
                               casos[i].ped, casos[i].centro);
            desenhar_caixa(150 + 3 * 200, 150 + 1 * 200, 100);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}