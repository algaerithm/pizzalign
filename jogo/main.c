/*
 * PIZZALIGN - main.c  (interface em raylib; as regras e a matematica ficam no motor.c)
 *
 * Compilar no Windows (MinGW), dentro da pasta do jogo:
 *   gcc main.c motor.c -o pizzalign.exe -I ../../raylib/src -L ../../raylib/src -lraylib -lopengl32 -lgdi32 -lwinmm -lm
 * Compilar no Linux:
 *   gcc main.c motor.c -o pizzalign -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
 *
 * assets_embutidos.h guarda a logo, as cartas e as fontes dentro do codigo
 * (para trocar algum arquivo de assets/, rode gerar_embutidos.py e compile de novo).
 *
 * Dois jogadores se revezam, uma carta cada vez:
 *   Jogador 1 (comeca): A / D = mover | S = descer mais rapido
 *   Jogador 2         : seta esquerda / direita = mover | seta para baixo = descer
 *   P = pausa | M = menu | Q = sair | N = som ligado/desligado | F11 = tela cheia
 *   Tudo que aparece como botao ou tecla na tela tambem pode ser clicado com o mouse.
 *
 * Regras (estao no motor.c): a carta pousa e empilha. Ao pousar, se a vizinha de baixo, da esquerda
 * ou da direita (de qualquer jogador) somar 6 com ela, vale 3 pontos; se for igual, vale 1 ponto.
 * Quando pontua, as duas cartas se fundem e somem; senao, so acumulam. Tomate nunca pontua.
 *
 * Mapa do arquivo: 1) recursos  2) texto  3) pecas da interface  4) telas  5) logica da partida  6) main
 */
#include "raylib.h"
#include "motor.h"
#include "assets_embutidos.h"
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <time.h>

/* ---------- tela (medidas "logicas": a janela so escala tudo para caber no monitor) ---------- */
#define LARGURA   640
#define ALTURA    980
#define QUAD      36                          /* lado de um quadradinho do tabuleiro */
#define CARTA     (QUAD * CASAS_CARTA)        /* cada carta ocupa 2x2 quadradinhos = 72 px */
#define MESA_L    (larguraTab * CARTA)        /* tabuleiro: 10 x 20 quadradinhos = 360 x 720 px */
#define MESA_A    (alturaTab * CARTA)
#define MESA_X    ((LARGURA - MESA_L) / 2)
#define MESA_Y    180
#define MARGEM    20                          /* respiro das bordas da tela */
#define RODAPE_A  54                          /* faixa de tinta com as teclas, no pe de todas as telas */
#define RODAPE_Y  (ALTURA - RODAPE_A)

/* ---------- hierarquia dos textos (Caveat) ---------- */
#define T_TITULO   78    /* titulo de tela: Como jogar, Fim de jogo      (negrito) */
#define T_BOTAO    52    /* botoes do menu                               (negrito) */
#define T_SUB      40    /* subtitulos e nomes dos jogadores             (negrito) */
#define T_CORPO    32    /* texto corrido                                (seminegrito) */
#define T_PEQUENO  28    /* legendas e dicas de tecla                    (seminegrito) */
#define MEIO_LETRA 0.54f /* na Caveat, o meio visual das letras fica a 54% da altura da linha */

#define TRACO      3.5f  /* espessura do contorno de tinta dos paineis */

/* ---------- constantes de jogo e efeitos ---------- */
#define ARQUIVO_RECORDES    "recordes.txt"
#define TAM_NOME_DIGITADO   12
#define PONTOS_POR_NIVEL    10
#define MAX_RANKING_EXIBIDO 5
#define MAX_PREVIA_FILA     3
#define VIDA_FANTASMA       0.55f
#define GRAVIDADE_MIGALHA   520.0f
#define VELOCIDADE_POPUP    40.0f
#define COR(r, g, b) ((Color){(r), (g), (b), 255})

/* ---------- paleta: as cores da logo ---------- */
#define TINTA     COR(24, 20, 18)       /* contornos e texto */
#define PAPEL     COR(255, 248, 232)    /* paineis */
#define PAPEL_2   COR(247, 233, 204)    /* papel um tom abaixo (xadrez do tabuleiro, linhas alternadas) */
#define VERDE     COR(71, 137, 101)
#define VERMELHO  COR(176, 42, 4)
#define QUEIJO    COR(246, 196, 69)     /* destaque: de quem e a vez, pontos ganhos */

static const Color COR_J[2] = {{71, 137, 101, 255}, {176, 42, 4, 255}};     /* Jogador 1 = verde, Jogador 2 = vermelho */

typedef enum { TELA_MENU, TELA_COMO, TELA_JOGO } Tela;
typedef struct { float x, y, vida; int valor; } Fantasma;          /* carta que acabou de pontuar, sumindo */
typedef struct { int pontos; bool pizza; float x, y, vida, ox, oy; } Popup;   /* "+3" / "+1" subindo; (ox, oy) = onde nasceu */
typedef struct { float x, y, vx, vy, vida, tam; Color cor; } Migalha;       /* farelo que voa quando duas cartas se fundem */
typedef struct { const char *tecla1, *tecla2, *texto; int tecla; } Dica;    /* dica do rodape: tecla(s) + o que faz */
#define MAX_MIGALHAS 120
typedef struct {
    float tempo, dispX, dispY, tEsq, tDir;
    float inclina;           /* o chapeu do topo se inclina para o lado de quem joga */
    Fantasma fantasmas[2 * MAX_PARES];
    Popup popups[MAX_PARES];
    Migalha migalhas[MAX_MIGALHAS];
    char nome[TAM_NOME_DIGITADO + 1];
    int tamNome, vez;        /* vez: 0 = jogador 1, 1 = jogador 2 */
    int pontos[2];           /* pontos de cada jogador (o motor guarda so o total) */
    bool salvo, pausado;
} Partida;

/* ---------- dificuldade (escolhida no menu) ---------- */
typedef struct {
    const char *nome;
    float queda;             /* segundos por linha no comeco da partida */
    int   acelera_apos;      /* pontos a partir dos quais a queda comeca a acelerar */
    float acelera;           /* segundos a menos por ponto depois disso */
    float queda_min;         /* a queda nunca fica mais rapida que isto */
    int   tomate_nivel, tomate_inicio, tomate_por_nivel, tomate_max;   /* chances em % */
} Dificuldade;
static const Dificuldade DIFICULDADES[3] = {
    {"Fácil",   0.68f, 40, 0.003f, 0.26f, 3, 3, 1, 10},
    {"Normal",  0.55f, 30, 0.004f, 0.18f, 2, 4, 2, 20},
    {"Difícil", 0.42f, 10, 0.005f, 0.13f, 1, 6, 4, 36},
};
static const Color COR_DIF[3] = {{71, 137, 101, 255}, {214, 150, 38, 255}, {176, 42, 4, 255}};
static int dificuldade = 1;      /* 0 = Facil, 1 = Normal, 2 = Dificil */

/* ---------- mouse, janela e som (valem para todas as telas) ---------- */
static Vector2 mouse;            /* posicao do mouse nas medidas do jogo */
static Rectangle visivel;        /* pedaco do jogo que aparece na janela */
static bool sobre_clicavel;      /* o mouse esta em cima de algo clicavel neste quadro? */
static int  tecla_clicada;       /* tecla de um botao clicado neste quadro... */
static int  tecla_virtual;       /* ...que vale como tecla apertada no quadro seguinte */
static bool audio_ok, som_ligado = true, tela_cheia;

/* ================================================================
 *  1. RECURSOS (tudo vem de assets_embutidos.h)
 * ================================================================ */
static Font fTitulo, fTexto;
static Texture2D spr_carta[9], spr_tomate, spr_logo, spr_mascote;   /* spr_carta[CARTA_CAIXA..CARTA_INTEIRA] */
static Texture2D spr_fundo, spr_fundo_borrado;
static Sound s_passar, s_clique, s_mover, s_pouso, s_iguais, s_pizza, s_nivel, s_fim;
static Music musica;

static Texture2D textura(const unsigned char *dados, int tam) {
    Image img = LoadImageFromMemory(".png", dados, tam);
    if (img.data == NULL) {
        TraceLog(LOG_WARNING, "PIZZALIGN: uma imagem embutida nao carregou");
        return (Texture2D){0};
    }
    Texture2D t = LoadTextureFromImage(img);
    UnloadImage(img);
    GenTextureMipmaps(&t);
    SetTextureFilter(t, TEXTURE_FILTER_TRILINEAR);
    return t;
}

static Font fonte(const unsigned char *dados, int tam, int tamanhoBase) {
    int pontos[200], n = 0;                                  /* ASCII + letras acentuadas (Latin-1) */
    for (int c = 32; c < 127; c++) pontos[n++] = c;
    for (int c = 160; c < 256; c++) pontos[n++] = c;
    Font f = LoadFontFromMemory(".ttf", dados, tam, tamanhoBase, pontos, n);
    if (f.texture.id == 0) {
        TraceLog(LOG_WARNING, "PIZZALIGN: a fonte nao carregou, usando a padrao");
        return GetFontDefault();
    }
    GenTextureMipmaps(&f.texture);
    SetTextureFilter(f.texture, TEXTURE_FILTER_TRILINEAR);
    return f;
}

static Sound som(const unsigned char *dados, int tam, float volume) {
    Wave w = LoadWaveFromMemory(".wav", dados, tam);
    Sound s = LoadSoundFromWave(w);
    UnloadWave(w);
    SetSoundVolume(s, volume);
    return s;
}

/* toca um efeito. tom: 1 = normal; maior = mais agudo (usado nas cascatas) */
static void tocar(Sound s, float tom) {
    if (!audio_ok) return;
    SetSoundPitch(s, tom);
    PlaySound(s);
}

static void carregar_sons(void) {
    InitAudioDevice();
    audio_ok = IsAudioDeviceReady();
    if (!audio_ok) {
        TraceLog(LOG_WARNING, "PIZZALIGN: sem dispositivo de audio, o jogo vai rodar sem som");
        return;
    }
    s_passar = som(SOM_PASSAR_WAV, SOM_PASSAR_WAV_TAM, 0.45f);
    s_clique = som(SOM_CLIQUE_WAV, SOM_CLIQUE_WAV_TAM, 0.70f);
    s_mover  = som(SOM_MOVER_WAV,  SOM_MOVER_WAV_TAM,  0.35f);
    s_pouso  = som(SOM_POUSO_WAV,  SOM_POUSO_WAV_TAM,  0.80f);
    s_iguais = som(SOM_IGUAIS_WAV, SOM_IGUAIS_WAV_TAM, 0.75f);
    s_pizza  = som(SOM_PIZZA_WAV,  SOM_PIZZA_WAV_TAM,  0.85f);
    s_nivel  = som(SOM_NIVEL_WAV,  SOM_NIVEL_WAV_TAM,  0.60f);
    s_fim    = som(SOM_FIM_WAV,    SOM_FIM_WAV_TAM,    0.80f);
    musica = LoadMusicStreamFromMemory(".ogg", MUSICA_OGG, MUSICA_OGG_TAM);
    musica.looping = true;
    SetMusicVolume(musica, 0.55f);
    PlayMusicStream(musica);
}

static void carregar_recursos(void) {
    spr_carta[CARTA_CAIXA]    = textura(CARTA_CAIXA_PNG, CARTA_CAIXA_PNG_TAM);
    spr_carta[CARTA_FATIA_E]  = textura(CARTA_FATIA_E_PNG, CARTA_FATIA_E_PNG_TAM);
    spr_carta[CARTA_FATIA_D]  = textura(CARTA_FATIA_D_PNG, CARTA_FATIA_D_PNG_TAM);
    spr_carta[CARTA_METADE_E] = textura(CARTA_METADE_E_PNG, CARTA_METADE_E_PNG_TAM);
    spr_carta[CARTA_METADE_D] = textura(CARTA_METADE_D_PNG, CARTA_METADE_D_PNG_TAM);
    spr_carta[CARTA_FALTA_E]  = textura(CARTA_FALTA_E_PNG, CARTA_FALTA_E_PNG_TAM);
    spr_carta[CARTA_FALTA_D]  = textura(CARTA_FALTA_D_PNG, CARTA_FALTA_D_PNG_TAM);
    spr_carta[CARTA_INTEIRA]  = textura(CARTA_INTEIRA_PNG, CARTA_INTEIRA_PNG_TAM);
    spr_tomate  = textura(TOMATE_PNG, TOMATE_PNG_TAM);
    spr_logo    = textura(LOGO_PNG, LOGO_PNG_TAM);
    spr_mascote = textura(MASCOTE_PNG, MASCOTE_PNG_TAM);
    spr_fundo   = textura(FUNDO_PNG, FUNDO_PNG_TAM);
    spr_fundo_borrado = textura(FUNDO_BORRADO_PNG, FUNDO_BORRADO_PNG_TAM);
    fTitulo = fonte(FONTE_TITULO_TTF, FONTE_TITULO_TTF_TAM, 104);
    fTexto  = fonte(FONTE_TEXTO_TTF, FONTE_TEXTO_TTF_TAM, 64);

    Image icone = LoadImageFromMemory(".png", ICONE_PNG, ICONE_PNG_TAM);
    if (icone.data != NULL) {
        ImageFormat(&icone, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        SetWindowIcon(icone);
        UnloadImage(icone);
    }
}

static void liberar_recursos(void) {
    if (audio_ok) {
        UnloadSound(s_passar); UnloadSound(s_clique); UnloadSound(s_mover); UnloadSound(s_pouso);
        UnloadSound(s_iguais); UnloadSound(s_pizza); UnloadSound(s_nivel); UnloadSound(s_fim);
        UnloadMusicStream(musica);
    }
    CloseAudioDevice();
    for (int i = CARTA_CAIXA; i <= CARTA_INTEIRA; i++) UnloadTexture(spr_carta[i]);
    UnloadTexture(spr_tomate); UnloadTexture(spr_logo); UnloadTexture(spr_mascote);
    UnloadTexture(spr_fundo); UnloadTexture(spr_fundo_borrado);
    if (fTitulo.texture.id != GetFontDefault().texture.id) UnloadFont(fTitulo);
    if (fTexto.texture.id != GetFontDefault().texture.id) UnloadFont(fTexto);
}

/* ================================================================
 *  2. TEXTO
 * ================================================================ */
static float mede(Font f, const char *t, float tam) { return MeasureTextEx(f, t, tam, 0).x; }

/* escreve com o meio das letras na altura cy.
 * alinh: -1 = o texto comeca em x | 0 = x e o centro | 1 = o texto termina em x */
static void texto(Font f, const char *t, float x, float cy, float tam, Color cor, int alinh) {
    float larg = mede(f, t, tam);
    float px = (alinh < 0) ? x : (alinh == 0) ? x - larg / 2 : x - larg;
    DrawTextEx(f, t, (Vector2){px, cy - MEIO_LETRA * tam}, tam, 0, cor);
}

/* texto com contorno de tinta em volta (para ler em cima do tabuleiro) */
static void texto_contornado(Font f, const char *t, float x, float cy, float tam, Color cor, float alfa) {
    const float dx[8] = {-1, 0, 1, -1, 1, -1, 0, 1}, dy[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
    for (int i = 0; i < 8; i++) texto(f, t, x + dx[i] * 3, cy + dy[i] * 3, tam, Fade(TINTA, alfa), -1);
    texto(f, t, x, cy, tam, Fade(cor, alfa), -1);
}

/* escreve um paragrafo quebrando as linhas; devolve o y logo abaixo da ultima linha */
static float paragrafo(const char *txt, float x, float y, float larg, float tam, Color cor) {
    char linha[512] = "", palavra[64], teste[600];
    float passo = tam * 0.90f;
    const char *p = txt;
    while (*p) {
        int n = 0;
        while (*p && *p != ' ' && n < 63) palavra[n++] = *p++;
        palavra[n] = '\0';
        while (*p == ' ') p++;
        snprintf(teste, sizeof teste, "%s%s%s", linha, linha[0] ? " " : "", palavra);
        if (mede(fTexto, teste, tam) > larg && linha[0]) {
            texto(fTexto, linha, x, y + passo / 2, tam, cor, -1);
            y += passo;
            snprintf(linha, sizeof linha, "%s", palavra);
        } else {
            snprintf(linha, sizeof linha, "%.500s", teste);
        }
    }
    if (linha[0]) { texto(fTexto, linha, x, y + passo / 2, tam, cor, -1); y += passo; }
    return y;
}

/* ================================================================
 *  3. PECAS DA INTERFACE (as mesmas em todas as telas)
 * ================================================================ */
static void retangulo(Rectangle r, float raio, Color cor) {
    float menor = fminf(r.width, r.height);
    if (menor <= 0) return;
    DrawRectangleRounded(r, fminf(1.0f, fmaxf(0.0f, 2 * raio / menor)), 12, cor);
}

/* painel no traco da logo: contorno de tinta, cor chapada e uma "espessura" de tinta embaixo */
static void caixa(Rectangle r, float raio, Color fundo, float espessura) {
    if (espessura > 0) retangulo((Rectangle){r.x, r.y + espessura, r.width, r.height}, raio, TINTA);
    retangulo(r, raio, TINTA);
    retangulo((Rectangle){r.x + TRACO, r.y + TRACO, r.width - 2 * TRACO, r.height - 2 * TRACO}, raio - TRACO, fundo);
}

/* mistura duas cores: t = 0 da a primeira, t = 1 da a segunda */
static Color mistura(Color a, Color b, float t) {
    return (Color){(unsigned char)(a.r + (b.r - a.r) * t), (unsigned char)(a.g + (b.g - a.g) * t),
                   (unsigned char)(a.b + (b.b - a.b) * t), (unsigned char)(a.a + (b.a - a.a) * t)};
}

typedef struct { float v, tremor; bool antes; } Anim;

static float animar(Anim *a, bool destaque) {
    float dt = GetFrameTime();
    if (destaque && !a->antes) a->tremor = 0.30f;
    a->antes = destaque;
    if (a->tremor > 0) a->tremor -= dt;
    a->v += ((destaque ? 1.0f : 0.0f) - a->v) * fminf(1.0f, 14.0f * dt);
    return a->v;
}

static float mexida(const Anim *a) {
    return (a->tremor > 0) ? sinf(a->tremor * 46.0f) * 4.5f * (a->tremor / 0.30f) : 0.0f;
}

static bool em_cima(Rectangle r) {
    bool sim = CheckCollisionPointRec(mouse, r);
    if (sim) sobre_clicavel = true;
    return sim;
}

static bool apertou(int t) { return IsKeyPressed(t) || tecla_virtual == t; }

static float largura_tecla(const char *rotulo, float alt) {
    bool seta = (rotulo[1] == '\0') && (rotulo[0] == '<' || rotulo[0] == '>' || rotulo[0] == 'v');
    return seta ? alt : fmaxf(alt, mede(fTitulo, rotulo, alt * 0.96f) + alt * 0.62f);
}

static float tecla_cor(const char *rotulo, float x, float cy, float alt, Color fundo) {
    bool seta = (rotulo[1] == '\0') && (rotulo[0] == '<' || rotulo[0] == '>' || rotulo[0] == 'v');
    float tam = alt * 0.96f;
    float larg = largura_tecla(rotulo, alt);
    Rectangle r = {x, cy - alt / 2 - 1, larg, alt};
    retangulo((Rectangle){r.x, r.y + 3, r.width, r.height}, 7, TINTA);
    retangulo(r, 7, TINTA);
    retangulo((Rectangle){r.x + 2, r.y + 2, r.width - 4, r.height - 4}, 5, fundo);
    if (seta) {
        float giro = (rotulo[0] == '>') ? 0 : (rotulo[0] == 'v') ? 90 : 180;
        DrawPoly((Vector2){r.x + r.width / 2, r.y + r.height / 2}, 3, alt * 0.27f, giro, TINTA);
    } else {
        texto(fTitulo, rotulo, r.x + r.width / 2, r.y + r.height / 2, tam, TINTA, 0);
    }
    return larg;
}

static float tecla(const char *rotulo, float x, float cy, float alt) { return tecla_cor(rotulo, x, cy, alt, PAPEL); }

/* botoes pequenos do canto do rodape: 0 = som (N), 1 = tela cheia (F11) */
static void botao_icone(float x, int tipo) {
    static Anim anim[2];
    Rectangle r = {x, RODAPE_Y + 9, 36, 36};
    bool em = em_cima(r), apertado = em && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    if (em && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) tecla_clicada = tipo ? KEY_F11 : KEY_N;
    animar(&anim[tipo], em);
    r.x += mexida(&anim[tipo]);
    if (em && !apertado) r.y -= 3;
    retangulo(r, 9, em ? QUEIJO : PAPEL);
    float cx = r.x + 18, cy = r.y + 18;
    if (tipo == 0) {
        DrawRectangle((int)(cx - 11), (int)(cy - 4), 6, 8, TINTA);
        DrawTriangle((Vector2){cx + 1, cy - 10}, (Vector2){cx - 9, cy}, (Vector2){cx + 1, cy + 10}, TINTA);
        if (som_ligado) {
            DrawRing((Vector2){cx + 1, cy}, 5.5f, 8.0f, -52, 52, 12, TINTA);
            DrawRing((Vector2){cx + 1, cy}, 10.5f, 13.0f, -46, 46, 14, TINTA);
        } else {
            DrawLineEx((Vector2){cx + 5, cy - 5}, (Vector2){cx + 14, cy + 5}, 3, VERMELHO);
            DrawLineEx((Vector2){cx + 5, cy + 5}, (Vector2){cx + 14, cy - 5}, 3, VERMELHO);
        }
    } else {
        float d = tela_cheia ? 4.0f : 10.0f;
        for (int i = 0; i < 4; i++) {
            float sx = (i % 2) ? 1.0f : -1.0f, sy = (i / 2) ? 1.0f : -1.0f;
            float px = cx + sx * d, py = cy + sy * d;
            float dir = tela_cheia ? 1.0f : -1.0f;
            DrawRectangleRec((Rectangle){fminf(px, px + sx * dir * 7), py - 1.5f, 7, 3}, TINTA);
            DrawRectangleRec((Rectangle){px - 1.5f, fminf(py, py + sy * dir * 7), 3, 7}, TINTA);
        }
    }
}

static void desenhar_rodape(const Dica *dicas, int n) {
    static Anim anim[4];
    const float alt = 30, entre = 30;
    float x_icones = LARGURA - MARGEM - 36 - 44;
    float total = 0;
    for (int i = 0; i < n; i++) {
        total += largura_tecla(dicas[i].tecla1, alt) + 8 + mede(fTexto, dicas[i].texto, T_PEQUENO);
        if (dicas[i].tecla2) total += largura_tecla(dicas[i].tecla2, alt) + 5;
        if (i < n - 1) total += entre;
    }
    DrawRectangleRec((Rectangle){visivel.x, RODAPE_Y, visivel.width, visivel.y + visivel.height - RODAPE_Y}, TINTA);
    float x = (MARGEM + x_icones - 12 - total) / 2, cy = RODAPE_Y + RODAPE_A / 2.0f;
    for (int i = 0; i < n; i++) {
        float larg = largura_tecla(dicas[i].tecla1, alt) + 8 + mede(fTexto, dicas[i].texto, T_PEQUENO)
                   + (dicas[i].tecla2 ? largura_tecla(dicas[i].tecla2, alt) + 5 : 0);
        Rectangle area = {x - 9, RODAPE_Y + 5, larg + 18, RODAPE_A - 10};
        bool em = dicas[i].tecla != 0 && em_cima(area);
        bool apertado = em && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        if (em && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) tecla_clicada = dicas[i].tecla;
        if (em) retangulo(area, 12, Fade(PAPEL, apertado ? 0.30f : 0.16f));
        float sobe = (em && !apertado) ? 2.0f : 0.0f;
        Color fundo = em ? QUEIJO : PAPEL;
        animar(&anim[i % 4], em);
        float mx = mexida(&anim[i % 4]);
        x += tecla_cor(dicas[i].tecla1, x + mx, cy - sobe, alt, fundo);
        if (dicas[i].tecla2) x += 5 + tecla_cor(dicas[i].tecla2, x + 5 + mx, cy - sobe, alt, fundo);
        x += 8;
        texto(fTexto, dicas[i].texto, x, cy, T_PEQUENO, em ? QUEIJO : PAPEL, -1);
        x += mede(fTexto, dicas[i].texto, T_PEQUENO) + entre;
    }
    botao_icone(x_icones, 0);
    botao_icone(x_icones + 44, 1);
}

static void rabisco(float x, float y, float larg, Color cor) {
    Vector2 antes = {x, y};
    for (float dx = 6; dx <= larg; dx += 6) {
        Vector2 p = {x + dx, y + 3.2f * sinf(dx * 0.16f)};
        DrawLineEx(antes, p, 4, cor);
        DrawCircleV(p, 2, cor);
        antes = p;
    }
}

static void tracejado(float x, float y, float larg) {
    for (float dx = 0; dx + 10 <= larg; dx += 18) DrawRectangleRounded((Rectangle){x + dx, y, 10, 3}, 1, 4, Fade(TINTA, 0.35f));
}

static void selo(float cx, float cy, int pontos) {
    DrawCircleV((Vector2){cx, cy + 2}, 23, TINTA);
    DrawCircleV((Vector2){cx, cy}, 23, TINTA);
    DrawCircleV((Vector2){cx, cy}, 19.5f, pontos >= PONTOS_PIZZA ? QUEIJO : PAPEL_2);
    texto(fTitulo, TextFormat("+%d", pontos), cx - 1, cy, 36, TINTA, 0);
}

static void desenhar_textura(Texture2D t, float x, float y, float tam, float alfa) {
    DrawTexturePro(t, (Rectangle){0, 0, (float)t.width, (float)t.height}, (Rectangle){x, y, tam, tam},
                   (Vector2){0, 0}, 0, Fade(WHITE, alfa));
}

/* carta: CARTA_CAIXA..CARTA_INTEIRA = os desenhos das pizzas, TOMATE = bloco */
static void desenhar_carta(int valor, float x, float y, float tam, float alfa) {
    Texture2D t;
    if (valor == TOMATE) {
        t = spr_tomate;
    } else if (valor >= CARTA_CAIXA && valor <= CARTA_INTEIRA) {
        t = spr_carta[valor];
    } else {
        t = spr_carta[CARTA_CAIXA];
    }
    if (t.id == 0) { DrawRectangle((int)x, (int)y, (int)tam, (int)tam, Fade(QUEIJO, alfa)); return; }
    desenhar_textura(t, x, y, tam, alfa);
}

static void desenhar_adesivo(Texture2D t, float cx, float cy, float larg, float graus) {
    if (t.id == 0) { texto(fTitulo, "Pizzalign", cx, cy, T_TITULO, TINTA, 0); return; }
    float alt = larg * t.height / t.width;
    Rectangle src = {0, 0, (float)t.width, (float)t.height};
    Vector2 centro = {larg / 2, alt / 2};
    DrawTexturePro(t, src, (Rectangle){cx, cy + larg * 0.022f + 2, larg, alt}, centro, graus, Fade(TINTA, 0.40f));
    DrawTexturePro(t, src, (Rectangle){cx, cy, larg, alt}, centro, graus, WHITE);
}

static void desenhar_fundo(void) {
    Rectangle v = visivel;
    if (v.width > LARGURA + 1 || v.height > ALTURA + 1) {
        float esc = fmaxf(v.width / LARGURA, v.height / ALTURA);
        DrawTexturePro(spr_fundo_borrado, (Rectangle){0, 0, (float)spr_fundo_borrado.width, (float)spr_fundo_borrado.height},
                       (Rectangle){LARGURA / 2.0f, ALTURA / 2.0f, LARGURA * esc, ALTURA * esc},
                       (Vector2){LARGURA * esc / 2, ALTURA * esc / 2}, 0, WHITE);
        DrawRectangleRec(v, Fade(TINTA, 0.50f));
        DrawRectangle(-5, -5, LARGURA + 10, ALTURA + 10, Fade(TINTA, 0.55f));
    }
    float fl = (float)spr_fundo.width, fa = (float)spr_fundo.height;
    float esc = fminf(fl / LARGURA, fa / ALTURA);
    Rectangle recorte = {(fl - LARGURA * esc) / 2, (fa - ALTURA * esc) / 2, LARGURA * esc, ALTURA * esc};
    DrawTexturePro(spr_fundo, recorte, (Rectangle){0, 0, LARGURA, ALTURA}, (Vector2){0, 0}, 0, WHITE);
    DrawRectangle(0, 0, LARGURA, ALTURA, Fade(TINTA, 0.10f));
    DrawRectangleGradientV(0, ALTURA - 300, LARGURA, 300, Fade(TINTA, 0.0f), Fade(TINTA, 0.35f));
}

static void escurecer(void) { DrawRectangleRec(visivel, Fade(TINTA, 0.60f)); }

/* ================================================================
 *  4. TELAS
 * ================================================================ */
static int linha_de_pouso(const EstadoJogo *j) {
    int y = j->fatiaAtual.y;
    while (y + 1 < alturaTab && j->tab[y + 1][j->fatiaAtual.x] == VAZIA) y++;
    return y;
}

static void desenhar_mesa(const EstadoJogo *jogo, float dispX, float dispY, int vez) {
    caixa((Rectangle){MESA_X - 8, MESA_Y - 8, MESA_L + 16, MESA_A + 16}, 14, PAPEL, 7);
    for (int r = 0; r < alturaTab * CASAS_CARTA; r++)
        for (int c = 0; c < larguraTab * CASAS_CARTA; c++)
            if ((r + c) % 2 == 0) DrawRectangle(MESA_X + c * QUAD, MESA_Y + r * QUAD, QUAD, QUAD, PAPEL_2);

    if (!jogo->gameOver)
        DrawRectangle(MESA_X + jogo->fatiaAtual.x * CARTA, MESA_Y, CARTA, MESA_A, Fade(COR_J[vez], 0.20f));

    for (int r = 0; r < alturaTab; r++)
        for (int c = 0; c < larguraTab; c++)
            if (jogo->tab[r][c] != VAZIA)
                desenhar_carta(jogo->tab[r][c], (float)(MESA_X + c * CARTA), (float)(MESA_Y + r * CARTA), CARTA, 1.0f);

    if (!jogo->gameOver) {
        int x = jogo->fatiaAtual.x, v = jogo->fatiaAtual.pedacos;
        desenhar_carta(v, (float)(MESA_X + x * CARTA), (float)(MESA_Y + linha_de_pouso(jogo) * CARTA), CARTA, 0.28f);
        desenhar_carta(v, MESA_X + dispX * CARTA, MESA_Y + dispY * CARTA, CARTA, 1.0f);
    }
}

static void desenhar_jogador(int j, const Partida *pa, bool fim) {
    const float larg = 196, alt = 108;
    bool ativo = (pa->vez == j) && !fim;
    Rectangle r = {(j == 0) ? MARGEM : LARGURA - MARGEM - larg, ativo ? 26 : 31, larg, alt};
    caixa(r, 16, ativo ? COR_J[j] : PAPEL, ativo ? 7 : 2);

    texto(fTitulo, TextFormat("Jogador %d", j + 1), r.x + 16, r.y + 28, 36, ativo ? PAPEL : COR_J[j], -1);
    texto(fTitulo, TextFormat("%d", pa->pontos[j]), r.x + larg - 18, r.y + 58, pa->pontos[j] < 100 ? 92 : 62,
          ativo ? PAPEL : Fade(TINTA, 0.78f), 1);

    float kx = r.x + 16, ky = r.y + 76;
    kx += tecla(j ? "<" : "A", kx, ky, 26) + 4;
    kx += tecla(j ? "v" : "S", kx, ky, 26) + 4;
    tecla(j ? ">" : "D", kx, ky, 26);

    if (ativo) {
        Rectangle e = {r.x + (larg - 104) / 2, r.y + alt - 9, 104, 34};
        caixa(e, 17, QUEIJO, 0);
        texto(fTitulo, "sua vez", e.x + e.width / 2, e.y + e.height / 2 - 1, 30, TINTA, 0);
    }
}

static void desenhar_lateral_esquerda(const EstadoJogo *jogo, int nivel) {
    Rectangle n = {MARGEM, MESA_Y - 8, 100, 124};
    caixa(n, 14, PAPEL, 5);
    float cx = n.x + n.width / 2;
    texto(fTexto, "nível", cx, n.y + 24, T_PEQUENO, Fade(TINTA, 0.75f), 0);
    texto(fTitulo, TextFormat("%d", nivel), cx, n.y + 66, 72, TINTA, 0);
    for (int i = 0; i < PONTOS_POR_NIVEL; i++)
        DrawRectangleRounded((Rectangle){n.x + 13 + i * 7.5f, n.y + 102, 5.5f, 9}, 0.8f, 4,
                             i < jogo->pontuacao % PONTOS_POR_NIVEL ? VERMELHO : Fade(TINTA, 0.18f));

    Rectangle c = {MARGEM, n.y + n.height + 22, 100, 162};
    caixa(c, 14, PAPEL, 5);
    desenhar_carta(CARTA_METADE_E, c.x + 9, c.y + 14, 30, 1);
    desenhar_carta(CARTA_METADE_D, c.x + 33, c.y + 14, 30, 1);
    texto(fTitulo, "+3", c.x + c.width - 12, c.y + 29, 38, TINTA, 1);
    texto(fTexto, "completou", cx, c.y + 60, T_PEQUENO - 3, Fade(TINTA, 0.75f), 0);
    tracejado(c.x + 14, c.y + 80, c.width - 28);
    desenhar_carta(CARTA_FATIA_D, c.x + 7, c.y + 90, 30, 1);
    desenhar_carta(CARTA_FATIA_D, c.x + 31, c.y + 90, 30, 1);
    texto(fTitulo, "+1", c.x + c.width - 12, c.y + 105, 38, TINTA, 1);
    texto(fTexto, "iguais", cx, c.y + 136, T_PEQUENO - 3, Fade(TINTA, 0.75f), 0);

    Rectangle d = {MARGEM, c.y + c.height + 22, 100, 46};
    caixa(d, 23, COR_DIF[dificuldade], 4);
    texto(fTitulo, DIFICULDADES[dificuldade].nome, cx, d.y + d.height / 2 - 1, 32, dificuldade == 1 ? TINTA : PAPEL, 0);
}

static void desenhar_proximas(const EstadoJogo *jogo) {
    Rectangle p = {LARGURA - MARGEM - 100, MESA_Y - 8, 100, 268};
    caixa(p, 14, PAPEL, 5);
    texto(fTexto, "próximas", p.x + p.width / 2, p.y + 24, T_PEQUENO, Fade(TINTA, 0.75f), 0);
    retangulo((Rectangle){p.x + 11, p.y + 44, 78, 78}, 12, Fade(QUEIJO, 0.55f));
    const NoFatia *n = jogo->filaFatias;
    for (int i = 0; i < MAX_PREVIA_FILA && n; i++, n = n->proximo) {
        if (i == 0) desenhar_carta(n->pedacos, p.x + 16, p.y + 49, 68, 1.0f);
        else        desenhar_carta(n->pedacos, p.x + 24, p.y + 136 + (i - 1) * 62, 52, 0.85f);
    }
}

static void desenhar_popup(const Popup *p) {
    float nasceu = (1.4f - p->vida) / 0.45f;
    if (nasceu < 1.0f) {
        float raio = 26 + 58 * nasceu;
        DrawRing((Vector2){p->ox + CARTA / 2.0f, p->oy + CARTA / 2.0f}, raio - 3.5f * (1 - nasceu), raio, 0, 360, 48,
                 Fade(p->pizza ? QUEIJO : PAPEL, 0.85f * (1 - nasceu)));
    }
    float a = fminf(1.0f, p->vida * 1.6f);
    float tam = 64 + 26 * fmaxf(0.0f, (p->vida - 1.25f) / 0.15f);
    const char *t = TextFormat("+%d", p->pontos);
    float larg = mede(fTitulo, t, tam) + (p->pizza ? 50 : 0);
    float x = p->x + (CARTA - larg) / 2, cy = p->y + 30;
    if (p->pizza) { desenhar_textura(spr_carta[CARTA_INTEIRA], x, cy - 23, 46, a); x += 50; }
    texto_contornado(fTitulo, t, x, cy, tam, p->pizza ? QUEIJO : PAPEL, a);
}

static void desenhar_migalhas(const Partida *pa) {
    for (int i = 0; i < MAX_MIGALHAS; i++) {
        const Migalha *m = &pa->migalhas[i];
        if (m->vida > 0) DrawCircleV((Vector2){m->x, m->y}, m->tam, Fade(m->cor, fminf(1.0f, m->vida * 3.0f)));
    }
}

/* ---------- menu ---------- */
static Rectangle botao_menu(int i)        { return (Rectangle){150, i == 0 ? 540.0f : 724.0f, 340, 72}; }
static Rectangle botao_dificuldade(int d) { return (Rectangle){150 + d * 116.0f, 632, 108, 72}; }
static Anim anim_menu[2], anim_dif[3];
static bool foco_teclado = true;

static void desenhar_botao(Rectangle b, const char *rotulo, bool destaque, Anim *anim) {
    bool apertado = CheckCollisionPointRec(mouse, b) && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    float a = animar(anim, destaque);
    Rectangle r = {b.x - 6 * a + mexida(anim), apertado ? b.y + 6 : b.y - 5 * a, b.width + 12 * a, b.height};
    caixa(r, 20, mistura(PAPEL, VERMELHO, a), apertado ? 2 : 6 + 5 * a);
    texto(fTitulo, rotulo, r.x + r.width / 2, r.y + r.height / 2, T_BOTAO + 4 * a, mistura(TINTA, PAPEL, a), 0);
}

static void desenhar_dificuldade(int d, bool linha_em_foco) {
    Rectangle b = botao_dificuldade(d);
    bool em = CheckCollisionPointRec(mouse, b), apertado = em && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    bool escolhida = (d == dificuldade);
    float a = animar(&anim_dif[d], em || (linha_em_foco && escolhida));
    float afunda = apertado ? 6 : (escolhida ? 3 : 0);
    Rectangle r = {b.x + mexida(&anim_dif[d]), b.y + afunda - 4 * a, b.width, b.height};
    caixa(r, 18, mistura(PAPEL, COR_DIF[d], a), apertado ? 2 : (escolhida ? 3 : 6) + 4 * a);
    Color cor = mistura(TINTA, d == 1 ? TINTA : PAPEL, a);
    float cx = r.x + r.width / 2, cy = r.y + r.height / 2;
    texto(fTitulo, DIFICULDADES[d].nome, cx, cy - (escolhida ? 3 : 0), 40, cor, 0);
    if (escolhida) rabisco(cx - 30, cy + 21, 60, cor);
}

static void desenhar_menu(int opcao, const Recorde *recordes) {
    float t = (float)GetTime();
    desenhar_adesivo(spr_logo, LARGURA / 2.0f, 276 + 4 * sinf(t * 1.6f), 356, -3 + 1.2f * sinf(t * 0.9f));

    bool em_novo = CheckCollisionPointRec(mouse, botao_menu(0)), em_como = CheckCollisionPointRec(mouse, botao_menu(1));
    desenhar_botao(botao_menu(0), "Novo jogo", foco_teclado ? opcao == 0 : em_novo, &anim_menu[0]);
    desenhar_botao(botao_menu(1), "Como jogar", foco_teclado ? opcao == 2 : em_como, &anim_menu[1]);

    for (int d = 0; d < 3; d++) desenhar_dificuldade(d, foco_teclado && opcao == 1);

    Rectangle linha = {150, 632, 340, 72};
    Rectangle foco = (opcao == 0) ? botao_menu(0) : (opcao == 2) ? botao_menu(1) : linha;
    if (foco_teclado || CheckCollisionPointRec(mouse, foco))
        desenhar_carta(CARTA_FATIA_E, foco.x - 96 + 5 * sinf(t * 5), foco.y + foco.height / 2 - 46, 84, 1);

    if (recordes) {
        const char *r = TextFormat("recorde: %s, %d pontos", recordes->nome, recordes->pontos);
        float larg = mede(fTexto, r, T_CORPO) + 52;
        Rectangle e = {(LARGURA - larg) / 2, 842, larg, 46};
        caixa(e, 23, PAPEL, 4);
        texto(fTexto, r, LARGURA / 2.0f, e.y + e.height / 2, T_CORPO, TINTA, 0);
    }

    Dica escolher = {"W", "S", "escolher", 0}, sair = {"Q", NULL, "sair", KEY_Q};
    Dica confirmar = {"Enter", NULL, "confirmar", KEY_ENTER}, mudar = {"A", "D", "mudar", 0};
    Dica dicas[3] = {escolher, opcao == 1 ? mudar : confirmar, sair};
    desenhar_rodape(dicas, 3);
}

static float linha_exemplo(int a, int b, int pontos, const char *explica, float x, float y) {
    const float ts = 50;
    float cy = y + ts / 2;
    desenhar_carta(a, x, y, ts, 1);
    texto(fTitulo, "+", x + 64, cy, T_SUB, TINTA, 0);
    desenhar_carta(b, x + 78, y, ts, 1);
    selo(x + 166, cy, pontos);
    texto(fTexto, explica, x + 204, cy, T_CORPO, TINTA, -1);
    return y + ts + 8;
}

static void desenhar_como_jogar(void) {
    Rectangle p = {30, 22, 580, 874};
    caixa(p, 22, PAPEL, 8);
    float cx = LARGURA / 2.0f, x = p.x + 34, l = p.width - 68;
    texto(fTitulo, "Como jogar", cx, p.y + 54, T_TITULO, TINTA, 0);
    rabisco(cx - 116, p.y + 94, 232, VERMELHO);

    float y = p.y + 120, lc = (l - 16) / 2;
    for (int j = 0; j < 2; j++) {
        Rectangle c = {x + j * (lc + 16), y, lc, 98};
        caixa(c, 14, COR_J[j], 0);
        texto(fTitulo, TextFormat("Jogador %d", j + 1), c.x + 16, c.y + 27, T_SUB, PAPEL, -1);
        float kx = c.x + 16, ky = c.y + 68;
        kx += tecla(j ? "<" : "A", kx, ky, 28) + 4;
        kx += tecla(j ? ">" : "D", kx, ky, 28) + 7;
        texto(fTexto, "mover", kx, ky, T_PEQUENO, PAPEL, -1);
        kx += mede(fTexto, "mover", T_PEQUENO) + 14;
        kx += tecla(j ? "v" : "S", kx, ky, 28) + 7;
        texto(fTexto, "descer", kx, ky, T_PEQUENO, PAPEL, -1);
    }
    y = paragrafo("Cada um joga uma carta e passa a vez.", x, y + 98 + 10, l, T_CORPO, TINTA);

    texto(fTitulo, "Como pontuar", x, y + 32, T_SUB, TINTA, -1);
    y = paragrafo("Quando a carta pousa, ela combina com a vizinha de baixo ou dos lados, de qualquer jogador. "
                  "As duas somem e as de cima descem.", x, y + 56, l, T_CORPO, TINTA);
    y += 10;
    y = linha_exemplo(CARTA_METADE_E, CARTA_METADE_D, PONTOS_PIZZA, "as duas metades", x, y);
    y = linha_exemplo(CARTA_FATIA_E, CARTA_FALTA_E, PONTOS_PIZZA, "a fatia que falta, do mesmo lado", x, y);
    y = linha_exemplo(CARTA_INTEIRA, CARTA_CAIXA, PONTOS_PIZZA, "a pizza inteira e a caixa vazia", x, y);
    y = linha_exemplo(CARTA_FATIA_E, CARTA_FATIA_E, PONTOS_IGUAL, "duas cartas iguais", x, y);

    texto(fTitulo, "Fique de olho", x, y + 30, T_SUB, TINTA, -1);
    y += 56;
    desenhar_carta(TOMATE, x, y + 2, 50, 1);
    float yt = paragrafo("O tomate não combina com nada, só ocupa espaço. A cada nível (10 pontos) ele aparece mais.",
                         x + 64, y, l - 64, T_CORPO, TINTA);
    y = paragrafo("A queda acelera com os pontos; no Difícil, tudo vem mais cedo. Quando o topo enche, vence quem tiver mais pontos.",
                  x, fmaxf(yt, y + 56) + 6, l, T_CORPO, TINTA);
    float kx = x, ky = y + 24;
    kx += tecla("N", kx, ky, 28) + 7;
    texto(fTexto, "liga ou desliga o som", kx, ky, T_PEQUENO, TINTA, -1);
    kx += mede(fTexto, "liga ou desliga o som", T_PEQUENO) + 22;
    kx += tecla("F11", kx, ky, 28) + 7;
    texto(fTexto, "tela cheia", kx, ky, T_PEQUENO, TINTA, -1);

    Dica dicas[1] = {{"Enter", NULL, "voltar ao menu", KEY_ENTER}};
    desenhar_rodape(dicas, 1);
}

static int vencedor(const Partida *pa) {
    if (pa->pontos[0] == pa->pontos[1]) return -1;
    return (pa->pontos[0] > pa->pontos[1]) ? 0 : 1;
}

static void desenhar_pausa(void) {
    escurecer();
    Rectangle p = {140, 380, 360, 168};
    caixa(p, 22, PAPEL, 8);
    texto(fTitulo, "Pausado", LARGURA / 2.0f, p.y + 62, T_TITULO, TINTA, 0);
    texto(fTexto, "o forno espera por vocês", LARGURA / 2.0f, p.y + 122, T_CORPO, Fade(TINTA, 0.75f), 0);
    Dica dicas[3] = {{"P", NULL, "continuar", KEY_P}, {"M", NULL, "menu", KEY_M}, {"Q", NULL, "sair", KEY_Q}};
    desenhar_rodape(dicas, 3);
}

static void desenhar_fim(const Recorde *recordes, const Partida *pa) {
    int v = vencedor(pa);
    float cx = LARGURA / 2.0f;
    escurecer();
    int linhas = 0;
    for (const Recorde *r = recordes; r && linhas < MAX_RANKING_EXIBIDO; r = r->prox) linhas++;
    Rectangle p = {60, 0, 520, pa->salvo ? 356.0f + linhas * 50 : 468.0f};
    p.y = (RODAPE_Y - p.height) / 2 - 6;
    caixa(p, 22, PAPEL, 8);
    float x = p.x + 44, l = p.width - 88;

    texto(fTitulo, "Fim de jogo", cx, p.y + 56, T_TITULO, TINTA, 0);

    Rectangle faixa = {x, p.y + 104, l, 62};
    caixa(faixa, 31, v < 0 ? QUEIJO : COR_J[v], 0);
    texto(fTitulo, v < 0 ? "Empate!" : TextFormat("Jogador %d venceu!", v + 1), cx, faixa.y + faixa.height / 2 - 1,
          46, v < 0 ? TINTA : PAPEL, 0);

    float py = p.y + 212;
    texto(fTitulo, "x", cx, py, 34, Fade(TINTA, 0.55f), 0);
    texto(fTitulo, TextFormat("%d", pa->pontos[0]), cx - 26, py, 70, TINTA, 1);
    texto(fTitulo, TextFormat("%d", pa->pontos[1]), cx + 26, py, 70, TINTA, -1);
    texto(fTitulo, "Jogador 1", x, py, 34, COR_J[0], -1);
    texto(fTitulo, "Jogador 2", x + l, py, 34, COR_J[1], 1);
    tracejado(x, p.y + 258, l);

    if (!pa->salvo) {
        texto(fTexto, v < 0 ? "Digite um nome para o ranking" : "Nome de quem venceu, para o ranking", x, p.y + 296, T_CORPO, TINTA, -1);
        Rectangle campo = {x, p.y + 322, l, 68};
        caixa(campo, 14, WHITE, 0);
        float cyc = campo.y + campo.height / 2;
        texto(fTitulo, pa->nome, campo.x + 20, cyc, 48, TINTA, -1);
        if (((int)(GetTime() * 2)) % 2 == 0)
            DrawRectangleRounded((Rectangle){campo.x + 24 + mede(fTitulo, pa->nome, 48), cyc - 18, 4, 36}, 1, 4, VERMELHO);
        texto(fTexto, TextFormat("até %d letras ou números", TAM_NOME_DIGITADO), x, p.y + 418, T_PEQUENO, Fade(TINTA, 0.7f), -1);
        Dica dicas[1] = {{"Enter", NULL, "salvar", KEY_ENTER}};
        desenhar_rodape(dicas, 1);
    } else {
        texto(fTitulo, "Ranking", x, p.y + 298, T_SUB, TINTA, -1);
        int i = 0;
        for (const Recorde *r = recordes; r && i < MAX_RANKING_EXIBIDO; r = r->prox, i++) {
            Rectangle linha = {x - 12, p.y + 330 + i * 50, l + 24, 44};
            if (i == 0) retangulo(linha, 22, Fade(QUEIJO, 0.65f));
            else if (i % 2 == 0) retangulo(linha, 22, PAPEL_2);
            float cyl = linha.y + linha.height / 2;
            texto(fTitulo, TextFormat("%d", i + 1), x + 8, cyl, 34, Fade(TINTA, 0.6f), -1);
            texto(fTexto, r->nome, x + 44, cyl, 32, TINTA, -1);
            texto(fTitulo, TextFormat("%d", r->pontos), x + l - 4, cyl, 38, TINTA, 1);
        }
        Dica dicas[3] = {{"R", NULL, "jogar de novo", KEY_R}, {"M", NULL, "menu", KEY_M}, {"Q", NULL, "sair", KEY_Q}};
        desenhar_rodape(dicas, 3);
    }
}

/* ================================================================
 *  5. LOGICA DA PARTIDA (chama o motor)
 * ================================================================ */
static void nova_partida(Partida *p, EstadoJogo *jogo) {
    iniciarJogo(jogo);
    memset(p, 0, sizeof *p);
    p->dispX = (float)jogo->fatiaAtual.x;
}

static float intervalo_de_queda(int pontos) {
    const Dificuldade *d = &DIFICULDADES[dificuldade];
    float t = d->queda;
    if (pontos > d->acelera_apos) t -= d->acelera * (pontos - d->acelera_apos);
    return t < d->queda_min ? d->queda_min : t;
}

static int chance_de_tomate(int pontos) {
    const Dificuldade *d = &DIFICULDADES[dificuldade];
    int nivel = pontos / PONTOS_POR_NIVEL + 1;
    if (nivel < d->tomate_nivel) return 0;
    int chance = d->tomate_inicio + d->tomate_por_nivel * (nivel - d->tomate_nivel);
    return chance > d->tomate_max ? d->tomate_max : chance;
}

static void sortear_tomate(EstadoJogo *jogo) {
    if (rand() % 100 >= chance_de_tomate(jogo->pontuacao)) return;
    NoFatia *n = jogo->filaFatias;
    while (n && n->proximo) n = n->proximo;
    if (n) n->pedacos = TOMATE;
}

static void soltar_migalhas(Partida *pa, float x, float y, int quantas) {
    const Color cores[4] = {QUEIJO, COR(214, 140, 58), VERMELHO, VERDE};
    for (int k = 0; k < MAX_MIGALHAS && quantas > 0; k++) {
        if (pa->migalhas[k].vida > 0) continue;
        float ang = (float)(rand() % 628) / 100.0f, forca = 70.0f + rand() % 170;
        pa->migalhas[k] = (Migalha){x, y, cosf(ang) * forca, sinf(ang) * forca - 110.0f,
                                    0.45f + (rand() % 40) / 100.0f, 2.5f + (rand() % 30) / 10.0f, cores[rand() % 4]};
        quantas--;
    }
}

static void registrar_efeitos(Partida *pa, const EstadoJogo *jogo) {
    for (int i = 0; i < jogo->numPares; i++) {
        const Par *p = &jogo->pares[i];
        float x1 = (float)(MESA_X + p->col1 * CARTA), y1 = (float)(MESA_Y + p->lin1 * CARTA);
        float x2 = (float)(MESA_X + p->col2 * CARTA), y2 = (float)(MESA_Y + p->lin2 * CARTA);
        for (int k = 0; k < 2 * MAX_PARES; k++)
            if (pa->fantasmas[k].vida <= 0) { pa->fantasmas[k] = (Fantasma){x1, y1, VIDA_FANTASMA, p->valor1}; break; }
        for (int k = 0; k < 2 * MAX_PARES; k++)
            if (pa->fantasmas[k].vida <= 0) { pa->fantasmas[k] = (Fantasma){x2, y2, VIDA_FANTASMA, p->valor2}; break; }
        for (int k = 0; k < MAX_PARES; k++)
            if (pa->popups[k].vida <= 0) {
                pa->popups[k] = (Popup){p->pontos, p->pontos == PONTOS_PIZZA, (x1 + x2) / 2, (y1 + y2) / 2, 1.4f,
                                        (x1 + x2) / 2, (y1 + y2) / 2};
                break;
            }
        soltar_migalhas(pa, (x1 + x2) / 2 + CARTA / 2.0f, (y1 + y2) / 2 + CARTA / 2.0f, p->pontos == PONTOS_PIZZA ? 18 : 9);
    }
}

static void avancar(Partida *pa, EstadoJogo *jogo) {
    int antes = jogo->pecasFixadas, nivel_antes = jogo->pontuacao / PONTOS_POR_NIVEL;
    descer_peca(jogo);
    if (jogo->pecasFixadas == antes) return;

    float tom = 1.0f + 0.07f * (jogo->numPares > 1 ? jogo->numPares - 1 : 0);
    bool fez_pizza = false;
    for (int i = 0; i < jogo->numPares; i++) if (jogo->pares[i].pontos == PONTOS_PIZZA) fez_pizza = true;
    tocar(s_pouso, 1.0f);
    if (fez_pizza) tocar(s_pizza, tom);
    else if (jogo->ganhoUltima > 0) tocar(s_iguais, tom);
    if (jogo->gameOver) tocar(s_fim, 1.0f);
    else if (jogo->pontuacao / PONTOS_POR_NIVEL > nivel_antes) tocar(s_nivel, 1.0f);

    pa->pontos[pa->vez] += jogo->ganhoUltima;
    registrar_efeitos(pa, jogo);
    sortear_tomate(jogo);
    pa->vez = 1 - pa->vez;
    pa->tEsq = pa->tDir = 0;
    pa->dispX = (float)jogo->fatiaAtual.x;
}

static bool tecla_com_repeticao(int codigo_tecla, float *t, float dt) {
    if (IsKeyPressed(codigo_tecla)) { *t = 0; return true; }
    if (!IsKeyDown(codigo_tecla)) { *t = 0; return false; }
    *t += dt;
    if (*t >= 0.22f) { *t -= 0.06f; return true; }
    return false;
}

static void atualizar_partida(Partida *pa, EstadoJogo *jogo, float dt, float intervalo) {
    if (apertou(KEY_P)) { pa->pausado = !pa->pausado; tocar(s_clique, 1.0f); }
    if (pa->pausado) return;

    int kEsq = pa->vez ? KEY_LEFT : KEY_A, kDir = pa->vez ? KEY_RIGHT : KEY_D, kBaixo = pa->vez ? KEY_DOWN : KEY_S;
    int coluna = jogo->fatiaAtual.x;
    if (tecla_com_repeticao(kEsq, &pa->tEsq, dt)) mover_peca(jogo, -1, 0);
    if (tecla_com_repeticao(kDir, &pa->tDir, dt)) mover_peca(jogo, 1, 0);
    if (jogo->fatiaAtual.x != coluna) tocar(s_mover, 1.0f);

    pa->tempo += dt;
    float passo = IsKeyDown(kBaixo) ? fminf(intervalo, 0.05f) : intervalo;
    if (pa->tempo >= passo) { pa->tempo = 0; avancar(pa, jogo); }
}

static void atualizar_nome(Partida *pa, Recorde **recordes) {
    int ch;
    while ((ch = GetCharPressed()) > 0)
        if (ch < 128 && isalnum((unsigned char)ch) && pa->tamNome < TAM_NOME_DIGITADO) {
            pa->nome[pa->tamNome++] = (char)toupper((unsigned char)ch);
            pa->nome[pa->tamNome] = '\0';
        }
    if (IsKeyPressed(KEY_BACKSPACE) && pa->tamNome > 0) pa->nome[--pa->tamNome] = '\0';
    if (apertou(KEY_ENTER)) {
        tocar(s_clique, 1.0f);
        int v = vencedor(pa), pts = (v < 1) ? pa->pontos[0] : pa->pontos[1];
        recorde_inserir(recordes, pa->tamNome ? pa->nome : "JOGADOR", pts);
        recordes_salvar(*recordes, ARQUIVO_RECORDES);
        pa->salvo = true;
    }
}

static void atualizar_efeitos(Partida *pa, const EstadoJogo *jogo, float dt, float intervalo) {
    pa->dispX += ((float)jogo->fatiaAtual.x - pa->dispX) * fminf(1.0f, 20.0f * dt);
    float frac = (!pa->pausado && !jogo->gameOver && pode_mover(jogo, 0, 1)) ? fminf(1.0f, pa->tempo / intervalo) : 0;
    pa->dispY = jogo->fatiaAtual.y + frac;
    float alvo = jogo->gameOver ? 0.0f : (pa->vez ? 6.0f : -6.0f);
    pa->inclina += (alvo - pa->inclina) * fminf(1.0f, 9.0f * dt);
    for (int i = 0; i < 2 * MAX_PARES; i++) if (pa->fantasmas[i].vida > 0) pa->fantasmas[i].vida -= dt;
    for (int i = 0; i < MAX_PARES; i++)
        if (pa->popups[i].vida > 0) { pa->popups[i].vida -= dt; pa->popups[i].y -= VELOCIDADE_POPUP * dt; }
    for (int i = 0; i < MAX_MIGALHAS; i++) {
        Migalha *m = &pa->migalhas[i];
        if (m->vida <= 0) continue;
        m->vida -= dt;
        m->vy += GRAVIDADE_MIGALHA * dt;
        m->x += m->vx * dt;
        m->y += m->vy * dt;
    }
}

static void desenhar_efeitos(const Partida *pa) {
    for (int i = 0; i < 2 * MAX_PARES; i++)
        if (pa->fantasmas[i].vida > 0) {
            float t = pa->fantasmas[i].vida / VIDA_FANTASMA, tam = CARTA * (1 + (1 - t) * 0.35f);
            desenhar_carta(pa->fantasmas[i].valor, pa->fantasmas[i].x - (tam - CARTA) / 2,
                           pa->fantasmas[i].y - (tam - CARTA) / 2, tam, t);
        }
    desenhar_migalhas(pa);
    for (int i = 0; i < MAX_PARES; i++) if (pa->popups[i].vida > 0) desenhar_popup(&pa->popups[i]);
}

/* ================================================================
 *  6. MAIN
 * ================================================================ */
static Camera2D camera_ajustada(void) {
    float z = fminf((float)GetScreenWidth() / LARGURA, (float)GetScreenHeight() / ALTURA);
    Camera2D c = {0};
    c.zoom = z;
    c.offset = (Vector2){(GetScreenWidth() - LARGURA * z) / 2, (GetScreenHeight() - ALTURA * z) / 2};
    visivel = (Rectangle){-c.offset.x / z, -c.offset.y / z, GetScreenWidth() / z, GetScreenHeight() / z};
    mouse = GetScreenToWorld2D(GetMousePosition(), c);
    return c;
}

static void alternar_tela_cheia(void) {
#if defined(RAYLIB_VERSION_MAJOR) && RAYLIB_VERSION_MAJOR >= 5
    ToggleBorderlessWindowed();
#else
    static int larg_janela, alt_janela;
    if (!IsWindowFullscreen()) {
        larg_janela = GetScreenWidth(); alt_janela = GetScreenHeight();
        int mon = GetCurrentMonitor();
        SetWindowSize(GetMonitorWidth(mon), GetMonitorHeight(mon));
        ToggleFullscreen();
    } else {
        ToggleFullscreen();
        SetWindowSize(larg_janela, alt_janela);
    }
#endif
    tela_cheia = !tela_cheia;
}

static Tela atualizar_menu(int *opcao, Partida *pa, EstadoJogo *jogo) {
    int opcao_antes = *opcao, dif_antes = dificuldade, ativar = -1;
    Vector2 mv = GetMouseDelta();
    bool moveu = (mv.x != 0 || mv.y != 0), soltou = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);

    if (GetKeyPressed() != 0) foco_teclado = true;
    if (moveu && GetTime() > 0.5) foco_teclado = false;
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) *opcao = (*opcao + 1) % 3;
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) *opcao = (*opcao + 2) % 3;

    for (int i = 0; i < 2; i++)
        if (em_cima(botao_menu(i))) {
            if (moveu) *opcao = i * 2;
            if (soltou) { *opcao = i * 2; ativar = i * 2; }
        }
    for (int d = 0; d < 3; d++)
        if (em_cima(botao_dificuldade(d))) {
            if (moveu) *opcao = 1;
            if (soltou) { *opcao = 1; dificuldade = d; }
        }

    if (*opcao == 1) {
        if ((IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) && dificuldade > 0) dificuldade--;
        if ((IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) && dificuldade < 2) dificuldade++;
        if (apertou(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) dificuldade = (dificuldade + 1) % 3;
    } else if (apertou(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        ativar = *opcao;
    }

    if (*opcao != opcao_antes) tocar(s_passar, 1.0f);
    if (dificuldade != dif_antes) tocar(s_clique, 0.9f + 0.15f * dificuldade);
    if (ativar >= 0) tocar(s_clique, 1.0f);
    if (ativar == 0) { nova_partida(pa, jogo); return TELA_JOGO; }
    if (ativar == 2) return TELA_COMO;
    return TELA_MENU;
}

int main(void) {
    srand((unsigned)time(NULL));
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(LARGURA / 2, ALTURA / 2, "Pizzalign");
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
    ChangeDirectory(GetApplicationDirectory());

    int mon = GetCurrentMonitor();
    float z = (GetMonitorHeight(mon) > 0) ? GetMonitorHeight(mon) * 0.88f / ALTURA : 0.75f;
    if (z > 1.4f) z = 1.4f;
    if (z < 0.5f) z = 0.5f;
    SetWindowSize((int)(LARGURA * z), (int)(ALTURA * z));
    Vector2 mp = GetMonitorPosition(mon);
    SetWindowPosition((int)(mp.x + (GetMonitorWidth(mon) - LARGURA * z) / 2), (int)(mp.y + (GetMonitorHeight(mon) - ALTURA * z) / 2 - 20));
    SetWindowMinSize(LARGURA / 2, ALTURA / 2);

    carregar_recursos();
    carregar_sons();

    EstadoJogo *jogo = estado_criar();
    if (!jogo) return 1;
    Partida pa;
    nova_partida(&pa, jogo);
    Recorde *recordes = recordes_carregar(ARQUIVO_RECORDES);

    Tela tela = TELA_MENU;
    int opcao = 0;
    bool sair = false, cursor_mao = false;

    while (!sair && !WindowShouldClose()) {
        float dt = GetFrameTime();
        Camera2D cam = camera_ajustada();
        bool digitando = (tela == TELA_JOGO && jogo->gameOver && !pa.salvo);
        int nivel = jogo->pontuacao / PONTOS_POR_NIVEL + 1;
        float intervalo = intervalo_de_queda(jogo->pontuacao);

        tecla_virtual = tecla_clicada;
        tecla_clicada = 0;
        sobre_clicavel = false;
        if (tecla_virtual != 0) tocar(s_clique, 1.0f);
        if (audio_ok) UpdateMusicStream(musica);

        if ((IsKeyPressed(KEY_Q) && !digitando) || tecla_virtual == KEY_Q) sair = true;
        if ((IsKeyPressed(KEY_N) && !digitando) || tecla_virtual == KEY_N) {
            som_ligado = !som_ligado;
            SetMasterVolume(som_ligado ? 1.0f : 0.0f);
        }
        if (apertou(KEY_F11)) alternar_tela_cheia();

        if (tela == TELA_MENU) {
            tela = atualizar_menu(&opcao, &pa, jogo);
        } else if (tela == TELA_COMO) {
            if (apertou(KEY_ENTER) || IsKeyPressed(KEY_M) || IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_SPACE)) {
                tocar(s_clique, 1.0f);
                tela = TELA_MENU;
            }
        } else {
            if ((IsKeyPressed(KEY_M) && !digitando) || tecla_virtual == KEY_M) tela = TELA_MENU;
            else if (!jogo->gameOver) atualizar_partida(&pa, jogo, dt, intervalo);
            else if (digitando) atualizar_nome(&pa, &recordes);
            else if (apertou(KEY_R)) nova_partida(&pa, jogo);

            atualizar_efeitos(&pa, jogo, dt, intervalo);
        }

        BeginDrawing();
        ClearBackground(TINTA);
        BeginMode2D(cam);
        desenhar_fundo();
        if (tela == TELA_MENU) {
            desenhar_menu(opcao, recordes);
        } else if (tela == TELA_COMO) {
            desenhar_como_jogar();
        } else {
            desenhar_jogador(0, &pa, jogo->gameOver);
            desenhar_jogador(1, &pa, jogo->gameOver);
            desenhar_adesivo(spr_mascote, LARGURA / 2.0f, 82, 138, pa.inclina);
            desenhar_mesa(jogo, pa.dispX, pa.dispY, pa.vez);
            desenhar_lateral_esquerda(jogo, nivel);
            desenhar_proximas(jogo);
            desenhar_efeitos(&pa);

            if (jogo->gameOver) desenhar_fim(recordes, &pa);
            else if (pa.pausado) desenhar_pausa();
            else {
                Dica dicas[3] = {{"P", NULL, "pausa", KEY_P}, {"M", NULL, "menu", KEY_M}, {"Q", NULL, "sair", KEY_Q}};
                desenhar_rodape(dicas, 3);
            }
        }
        EndMode2D();
        EndDrawing();

        if (sobre_clicavel != cursor_mao) {
            cursor_mao = sobre_clicavel;
            SetMouseCursor(cursor_mao ? MOUSE_CURSOR_POINTING_HAND : MOUSE_CURSOR_DEFAULT);
        }
    }

    recordes_liberar(recordes);
    estado_liberar(jogo);
    liberar_recursos();
    CloseWindow();
    return 0;
}