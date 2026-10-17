/*
 * PIZZALIGN 3D - jogo de cartas de pizza em 3D, inspirado no "Rise of the Half Moon"
 *                (Google Doodle). Feito em C com a biblioteca raylib.
 *
 * COMO COMPILAR (Windows, com o gcc do WinLibs; a pasta "raylib" fica ao lado deste arquivo):
 *     gcc -std=c99 -Wall -Wextra -O2 pizzalign3d.c -o pizzalign3d.exe -Iraylib/include -Lraylib/lib -lraylib -lopengl32 -lgdi32 -lwinmm
 *
 * COMO COMPILAR (Linux):
 *     gcc -std=c99 -Wall -Wextra -O2 pizzalign3d.c -o pizzalign3d -Iraylib/include raylib/lib/libraylib.a -lGL -lm -lpthread -ldl -lrt -lX11
 *
 * COMO JOGAR
 *   - Clique numa carta da sua mao (ou aperte 1, 2 ou 3) e depois numa casa vazia do tabuleiro.
 *   - Botao direito do mouse (ou setas) gira a camera; a roda do mouse aproxima e afasta.
 *   - Esc = pausa, R = regras.
 *
 * IDEIA DO JOGO
 *   Cada pizza tem 4 fatias. Existem 8 tipos de carta, que formam um ciclo:
 *   1 vazia -> 2 uma fatia (esq) -> 3 meia (esq) -> 4 falta a fatia da dir.
 *   -> 5 completa -> 6 falta a fatia da esq. -> 7 meia (dir) -> 8 uma fatia (dir)
 *   -> e volta para a 1.
 *
 * PONTUACAO (so pontua quando a carta jogada forma algo com as vizinhas):
 *   +1  fatias iguais       : carta igual encostada (cima, baixo ou lado)
 *   +2  fatias que completam: as duas cartas juntas formam uma pizza inteira
 *   +1  por carta           : 3 ou mais cartas seguidas do ciclo, em linha reta
 */

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ================================================================== */
/*  CONSTANTES                                                         */
/* ================================================================== */

#define N                3     /* o tabuleiro tem N x N casas */
#define NUM_CARTAS       8     /* tipos diferentes de carta */
#define COPIAS           4     /* quantas copias de cada carta no baralho */
#define TAM_BARALHO      (NUM_CARTAS * COPIAS)
#define TAM_MAO          3     /* cartas na mao de cada jogador */
#define NUM_FATIAS       4     /* fatias de cada pizza */

#define VAZIO            (-1)  /* casa sem carta / mao sem carta */
#define SEM_PAR          (-2)  /* carta que nao completa com nenhuma outra */

#define PONTOS_IGUAIS    1
#define PONTOS_COMPLETAM 2

#define LARGURA_TELA     1280
#define ALTURA_TELA      720

/* medidas do mundo 3D */
#define ESPACO_CASAS     2.4f  /* distancia entre o centro de duas casas */
#define ALTURA_TABULEIRO 0.21f /* altura em que as cartas pousam no tabuleiro */
#define PIZZA_RAIO       0.72f
#define DURACAO_VOO      0.8f  /* segundos que a carta leva para voar ate a casa */

#define MAX_POPUPS       8
#define MAX_CONFETE      140

/* ================================================================== */
/*  DADOS DAS CARTAS                                                   */
/* ================================================================== */

/* Nome de cada carta (o numero que o jogador ve e' o indice + 1). */
const char *NOME_CARTA[NUM_CARTAS] = {
    "Vazia",
    "1 fatia (esq.)",
    "Meia (esq.)",
    "Falta fatia dir.",
    "Completa",
    "Falta fatia esq.",
    "Meia (dir.)",
    "1 fatia (dir.)"
};

/* Com qual carta cada uma completa uma pizza inteira (indices a partir de 0). */
int PAR_QUE_COMPLETA[NUM_CARTAS] = {
    SEM_PAR,  /* 1 vazia            */
    5,        /* 2 uma fatia (esq)  + 6 falta fatia esq. */
    6,        /* 3 meia (esq)       + 7 meia (dir)       */
    7,        /* 4 falta fatia dir. + 8 uma fatia (dir)  */
    SEM_PAR,  /* 5 completa         */
    1,        /* 6 */
    2,        /* 7 */
    3         /* 8 */
};

/* Quais das 4 fatias existem em cada carta (1 = tem a fatia, 0 = nao tem).
 * As fatias estao na ordem: cima-esquerda, baixo-esquerda, baixo-direita, cima-direita.
 * Assim "esquerda" e "direita" dos nomes batem com o que se ve na mesa. */
int FATIAS[NUM_CARTAS][NUM_FATIAS] = {
    {0, 0, 0, 0},   /* 1 vazia */
    {1, 0, 0, 0},   /* 2 uma fatia (esq) */
    {1, 1, 0, 0},   /* 3 meia (esq) */
    {1, 1, 1, 0},   /* 4 falta a fatia da direita */
    {1, 1, 1, 1},   /* 5 completa */
    {0, 1, 1, 1},   /* 6 falta a fatia da esquerda */
    {0, 0, 1, 1},   /* 7 meia (dir) */
    {0, 0, 0, 1}    /* 8 uma fatia (dir) */
};

/* ================================================================== */
/*  CORES                                                              */
/* ================================================================== */

const Color COR_JOGADOR[2]  = { { 70, 175, 240, 255 }, { 250, 165, 45, 255 } };
const Color COR_MASSA       = { 232, 184, 112, 255 };
const Color COR_MASSA_LADO  = { 205, 158, 92, 255 };
const Color COR_BORDA       = { 214, 148, 78, 255 };
const Color COR_MOLHO       = { 204, 58, 36, 255 };
const Color COR_QUEIJO      = { 253, 210, 88, 255 };
const Color COR_PEPPERONI   = { 172, 40, 36, 255 };
const Color COR_PEPPERONI_2 = { 205, 78, 58, 255 };
const Color COR_MANJERICAO  = { 56, 156, 70, 255 };
const Color COR_CARTA       = { 34, 104, 70, 255 };   /* verde, como a bandeira da Italia */
const Color COR_COSTAS      = { 196, 40, 40, 255 };
const Color COR_BRANCO      = { 247, 244, 236, 255 };
const Color COR_PRATO       = { 238, 242, 246, 255 };

/* ================================================================== */
/*  ESTADO DO JOGO (regras, sem nada de grafico)                       */
/* ================================================================== */

typedef struct {
    int tabuleiro[N][N];       /* carta de cada casa (VAZIO se estiver livre) */
    int dono[N][N];            /* quem jogou a carta: 0 ou 1 */
    int baralho[TAM_BARALHO];
    int topo;                  /* posicao da proxima carta a ser comprada */
    int mao[2][TAM_MAO];       /* cartas de cada jogador */
    int pontos[2];
    int cartas_jogadas;        /* quantas casas ja foram preenchidas */
    int vez;                   /* de quem e' a vez: 0 ou 1 */
} Jogo;

/* ================================================================== */
/*  PONTUACAO                                                          */
/* ================================================================== */

/* A carta seguinte no ciclo (depois da ultima, volta para a primeira). */
int proxima_carta(int carta)
{
    return (carta + 1) % NUM_CARTAS;
}

/* Em uma linha de cartas, conta quantas cartas seguidas do ciclo
 * (crescente, da esquerda para a direita) passam pela posicao pos. */
int tamanho_da_sequencia(int linha[], int pos)
{
    int tamanho = 1;
    int i;

    /* anda para a direita enquanto a carta seguinte for a "proxima" do ciclo */
    i = pos;
    while (i + 1 < N && linha[i + 1] != VAZIO && linha[i + 1] == proxima_carta(linha[i])) {
        tamanho++;
        i++;
    }

    /* anda para a esquerda enquanto a carta atual for a "proxima" da anterior */
    i = pos;
    while (i - 1 >= 0 && linha[i - 1] != VAZIO && linha[i] == proxima_carta(linha[i - 1])) {
        tamanho++;
        i--;
    }
    return tamanho;
}

/* Pontos de sequencia crescente em uma linha, lida dos dois lados. */
int pontos_da_sequencia(int linha[], int pos)
{
    int invertida[N];
    int pontos = 0;
    int tamanho, i;

    tamanho = tamanho_da_sequencia(linha, pos);
    if (tamanho >= 3) {
        pontos += tamanho;
    }

    for (i = 0; i < N; i++) {
        invertida[i] = linha[N - 1 - i];
    }
    tamanho = tamanho_da_sequencia(invertida, N - 1 - pos);
    if (tamanho >= 3) {
        pontos += tamanho;
    }
    return pontos;
}

/* Quantos pontos a carta daria se fosse jogada na casa (lin, col).
 * A casa precisa estar vazia. O tabuleiro volta como estava. */
int pontuar_jogada(int tabuleiro[N][N], int lin, int col, int carta)
{
    int desloca_lin[4] = {-1, 1, 0, 0};   /* cima, baixo, esquerda, direita */
    int desloca_col[4] = {0, 0, -1, 1};
    int linha[N];
    int pontos = 0;
    int d, i;

    tabuleiro[lin][col] = carta;          /* coloca a carta so para testar */

    /* 1) vizinhos: fatias iguais e fatias que se completam */
    for (d = 0; d < 4; d++) {
        int viz_lin = lin + desloca_lin[d];
        int viz_col = col + desloca_col[d];
        int vizinha;

        if (viz_lin < 0 || viz_lin >= N || viz_col < 0 || viz_col >= N) {
            continue;
        }
        vizinha = tabuleiro[viz_lin][viz_col];
        if (vizinha == VAZIO) {
            continue;
        }
        if (vizinha == carta) {
            pontos += PONTOS_IGUAIS;
        } else if (vizinha == PAR_QUE_COMPLETA[carta]) {
            pontos += PONTOS_COMPLETAM;
        }
    }

    /* 2) sequencia crescente na linha da carta */
    for (i = 0; i < N; i++) {
        linha[i] = tabuleiro[lin][i];
    }
    pontos += pontos_da_sequencia(linha, col);

    /* 3) sequencia crescente na coluna da carta */
    for (i = 0; i < N; i++) {
        linha[i] = tabuleiro[i][col];
    }
    pontos += pontos_da_sequencia(linha, lin);

    tabuleiro[lin][col] = VAZIO;          /* tira a carta de teste */
    return pontos;
}

/* ================================================================== */
/*  REGRAS DO JOGO                                                     */
/* ================================================================== */

/* Tira uma carta do baralho (VAZIO se o baralho acabou). */
int comprar_carta(Jogo *jogo)
{
    int carta;

    if (jogo->topo >= TAM_BARALHO) {
        return VAZIO;
    }
    carta = jogo->baralho[jogo->topo];
    jogo->topo++;
    return carta;
}

/* Prepara uma partida nova: tabuleiro vazio, baralho embaralhado, maos distribuidas. */
void novo_jogo(Jogo *jogo)
{
    int i, j, sorteada, guardada;

    memset(jogo, 0, sizeof *jogo);

    for (i = 0; i < N; i++) {
        for (j = 0; j < N; j++) {
            jogo->tabuleiro[i][j] = VAZIO;
            jogo->dono[i][j] = -1;
        }
    }

    /* baralho: COPIAS de cada carta, depois embaralha */
    for (i = 0; i < TAM_BARALHO; i++) {
        jogo->baralho[i] = i % NUM_CARTAS;
    }
    for (i = TAM_BARALHO - 1; i > 0; i--) {
        sorteada = rand() % (i + 1);
        guardada = jogo->baralho[i];
        jogo->baralho[i] = jogo->baralho[sorteada];
        jogo->baralho[sorteada] = guardada;
    }

    /* cada jogador recebe TAM_MAO cartas */
    for (i = 0; i < 2; i++) {
        for (j = 0; j < TAM_MAO; j++) {
            jogo->mao[i][j] = comprar_carta(jogo);
        }
    }

    jogo->vez = rand() % 2;   /* sorteia quem comeca */
}

/* O jogador joga a carta da posicao 'indice' da mao na casa (lin, col).
 * Retorna os pontos que ele ganhou. */
int jogar_carta(Jogo *jogo, int jogador, int indice, int lin, int col)
{
    int carta = jogo->mao[jogador][indice];
    int pontos = pontuar_jogada(jogo->tabuleiro, lin, col, carta);

    jogo->tabuleiro[lin][col] = carta;
    jogo->dono[lin][col] = jogador;
    jogo->pontos[jogador] += pontos;
    jogo->cartas_jogadas++;
    jogo->mao[jogador][indice] = comprar_carta(jogo);
    return pontos;
}

/* ================================================================== */
/*  JOGADA DO COMPUTADOR (o pizzaiolo)                                 */
/* ================================================================== */

/* Maior pontuacao que o adversario poderia fazer em seguida, com qualquer carta. */
int melhor_resposta_possivel(int tabuleiro[N][N])
{
    int melhor = 0;
    int lin, col, carta, pontos;

    for (lin = 0; lin < N; lin++) {
        for (col = 0; col < N; col++) {
            if (tabuleiro[lin][col] != VAZIO) {
                continue;
            }
            for (carta = 0; carta < NUM_CARTAS; carta++) {
                pontos = pontuar_jogada(tabuleiro, lin, col, carta);
                if (pontos > melhor) {
                    melhor = pontos;
                }
            }
        }
    }
    return melhor;
}

/* O computador escolhe a carta e a casa.
 * Dificuldade 1: joga qualquer coisa.
 * Dificuldade 2: escolhe a jogada que mais pontua.
 * Dificuldade 3: igual a 2, mas tambem evita deixar pontos faceis para o adversario. */
void escolher_jogada(Jogo *jogo, int jogador, int dificuldade, int *indice, int *lin, int *col)
{
    int cand_indice[TAM_MAO * N * N];     /* jogadas empatadas como melhores */
    int cand_lin[TAM_MAO * N * N];
    int cand_col[TAM_MAO * N * N];
    int qtd = 0;
    int melhor_valor = -1000000;
    int i, l, c, escolhida;

    for (i = 0; i < TAM_MAO; i++) {
        int carta = jogo->mao[jogador][i];
        if (carta == VAZIO) {
            continue;
        }
        for (l = 0; l < N; l++) {
            for (c = 0; c < N; c++) {
                int valor;

                if (jogo->tabuleiro[l][c] != VAZIO) {
                    continue;
                }

                if (dificuldade == 1) {
                    valor = 0;
                } else {
                    valor = pontuar_jogada(jogo->tabuleiro, l, c, carta) * 10;
                    if (dificuldade == 3 && jogo->cartas_jogadas + 1 < N * N) {
                        jogo->tabuleiro[l][c] = carta;       /* testa a jogada */
                        valor -= melhor_resposta_possivel(jogo->tabuleiro) * 6;
                        jogo->tabuleiro[l][c] = VAZIO;       /* desfaz */
                    }
                }

                if (valor > melhor_valor) {      /* achou uma jogada melhor: recomeca a lista */
                    melhor_valor = valor;
                    qtd = 0;
                }
                if (valor == melhor_valor) {     /* guarda jogadas empatadas */
                    cand_indice[qtd] = i;
                    cand_lin[qtd] = l;
                    cand_col[qtd] = c;
                    qtd++;
                }
            }
        }
    }

    escolhida = rand() % qtd;                    /* sorteia entre as melhores */
    *indice = cand_indice[escolhida];
    *lin = cand_lin[escolhida];
    *col = cand_col[escolhida];
}

/* Escolhe uma frase do pizzaiolo conforme a ultima jogada dele e a do jogador. */
const char *fala_do_chef(int pontos_chef, int pontos_jogador)
{
    const char *animado[]       = {"Mamma mia, que jogada!", "Isso é pizza de verdade!", "Ha! Essa foi saborosa!"};
    const char *impressionado[] = {"Eita! Você queimou minha massa...", "Hmm, nada mal.", "Na próxima eu giro melhor!"};
    const char *calmo[]         = {"Deixa a massa descansar...", "O forno ainda está esquentando.", "Mais uma fatia, por favor!"};

    if (pontos_chef >= 3) {
        return animado[rand() % 3];
    }
    if (pontos_jogador >= 3) {
        return impressionado[rand() % 3];
    }
    return calmo[rand() % 3];
}

/* ================================================================== */
/*  ESTADO DO PROGRAMA (telas, animacoes, camera)                      */
/* ================================================================== */

typedef enum { TELA_MENU, TELA_DIFICULDADE, TELA_REGRAS, TELA_JOGO } Tela;

/* O que esta acontecendo durante a partida */
typedef enum {
    FASE_TROCA,     /* 2 jogadores: tela de "passe o mouse para o proximo" (maos escondidas) */
    FASE_ESCOLHER,  /* um humano escolhe carta e casa */
    FASE_PENSAR,    /* o pizzaiolo esta pensando */
    FASE_VOO,       /* uma carta esta voando ate a casa */
    FASE_FIM        /* tabuleiro cheio */
} Fase;

/* A carta que esta voando ate o tabuleiro */
typedef struct {
    int ativo;
    int carta, dono, indice_mao, lin, col, pontos;
    Vector3 de, para;
    float tempo;
} Voo;

/* O "+3" que sobe quando alguem pontua */
typedef struct {
    int ativo;
    Vector3 pos;
    int pontos;
    int dono;
    float tempo;
} Popup;

/* Pedacinho de confete da comemoracao */
typedef struct {
    int ativo;
    Vector3 pos, vel;
    Color cor;
    float giro, vel_giro;
} Confete;

typedef struct {
    Tela tela;
    Tela tela_anterior;            /* para a tela de regras saber para onde voltar */
    Fase fase;

    int contra_computador;         /* 1 = o jogador 2 e' o pizzaiolo */
    int dificuldade;               /* 1 facil, 2 medio, 3 dificil */
    char nome[2][20];
    Jogo jogo;
    int placar[2];                 /* pontos mostrados na tela (atualiza quando a carta pousa) */

    int selecionada;               /* carta da mao escolhida (-1 = nenhuma) */
    int mao_sob_mouse;             /* carta da mao sob o mouse (-1 = nenhuma) */
    int lin_sob_mouse, col_sob_mouse;  /* casa sob o mouse (-1 = nenhuma) */
    float elevacao[TAM_MAO];       /* quanto cada carta da mao esta levantada */
    float surgir[TAM_MAO];         /* 0 a 1: carta nova aparecendo na mao */
    float pulo[N][N];              /* "quicada" da carta que acabou de pousar */

    Voo voo;
    Popup popups[MAX_POPUPS];
    Confete confete[MAX_CONFETE];

    char fala[80];                 /* o que o pizzaiolo esta dizendo */
    float tempo_fala;
    int humor_chef;                /* 1 sorrindo, 2 comemorando, -1 triste */
    int ultimos_pontos_humano;

    float tempo;                   /* segundos desde que o programa abriu */
    float espera;                  /* contagem do pizzaiolo "pensando" */
    float tempo_fim;               /* ha quanto tempo a partida acabou */
    int pausado;
    int sair;

    float yaw, pitch, distancia;   /* posicao da camera */
} App;

/* O que o jogador fez neste quadro (mouse e teclado) */
typedef struct {
    Vector2 mouse;
    Vector2 arrasto;               /* quanto o mouse andou com o botao direito apertado */
    float roda;                    /* rolagem do mouse */
    int clicou;                    /* botao esquerdo apertado neste quadro */
    int girar;                     /* -1, 0 ou 1: setas do teclado */
    int esc;
    int tecla_r;
    int tecla_carta;               /* 1, 2 ou 3 se apertou essa tecla; senao 0 */
} Entrada;

Shader sombreador;                 /* faz a luz das pizzas */

/* ================================================================== */
/*  LUZ (shader)                                                       */
/* ================================================================== */

/* O raylib nao tem luz pronta. Este pequeno programa da GPU calcula a luz de cada
 * pedaco da figura: cada triangulo fica mais claro ou mais escuro conforme a
 * direcao para a qual ele aponta. E' isso que da a sensacao de volume. */
const char *CODIGO_VERTICE =
    "#version 330\n"
    "in vec3 vertexPosition;\n"
    "in vec4 vertexColor;\n"
    "uniform mat4 mvp;\n"
    "out vec3 posicao;\n"
    "out vec4 cor;\n"
    "void main() {\n"
    "    posicao = vertexPosition;\n"
    "    cor = vertexColor;\n"
    "    gl_Position = mvp * vec4(vertexPosition, 1.0);\n"
    "}\n";

const char *CODIGO_FRAGMENTO =
    "#version 330\n"
    "in vec3 posicao;\n"
    "in vec4 cor;\n"
    "uniform vec3 posicaoCamera;\n"
    "out vec4 corFinal;\n"
    "void main() {\n"
    "    vec3 normal = normalize(cross(dFdx(posicao), dFdy(posicao)));\n"
    "    vec3 paraCamera = normalize(posicaoCamera - posicao);\n"
    "    if (dot(normal, paraCamera) < 0.0) normal = -normal;\n"
    "    vec3 luz1 = normalize(vec3(-0.45, 1.0, 0.55));\n"
    "    vec3 luz2 = normalize(vec3(0.7, 0.5, -0.3));\n"
    "    float difusa = max(dot(normal, luz1), 0.0) * 0.62 + max(dot(normal, luz2), 0.0) * 0.16;\n"
    "    vec3 meio = normalize(luz1 + paraCamera);\n"
    "    float brilho = pow(max(dot(normal, meio), 0.0), 40.0) * 0.16;\n"
    "    corFinal = vec4(cor.rgb * (0.42 + difusa) + vec3(brilho), cor.a);\n"
    "}\n";

void carregar_luz(void)
{
    sombreador = LoadShaderFromMemory(CODIGO_VERTICE, CODIGO_FRAGMENTO);
}

/* ================================================================== */
/*  GEOMETRIA: pedacinhos de pizza                                     */
/* ================================================================== */

/* Ponto de um circulo (visto de cima) com 'raio' e 'angulo', na altura dada. */
Vector3 ponto_polar(float raio, float angulo, float altura)
{
    Vector3 p;
    p.x = raio * cosf(angulo);
    p.y = altura;
    p.z = raio * sinf(angulo);
    return p;
}

/* Tampa de uma fatia (triangulos saindo do centro). */
void desenhar_topo_fatia(float raio, float ini, float fim, float altura, Color cor)
{
    int passos = 6;
    int i;
    Vector3 centro = { 0.0f, altura, 0.0f };

    for (i = 0; i < passos; i++) {
        float a = ini + (fim - ini) * i / passos;
        float b = ini + (fim - ini) * (i + 1) / passos;
        DrawTriangle3D(centro, ponto_polar(raio, a, altura), ponto_polar(raio, b, altura), cor);
    }
}

/* Parede curva da fatia (o "lado" redondo da massa). */
void desenhar_lateral_arco(float raio, float ini, float fim, float y_baixo, float y_cima, Color cor)
{
    int passos = 6;
    int i;

    for (i = 0; i < passos; i++) {
        float a = ini + (fim - ini) * i / passos;
        float b = ini + (fim - ini) * (i + 1) / passos;
        Vector3 a0 = ponto_polar(raio, a, y_baixo);
        Vector3 a1 = ponto_polar(raio, a, y_cima);
        Vector3 b0 = ponto_polar(raio, b, y_baixo);
        Vector3 b1 = ponto_polar(raio, b, y_cima);
        DrawTriangle3D(a0, a1, b1, cor);
        DrawTriangle3D(a0, b1, b0, cor);
    }
}

/* Parede reta da fatia (o corte, do centro ate a borda). */
void desenhar_lado_radial(float raio, float angulo, float y_baixo, float y_cima, Color cor)
{
    Vector3 c0 = { 0.0f, y_baixo, 0.0f };
    Vector3 c1 = { 0.0f, y_cima, 0.0f };
    Vector3 p0 = ponto_polar(raio, angulo, y_baixo);
    Vector3 p1 = ponto_polar(raio, angulo, y_cima);

    DrawTriangle3D(c0, c1, p1, cor);
    DrawTriangle3D(c0, p1, p0, cor);
}

/* Um ponto na superficie de um "cano" curvo (usado na borda da pizza). */
Vector3 ponto_do_tubo(float raio_arco, float raio_tubo, float altura, float angulo, float volta)
{
    float raio = raio_arco + raio_tubo * cosf(volta);
    float y = altura + raio_tubo * sinf(volta);
    return ponto_polar(raio, angulo, y);
}

/* Borda (crosta) da fatia: um cano curvo que acompanha a beirada. */
void desenhar_tubo_arco(float raio_arco, float raio_tubo, float altura, float ini, float fim, Color cor)
{
    int passos = 8;
    int lados = 8;
    int i, j;

    for (i = 0; i < passos; i++) {
        float a = ini + (fim - ini) * i / passos;
        float b = ini + (fim - ini) * (i + 1) / passos;
        for (j = 0; j < lados; j++) {
            float v0 = 2.0f * PI * j / lados;
            float v1 = 2.0f * PI * (j + 1) / lados;
            Vector3 p00 = ponto_do_tubo(raio_arco, raio_tubo, altura, a, v0);
            Vector3 p01 = ponto_do_tubo(raio_arco, raio_tubo, altura, a, v1);
            Vector3 p10 = ponto_do_tubo(raio_arco, raio_tubo, altura, b, v0);
            Vector3 p11 = ponto_do_tubo(raio_arco, raio_tubo, altura, b, v1);
            DrawTriangle3D(p00, p01, p11, cor);
            DrawTriangle3D(p00, p11, p10, cor);
        }
    }
}

/* Uma rodela de pepperoni. */
void desenhar_pepperoni(float raio_pos, float angulo, float altura, float tamanho)
{
    Vector3 pos = ponto_polar(raio_pos, angulo, altura);
    DrawCylinder(pos, tamanho, tamanho, 0.035f, 10, COR_PEPPERONI);
    pos.y += 0.003f;
    DrawCylinder(pos, tamanho * 0.45f, tamanho * 0.45f, 0.035f, 8, COR_PEPPERONI_2);
}

/* Uma folhinha de manjericao (uma bolinha achatada). */
void desenhar_manjericao(float raio_pos, float angulo, float altura, float giro)
{
    Vector3 pos = ponto_polar(raio_pos, angulo, altura);

    rlPushMatrix();
    rlTranslatef(pos.x, pos.y, pos.z);
    rlRotatef(giro, 0.0f, 1.0f, 0.0f);
    rlScalef(1.0f, 0.28f, 0.6f);
    DrawSphereEx((Vector3){ 0.0f, 0.0f, 0.0f }, 0.1f, 4, 6, COR_MANJERICAO);
    rlPopMatrix();
}

/* Uma fatia da pizza. 's' vai de 0 a 3 (veja a tabela FATIAS); y0 e' a altura do prato. */
void desenhar_fatia(int s, float y0)
{
    float folga = 0.035f;                            /* espaco entre as fatias */
    float ini = (180.0f - 90.0f * s) * DEG2RAD + folga;
    float fim = (270.0f - 90.0f * s) * DEG2RAD - folga;
    float meio = (ini + fim) / 2.0f;
    float topo_massa = y0 + 0.12f;

    /* massa */
    desenhar_topo_fatia(PIZZA_RAIO, ini, fim, topo_massa, COR_MASSA);
    desenhar_lateral_arco(PIZZA_RAIO, ini, fim, y0, topo_massa, COR_MASSA_LADO);
    desenhar_lado_radial(PIZZA_RAIO, ini, y0, topo_massa, COR_MASSA_LADO);
    desenhar_lado_radial(PIZZA_RAIO, fim, y0, topo_massa, COR_MASSA_LADO);

    /* molho e queijo (um pouco menores, para a massa aparecer na beirada) */
    desenhar_topo_fatia(PIZZA_RAIO - 0.14f, ini + 0.02f, fim - 0.02f, topo_massa + 0.006f, COR_MOLHO);
    desenhar_topo_fatia(PIZZA_RAIO - 0.24f, ini + 0.05f, fim - 0.05f, topo_massa + 0.014f, COR_QUEIJO);

    /* borda */
    desenhar_tubo_arco(PIZZA_RAIO - 0.03f, 0.085f, topo_massa - 0.01f, ini + 0.02f, fim - 0.02f, COR_BORDA);

    /* coberturas */
    desenhar_pepperoni(0.42f, meio - 0.24f, topo_massa + 0.014f, 0.115f);
    desenhar_pepperoni(0.26f, meio + 0.30f, topo_massa + 0.014f, 0.085f);
    desenhar_manjericao(0.50f, meio + 0.24f, topo_massa + 0.03f, 30.0f * s);
}

/* A pizza da carta: so as fatias que a carta tem. */
void desenhar_pizza(int carta, float y0)
{
    int s;
    for (s = 0; s < NUM_FATIAS; s++) {
        if (FATIAS[carta][s]) {
            desenhar_fatia(s, y0);
        }
    }
}

/* ================================================================== */
/*  GEOMETRIA: carta                                                   */
/* ================================================================== */

/* Desenha uma carta no mundo 3D. carta == VAZIO desenha o verso.
 * 'moldura' e' a cor da borda (a cor de quem jogou). */
void desenhar_carta(int carta, Vector3 pos, float giro, float escala, Color moldura)
{
    Vector3 centro_base = { 0.0f, 0.05f, 0.0f };
    Vector3 centro_corpo = { 0.0f, 0.07f, 0.0f };

    rlPushMatrix();
    rlTranslatef(pos.x, pos.y, pos.z);
    rlRotatef(giro, 0.0f, 1.0f, 0.0f);
    rlScalef(escala, escala, escala);

    DrawCube(centro_base, 2.16f, 0.10f, 2.16f, moldura);

    if (carta == VAZIO) {
        /* verso da carta: vermelho com um alvo bege no meio */
        DrawCube(centro_corpo, 1.96f, 0.12f, 1.96f, COR_COSTAS);
        DrawCylinder((Vector3){ 0.0f, 0.13f, 0.0f }, 0.72f, 0.72f, 0.02f, 28, COR_BRANCO);
        DrawCylinder((Vector3){ 0.0f, 0.14f, 0.0f }, 0.5f, 0.5f, 0.02f, 28, COR_COSTAS);
        DrawCylinder((Vector3){ 0.0f, 0.15f, 0.0f }, 0.26f, 0.26f, 0.02f, 20, COR_BRANCO);
    } else {
        DrawCube(centro_corpo, 1.96f, 0.12f, 1.96f, COR_CARTA);
        DrawCylinder((Vector3){ 0.0f, 0.13f, 0.0f }, 0.88f, 0.88f, 0.05f, 36, COR_BRANCO);
        DrawCylinder((Vector3){ 0.0f, 0.18f, 0.0f }, 0.78f, 0.78f, 0.004f, 36, COR_PRATO);
        desenhar_pizza(carta, 0.185f);
    }

    rlPopMatrix();
}

/* ================================================================== */
/*  POSICOES NO MUNDO 3D                                               */
/* ================================================================== */

/* Centro de uma casa do tabuleiro. */
Vector3 posicao_casa(int lin, int col)
{
    Vector3 p;
    p.x = (col - 1) * ESPACO_CASAS;
    p.y = ALTURA_TABULEIRO;
    p.z = (lin - 1) * ESPACO_CASAS;
    return p;
}

/* Onde fica cada carta da mao do jogador (perto da camera). */
Vector3 posicao_mao(int i)
{
    Vector3 p = { (i - 1) * 2.8f, 0.0f, 6.0f };
    return p;
}

/* Onde fica o pizzaiolo (do outro lado da mesa). */
Vector3 posicao_chef(void)
{
    Vector3 p = { 0.0f, 0.0f, -7.6f };
    return p;
}

/* De onde o pizzaiolo "joga" as cartas. */
Vector3 posicao_carta_do_chef(void)
{
    Vector3 p = { 2.8f, 2.6f, -5.6f };
    return p;
}

/* O monte de cartas, ao lado da mao. */
Vector3 posicao_monte(void)
{
    Vector3 p = { 7.2f, 0.0f, 3.6f };
    return p;
}

/* Suaviza um movimento: comeca devagar, acelera e termina devagar. t vai de 0 a 1. */
float suavizar(float t)
{
    return t * t * (3.0f - 2.0f * t);
}

/* Posicao da carta que esta voando, no instante atual. */
Vector3 posicao_do_voo(const Voo *voo)
{
    float t = voo->tempo / DURACAO_VOO;
    float k;
    Vector3 p;

    if (t > 1.0f) {
        t = 1.0f;
    }
    k = suavizar(t);
    p = Vector3Lerp(voo->de, voo->para, k);
    p.y += sinf(PI * t) * 2.6f;     /* faz um arco no ar */
    return p;
}

/* ================================================================== */
/*  GEOMETRIA: ambiente (mesa, parede, forno)                          */
/* ================================================================== */

void desenhar_mesa(int com_tabuleiro)
{
    int i, j;
    Color vermelho = { 206, 52, 48, 255 };
    Color branco = { 246, 240, 228, 255 };

    /* chao */
    DrawCube((Vector3){ 0.0f, -3.8f, 0.0f }, 80.0f, 0.4f, 70.0f, (Color){ 58, 40, 36, 255 });

    /* tampo de madeira */
    DrawCube((Vector3){ 0.0f, -0.37f, 1.3f }, 26.0f, 0.7f, 16.8f, (Color){ 128, 80, 46, 255 });

    /* toalha xadrez */
    for (i = 0; i < 20; i++) {
        for (j = 0; j < 13; j++) {
            Vector3 centro = { -11.4f + i * 1.2f, -0.01f, -5.8f + j * 1.2f + 1.3f };
            DrawCube(centro, 1.2f, 0.02f, 1.2f, ((i + j) % 2 == 0) ? vermelho : branco);
        }
    }

    /* tabuleiro de madeira, com uma "vaga" para cada carta */
    if (com_tabuleiro) {
        DrawCube((Vector3){ 0.0f, 0.10f, 0.0f }, 8.6f, 0.20f, 8.6f, (Color){ 190, 132, 80, 255 });
        DrawCube((Vector3){ 0.0f, 0.205f, 0.0f }, 8.3f, 0.02f, 8.3f, (Color){ 160, 108, 64, 255 });
    }
}

/* Uma "vaga" do tabuleiro; fica clara quando o mouse esta em cima ou quando da para jogar. */
void desenhar_vaga(Vector3 pos, float brilho)
{
    Color base = { 128, 84, 52, 255 };
    Color clara = { 255, 214, 110, 255 };
    Color cor;

    cor.r = (unsigned char)(base.r + (clara.r - base.r) * brilho);
    cor.g = (unsigned char)(base.g + (clara.g - base.g) * brilho);
    cor.b = (unsigned char)(base.b + (clara.b - base.b) * brilho);
    cor.a = 255;
    DrawCube((Vector3){ pos.x, 0.215f, pos.z }, 2.26f, 0.02f, 2.26f, cor);
}

void desenhar_parede_e_forno(float tempo)
{
    int linha, i;
    Color tijolo = { 138, 60, 46, 255 };
    Color argamassa = { 196, 168, 142, 255 };
    float fogo = 0.25f * sinf(tempo * 9.0f);

    /* parede de tijolos */
    DrawCube((Vector3){ 0.0f, 5.0f, -14.0f }, 70.0f, 20.0f, 0.6f, tijolo);
    for (linha = 0; linha < 16; linha++) {
        float y = -3.4f + linha * 0.8f;
        DrawCube((Vector3){ 0.0f, y, -13.68f }, 70.0f, 0.07f, 0.06f, argamassa);
        for (i = -18; i <= 18; i++) {
            float x = i * 1.6f + ((linha % 2) ? 0.8f : 0.0f);
            DrawCube((Vector3){ x, y + 0.4f, -13.68f }, 0.07f, 0.8f, 0.06f, argamassa);
        }
    }

    /* forno de pizza (uma meia-esfera de tijolo com a boca escura) */
    DrawSphereEx((Vector3){ -10.5f, -0.5f, -11.0f }, 3.6f, 16, 22, (Color){ 176, 84, 58, 255 });
    DrawCube((Vector3){ -10.5f, 0.6f, -7.7f }, 2.6f, 2.0f, 0.5f, (Color){ 30, 20, 18, 255 });
    DrawCylinderEx((Vector3){ -10.5f, 1.6f, -7.75f }, (Vector3){ -10.5f, 1.6f, -7.3f }, 1.3f, 1.3f, 20, (Color){ 30, 20, 18, 255 });

    /* chamas */
    DrawCylinder((Vector3){ -11.1f, -0.35f, -7.4f }, 0.0f, 0.45f, 1.3f + fogo, 8, (Color){ 255, 120, 20, 255 });
    DrawCylinder((Vector3){ -10.4f, -0.35f, -7.4f }, 0.0f, 0.55f, 1.7f - fogo, 8, (Color){ 255, 160, 30, 255 });
    DrawCylinder((Vector3){ -9.8f, -0.35f, -7.4f }, 0.0f, 0.4f, 1.1f + fogo, 8, (Color){ 255, 90, 20, 255 });
    DrawCylinder((Vector3){ -10.4f, -0.35f, -7.35f }, 0.0f, 0.3f, 0.9f + fogo, 8, (Color){ 255, 220, 80, 255 });
}

/* ================================================================== */
/*  GEOMETRIA: o pizzaiolo                                             */
/* ================================================================== */

/* Como o pizzaiolo esta posando neste quadro. */
typedef struct {
    Vector3 base;        /* onde ele esta (altura do tampo da mesa) */
    Vector3 mao_dir;     /* para onde vai a mao direita dele */
    float tempo;         /* para a animacao parada (respirar, piscar) */
    int humor;           /* 1 sorrindo, 2 comemorando, -1 triste */
    float pulo;          /* altura do pulinho de comemoracao */
} PosaChef;

/* Um olho: bola branca, bolinha preta e brilho. 'piscada' vai de 0 (fechado) a 1 (aberto). */
void desenhar_olho(float x, float piscada)
{
    rlPushMatrix();
    rlTranslatef(x, 0.18f, 0.62f);
    rlScalef(1.0f, piscada, 1.0f);
    DrawSphereEx((Vector3){ 0.0f, 0.0f, 0.0f }, 0.16f, 10, 10, WHITE);
    DrawSphereEx((Vector3){ 0.0f, -0.02f, 0.11f }, 0.085f, 8, 8, (Color){ 30, 24, 22, 255 });
    DrawSphereEx((Vector3){ 0.03f, 0.02f, 0.18f }, 0.025f, 4, 4, WHITE);
    rlPopMatrix();
}

/* Um lado do bigode: uma bola esticada, inclinada para cima na ponta. */
void desenhar_bigode(float lado)
{
    Color cor = { 78, 46, 30, 255 };

    rlPushMatrix();
    rlTranslatef(lado * 0.30f, -0.30f, 0.64f);
    rlRotatef(lado * -18.0f, 0.0f, 0.0f, 1.0f);
    rlScalef(1.2f, 0.5f, 0.6f);
    DrawSphereEx((Vector3){ 0.0f, 0.0f, 0.0f }, 0.3f, 10, 12, cor);
    rlPopMatrix();

    /* ponta enrolada */
    DrawSphereEx((Vector3){ lado * 0.66f, -0.18f, 0.55f }, 0.11f, 6, 8, cor);
}

/* A sobrancelha, um tracinho um pouco inclinado. */
void desenhar_sobrancelha(float lado, float inclinacao)
{
    rlPushMatrix();
    rlTranslatef(lado * 0.27f, 0.46f, 0.66f);
    rlRotatef(lado * inclinacao, 0.0f, 0.0f, 1.0f);
    DrawCube((Vector3){ 0.0f, 0.0f, 0.0f }, 0.3f, 0.07f, 0.07f, (Color){ 60, 36, 26, 255 });
    rlPopMatrix();
}

/* A boca: sorriso (humor > 0) ou cara triste (humor < 0). */
void desenhar_boca(int humor)
{
    int i;
    Color escuro = { 120, 30, 36, 255 };

    if (humor == 2) {
        /* sorrisao de boca aberta */
        rlPushMatrix();
        rlTranslatef(0.0f, -0.5f, 0.6f);
        rlScalef(1.0f, 0.7f, 0.5f);
        DrawSphereEx((Vector3){ 0.0f, 0.0f, 0.0f }, 0.2f, 8, 10, escuro);
        rlPopMatrix();
        return;
    }
    for (i = -3; i <= 3; i++) {
        float x = i * 0.065f;
        float curva = 1.0f - (x / 0.2f) * (x / 0.2f);
        float y = -0.5f - ((humor >= 0) ? 1.0f : -1.0f) * 0.07f * curva;
        DrawSphereEx((Vector3){ x, y, 0.64f }, 0.035f, 4, 6, escuro);
    }
}

/* Desenha o pizzaiolo inteiro, de pe do outro lado da mesa. */
void desenhar_chef(PosaChef p)
{
    Color jaqueta = { 248, 246, 240, 255 };
    Color pele = { 250, 206, 170, 255 };
    Color rosa = { 246, 150, 140, 255 };
    Color lenco = { 214, 44, 44, 255 };
    Color botao = { 40, 34, 32, 255 };
    Vector3 b = p.base;
    float respiro = 0.04f * sinf(p.tempo * 2.2f);
    float olhos = 1.0f;
    float resto = fmodf(p.tempo, 4.2f);
    Vector3 ombro_esq, ombro_dir, mao_esq;
    int i;

    b.y += p.pulo;
    if (resto > 4.0f) {
        olhos = 0.1f;            /* pisca de vez em quando */
    }

    /* corpo: jaqueta de cozinheiro */
    DrawCylinder((Vector3){ b.x, b.y - 3.6f, b.z }, 0.95f, 1.3f, 4.2f + respiro, 28, jaqueta);
    for (i = 0; i < 3; i++) {
        DrawSphereEx((Vector3){ b.x - 0.28f, b.y - 0.55f - i * 0.7f, b.z + 1.0f - i * 0.05f }, 0.08f, 6, 6, botao);
        DrawSphereEx((Vector3){ b.x + 0.28f, b.y - 0.55f - i * 0.7f, b.z + 1.0f - i * 0.05f }, 0.08f, 6, 6, botao);
    }

    /* lenco no pescoco */
    DrawCylinder((Vector3){ b.x, b.y + 0.42f, b.z }, 0.52f, 0.66f, 0.26f, 20, lenco);
    DrawSphereEx((Vector3){ b.x, b.y + 0.45f, b.z + 0.62f }, 0.17f, 6, 8, lenco);

    /* bracos */
    ombro_esq = (Vector3){ b.x - 1.0f, b.y + 0.15f, b.z };
    ombro_dir = (Vector3){ b.x + 1.0f, b.y + 0.15f, b.z };
    mao_esq = (Vector3){ b.x - 1.8f, b.y + 0.28f, b.z + 1.5f };
    DrawCapsule(ombro_esq, mao_esq, 0.3f, 8, 8, jaqueta);
    DrawSphereEx(mao_esq, 0.3f, 10, 10, pele);
    DrawCapsule(ombro_dir, p.mao_dir, 0.3f, 8, 8, jaqueta);
    DrawSphereEx(p.mao_dir, 0.3f, 10, 10, pele);

    /* cabeca: tudo e' desenhado em relacao ao centro dela */
    rlPushMatrix();
    rlTranslatef(b.x, b.y + 1.55f + respiro, b.z);
    rlRotatef(7.0f * sinf(p.tempo * 0.7f), 0.0f, 1.0f, 0.0f);

    DrawSphereEx((Vector3){ 0.0f, 0.0f, 0.0f }, 0.78f, 16, 20, pele);
    DrawSphereEx((Vector3){ -0.76f, 0.0f, 0.0f }, 0.2f, 8, 8, pele);      /* orelhas */
    DrawSphereEx((Vector3){ 0.76f, 0.0f, 0.0f }, 0.2f, 8, 8, pele);
    DrawSphereEx((Vector3){ -0.46f, -0.12f, 0.58f }, 0.17f, 8, 8, rosa);  /* bochechas */
    DrawSphereEx((Vector3){ 0.46f, -0.12f, 0.58f }, 0.17f, 8, 8, rosa);
    DrawSphereEx((Vector3){ 0.0f, -0.05f, 0.78f }, 0.2f, 10, 10, (Color){ 244, 172, 150, 255 });  /* nariz */

    desenhar_olho(-0.27f, olhos);
    desenhar_olho(0.27f, olhos);
    desenhar_sobrancelha(-1.0f, (p.humor < 0) ? -20.0f : -6.0f);
    desenhar_sobrancelha(1.0f, (p.humor < 0) ? -20.0f : -6.0f);
    desenhar_bigode(-1.0f);
    desenhar_bigode(1.0f);
    desenhar_boca(p.humor);

    /* chapeu de cozinheiro */
    DrawCylinder((Vector3){ 0.0f, 0.62f, 0.0f }, 0.8f, 0.8f, 0.26f, 24, jaqueta);
    DrawCylinder((Vector3){ 0.0f, 0.86f, 0.0f }, 0.74f, 0.64f, 0.7f, 24, jaqueta);
    DrawSphereEx((Vector3){ -0.4f, 1.62f, 0.0f }, 0.42f, 10, 12, jaqueta);
    DrawSphereEx((Vector3){ 0.4f, 1.62f, 0.0f }, 0.42f, 10, 12, jaqueta);
    DrawSphereEx((Vector3){ 0.0f, 1.82f, 0.05f }, 0.5f, 10, 12, jaqueta);
    DrawSphereEx((Vector3){ 0.0f, 1.62f, 0.42f }, 0.34f, 8, 10, jaqueta);

    rlPopMatrix();
}

/* ================================================================== */
/*  CAMERA                                                             */
/* ================================================================== */

/* A camera gira em volta da mesa; yaw = para os lados, pitch = para cima e para baixo. */
Camera3D calcular_camera(const App *app)
{
    Camera3D cam = { 0 };
    float horizontal = app->distancia * cosf(app->pitch);

    cam.target = (Vector3){ 0.0f, 0.4f, 1.0f };
    cam.position.x = cam.target.x + horizontal * sinf(app->yaw);
    cam.position.y = cam.target.y + app->distancia * sinf(app->pitch);
    cam.position.z = cam.target.z + horizontal * cosf(app->yaw);
    cam.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    cam.fovy = 50.0f;
    cam.projection = CAMERA_PERSPECTIVE;
    return cam;
}

/* ================================================================== */
/*  CENAS 3D                                                           */
/* ================================================================== */

/* Quem esta com a mao na frente da camera: o humano (no modo 1 jogador)
 * ou o jogador da vez (no modo 2 jogadores). */
int dono_da_mao(const App *app)
{
    if (app->contra_computador) {
        return 0;
    }
    return app->jogo.vez;
}

/* Pose do pizzaiolo conforme o que esta acontecendo no jogo. */
PosaChef pose_do_chef(const App *app)
{
    PosaChef p;
    Vector3 base = posicao_chef();

    p.base = base;
    p.tempo = app->tempo;
    p.humor = app->humor_chef;
    p.pulo = 0.0f;
    /* mao em descanso, em cima da mesa */
    p.mao_dir = (Vector3){ base.x + 1.8f, base.y + 0.28f, base.z + 1.5f };

    if (app->tela == TELA_JOGO) {
        if (app->fase == FASE_PENSAR && app->contra_computador) {
            /* mao no queixo, pensando */
            p.mao_dir = (Vector3){ base.x + 0.55f, base.y + 1.15f, base.z + 1.0f };
        }
        if (app->fase == FASE_VOO && app->voo.ativo && app->voo.dono == 1 && app->contra_computador) {
            p.mao_dir = posicao_do_voo(&app->voo);
            p.mao_dir.y += 0.5f;
        }
        if (app->fase == FASE_FIM && p.humor == 2) {
            p.pulo = 0.35f * fabsf(sinf(app->tempo * 6.0f));
        }
    }
    return p;
}

/* Onde fica cada uma das 8 cartas na tela de regras (2 fileiras de 4). */
Vector3 posicao_carta_das_regras(int i)
{
    Vector3 p;
    p.x = ((i % 4) - 1.5f) * 3.7f;
    p.y = 0.04f;
    p.z = (i / 4) * 3.3f - 3.6f;
    return p;
}

/* Cartas girando em cima da mesa, na tela de regras. */
void desenhar_cartas_das_regras(const App *app)
{
    int i;

    for (i = 0; i < NUM_CARTAS; i++) {
        Vector3 pos = posicao_carta_das_regras(i);
        float giro = 14.0f * sinf(app->tempo * 0.9f + i);

        pos.y += 0.08f + 0.08f * sinf(app->tempo * 1.4f + i * 0.8f);
        desenhar_carta(i, pos, giro, 1.0f, COR_JOGADOR[0]);
    }
}

/* O tabuleiro: vagas, cartas ja jogadas, destaque da casa sob o mouse. */
void desenhar_tabuleiro(const App *app)
{
    int lin, col;
    float pisca = 0.5f + 0.5f * sinf(app->tempo * 5.0f);

    for (lin = 0; lin < N; lin++) {
        for (col = 0; col < N; col++) {
            Vector3 pos = posicao_casa(lin, col);
            int carta = app->jogo.tabuleiro[lin][col];
            int pousando = app->voo.ativo && app->voo.lin == lin && app->voo.col == col;

            if (carta == VAZIO || pousando) {
                float brilho = 0.0f;
                if (app->fase == FASE_ESCOLHER && app->selecionada >= 0 && !pousando) {
                    brilho = 0.18f + 0.14f * pisca;           /* mostra onde da para jogar */
                    if (app->lin_sob_mouse == lin && app->col_sob_mouse == col) {
                        brilho = 0.95f;
                    }
                }
                desenhar_vaga(pos, brilho);
            }
            if (carta != VAZIO && !pousando) {
                float quique = app->pulo[lin][col];
                pos.y += 0.25f * quique;
                desenhar_carta(carta, pos, 0.0f, 1.0f + 0.1f * quique, COR_JOGADOR[app->jogo.dono[lin][col]]);
            }
        }
    }
}

/* As cartas da mao, perto da camera. */
void desenhar_mao(const App *app)
{
    int dono = dono_da_mao(app);
    int escondida = (app->fase == FASE_TROCA);
    int i;

    for (i = 0; i < TAM_MAO; i++) {
        int carta = app->jogo.mao[dono][i];
        Vector3 pos = posicao_mao(i);
        float escala = 1.0f;
        float balanco = 0.0f;

        if (carta == VAZIO) {
            continue;
        }
        if (app->voo.ativo && app->voo.dono == dono && app->voo.indice_mao == i) {
            continue;                              /* esta carta esta voando */
        }
        pos.y += app->elevacao[i];
        escala = app->surgir[i];
        if (i == app->selecionada) {
            balanco = 3.0f * sinf(app->tempo * 3.0f);
        }
        if (escala < 0.05f) {
            continue;
        }
        desenhar_carta(escondida ? VAZIO : carta, pos, balanco, escala, COR_JOGADOR[dono]);
    }
}

/* O monte de cartas que ainda nao foram compradas. */
void desenhar_monte(const App *app)
{
    int restantes = TAM_BARALHO - app->jogo.topo;
    int altura = restantes / 3;
    int i;
    Vector3 pos = posicao_monte();

    for (i = 0; i < altura; i++) {
        Vector3 p = pos;
        p.y += i * 0.07f;
        desenhar_carta(VAZIO, p, 6.0f * (i % 3) - 6.0f, 0.82f, COR_BRANCO);
    }
}

/* Cartas voando, em cima do tabuleiro. */
void desenhar_voo(const App *app)
{
    const Voo *v = &app->voo;
    Vector3 pos;
    float t;

    if (!v->ativo) {
        return;
    }
    t = v->tempo / DURACAO_VOO;
    if (t > 1.0f) {
        t = 1.0f;
    }
    pos = posicao_do_voo(v);
    desenhar_carta(v->carta, pos, 360.0f * suavizar(t), 1.0f + 0.15f * sinf(PI * t), COR_JOGADOR[v->dono]);
}

/* Confete da comemoracao. */
void desenhar_confete(const App *app)
{
    int i;

    for (i = 0; i < MAX_CONFETE; i++) {
        const Confete *c = &app->confete[i];
        if (!c->ativo) {
            continue;
        }
        rlPushMatrix();
        rlTranslatef(c->pos.x, c->pos.y, c->pos.z);
        rlRotatef(c->giro, 1.0f, 0.4f, 0.2f);
        DrawCube((Vector3){ 0.0f, 0.0f, 0.0f }, 0.22f, 0.03f, 0.14f, c->cor);
        rlPopMatrix();
    }
}

/* Tudo que existe no mundo 3D, conforme a tela. */
void desenhar_cena(const App *app)
{
    desenhar_mesa(app->tela != TELA_REGRAS);
    desenhar_parede_e_forno(app->tempo);
    if (app->tela != TELA_REGRAS) {
        desenhar_chef(pose_do_chef(app));
    }

    if (app->tela == TELA_REGRAS) {
        desenhar_cartas_das_regras(app);
    } else if (app->tela == TELA_JOGO) {
        desenhar_tabuleiro(app);
        desenhar_mao(app);
        desenhar_monte(app);
        desenhar_voo(app);
        desenhar_confete(app);
    } else {
        /* menu: uma pizza grande girando em cima da mesa */
        rlPushMatrix();
        rlTranslatef(0.0f, 0.15f + 0.1f * sinf(app->tempo * 1.5f), 0.5f);
        rlRotatef(app->tempo * 25.0f, 0.0f, 1.0f, 0.0f);
        rlScalef(2.6f, 2.6f, 2.6f);
        DrawCylinder((Vector3){ 0.0f, 0.0f, 0.0f }, 0.88f, 0.88f, 0.05f, 36, COR_BRANCO);
        desenhar_pizza(4, 0.05f);
        rlPopMatrix();
    }
}

/* ================================================================== */
/*  INICIO DA PARTIDA                                                  */
/* ================================================================== */

/* Zera tudo e comeca uma partida nova (o modo, os nomes e a dificuldade ja foram escolhidos). */
void iniciar_partida(App *app)
{
    int i, j;

    novo_jogo(&app->jogo);
    app->placar[0] = 0;
    app->placar[1] = 0;
    app->selecionada = -1;
    app->mao_sob_mouse = -1;
    app->lin_sob_mouse = -1;
    app->col_sob_mouse = -1;
    for (i = 0; i < TAM_MAO; i++) {
        app->elevacao[i] = 0.0f;
        app->surgir[i] = 1.0f;
    }
    for (i = 0; i < N; i++) {
        for (j = 0; j < N; j++) {
            app->pulo[i][j] = 0.0f;
        }
    }
    app->voo.ativo = 0;
    for (i = 0; i < MAX_POPUPS; i++) {
        app->popups[i].ativo = 0;
    }
    for (i = 0; i < MAX_CONFETE; i++) {
        app->confete[i].ativo = 0;
    }
    app->tempo_fala = 0.0f;
    app->humor_chef = 1;
    app->ultimos_pontos_humano = 0;
    app->tempo_fim = 0.0f;
    app->pausado = 0;
    app->tela = TELA_JOGO;
}

/* Define quem joga agora e em que fase o jogo fica. */
void comecar_turno(App *app)
{
    app->selecionada = -1;
    if (app->contra_computador && app->jogo.vez == 1) {
        app->fase = FASE_PENSAR;
        app->espera = 1.1f + 0.1f * (float)(rand() % 6);
    } else if (!app->contra_computador) {
        app->fase = FASE_TROCA;
    } else {
        app->fase = FASE_ESCOLHER;
    }
}

/* ================================================================== */
/*  INTERFACE 2D                                                       */
/* ================================================================== */

/* Texto com sombra, centralizado em cx. */
void texto_centro(const char *texto, int cx, int y, int tamanho, Color cor)
{
    int largura = MeasureText(texto, tamanho);
    DrawText(texto, cx - largura / 2 + 2, y + 2, tamanho, Fade(BLACK, 0.55f));
    DrawText(texto, cx - largura / 2, y, tamanho, cor);
}

/* Texto com sombra, começando em x. */
void texto_esq(const char *texto, int x, int y, int tamanho, Color cor)
{
    DrawText(texto, x + 2, y + 2, tamanho, Fade(BLACK, 0.55f));
    DrawText(texto, x, y, tamanho, cor);
}

/* Botao de clicar. Desenha e devolve 1 no quadro em que foi clicado. */
int botao(Rectangle r, const char *texto, const Entrada *e)
{
    int sobre = CheckCollisionPointRec(e->mouse, r);
    Color fundo = sobre ? (Color){ 255, 190, 60, 255 } : (Color){ 204, 48, 40, 255 };
    Color letra = sobre ? (Color){ 70, 30, 20, 255 } : COR_BRANCO;
    int tamanho = 30;

    DrawRectangleRounded((Rectangle){ r.x, r.y + 6, r.width, r.height }, 0.4f, 10, Fade(BLACK, 0.45f));
    DrawRectangleRounded(r, 0.4f, 10, fundo);
    DrawText(texto, (int)(r.x + (r.width - MeasureText(texto, tamanho)) / 2),
             (int)(r.y + (r.height - tamanho) / 2), tamanho, letra);
    return sobre && e->clicou;
}

/* Fundo escurecido atras de janelas (pausa, fim de jogo). */
void escurecer_tela(float intensidade)
{
    DrawRectangle(0, 0, LARGURA_TELA, ALTURA_TELA, Fade(BLACK, intensidade));
}

/* Janelinha arredondada. */
void desenhar_painel(Rectangle r)
{
    DrawRectangleRounded((Rectangle){ r.x - 4, r.y - 4, r.width + 8, r.height + 8 }, 0.08f, 10, (Color){ 255, 214, 130, 255 });
    DrawRectangleRounded(r, 0.08f, 10, (Color){ 46, 28, 26, 245 });
}

/* Balao de fala do pizzaiolo, ao lado da cabeca dele (sempre abaixo do placar). */
void desenhar_balao(const char *texto, Camera3D cam)
{
    Vector3 cabeca = posicao_chef();
    Vector2 ancora;
    int tamanho = 26;
    int largura = MeasureText(texto, tamanho) + 40;
    int altura = 58;
    float x, y;

    cabeca.y += 1.8f;
    ancora = GetWorldToScreen(cabeca, cam);
    x = ancora.x + 70;
    y = ancora.y - altura / 2 - 20;
    if (x + largura > LARGURA_TELA - 10) {
        x = LARGURA_TELA - 10 - largura;
    }
    if (y < 110) {
        y = 110;
    }

    /* "rabinho" do balao, apontando para a cabeca */
    DrawTriangle((Vector2){ x + 4, y + altura * 0.30f }, (Vector2){ x - 26, y + altura * 0.55f },
                 (Vector2){ x + 4, y + altura * 0.75f }, COR_BRANCO);
    DrawRectangleRounded((Rectangle){ x - 3, y - 3, (float)largura + 6, (float)altura + 6 }, 0.5f, 10, (Color){ 70, 40, 30, 255 });
    DrawRectangleRounded((Rectangle){ x, y, (float)largura, (float)altura }, 0.5f, 10, COR_BRANCO);
    DrawTriangle((Vector2){ x + 6, y + altura * 0.34f }, (Vector2){ x - 22, y + altura * 0.55f },
                 (Vector2){ x + 6, y + altura * 0.72f }, COR_BRANCO);
    DrawText(texto, (int)x + 20, (int)y + (altura - tamanho) / 2, tamanho, (Color){ 60, 34, 28, 255 });
}

/* "+3" que sobe da carta quando alguem pontua. */
void desenhar_popups(const App *app, Camera3D cam)
{
    int i;

    for (i = 0; i < MAX_POPUPS; i++) {
        const Popup *p = &app->popups[i];
        Vector3 pos;
        Vector2 tela;
        char texto[16];
        float k;

        if (!p->ativo) {
            continue;
        }
        k = p->tempo / 1.5f;
        pos = p->pos;
        pos.y += 1.2f + 1.6f * k;
        tela = GetWorldToScreen(pos, cam);
        snprintf(texto, sizeof texto, "+%d", p->pontos);
        texto_centro(texto, (int)tela.x, (int)tela.y - 30, 64, Fade(COR_JOGADOR[p->dono], 1.0f - k * k));
    }
}

/* Placar de um jogador (canto da tela). */
void desenhar_placar(const App *app, int jogador, int x)
{
    int ativo = (app->tela == TELA_JOGO && app->fase != FASE_FIM && app->jogo.vez == jogador);
    Rectangle r = { (float)x, 16, 300, 82 };
    char numero[8];

    DrawRectangleRounded((Rectangle){ r.x, r.y + 5, r.width, r.height }, 0.25f, 10, Fade(BLACK, 0.4f));
    DrawRectangleRounded(r, 0.25f, 10, ativo ? COR_JOGADOR[jogador] : (Color){ 52, 36, 34, 230 });
    DrawCircle((int)r.x + 36, (int)r.y + 41, 20, COR_JOGADOR[jogador]);
    DrawText(app->nome[jogador], (int)r.x + 68, (int)r.y + 14, 26, ativo ? (Color){ 40, 24, 20, 255 } : COR_BRANCO);
    snprintf(numero, sizeof numero, "%d", app->placar[jogador]);
    DrawText(numero, (int)r.x + 68, (int)r.y + 40, 38, ativo ? (Color){ 40, 24, 20, 255 } : COR_BRANCO);
}

/* Pontos extras so para os numeros 1, 2, 3 em cima das cartas da mao. */
void desenhar_numeros_da_mao(const App *app, Camera3D cam)
{
    int dono = dono_da_mao(app);
    int i;

    if (app->fase == FASE_TROCA) {
        return;
    }
    for (i = 0; i < TAM_MAO; i++) {
        Vector3 pos = posicao_mao(i);
        Vector2 tela;
        char numero[4];

        if (app->jogo.mao[dono][i] == VAZIO || app->surgir[i] < 0.95f) {
            continue;
        }
        if (app->voo.ativo && app->voo.dono == dono && app->voo.indice_mao == i) {
            continue;
        }
        pos.z += 1.4f;
        pos.y += app->elevacao[i];
        tela = GetWorldToScreen(pos, cam);
        snprintf(numero, sizeof numero, "%d", i + 1);
        DrawCircle((int)tela.x, (int)tela.y + 14, 17, Fade(BLACK, 0.55f));
        DrawCircle((int)tela.x, (int)tela.y + 14, 15, (i == app->selecionada) ? (Color){ 255, 205, 70, 255 } : COR_BRANCO);
        DrawText(numero, (int)tela.x - MeasureText(numero, 24) / 2, (int)tela.y + 2, 24, (Color){ 50, 30, 24, 255 });
    }
}

/* Nome da carta que esta sob o mouse (ou escolhida). */
void desenhar_nome_da_carta(const App *app)
{
    int dono = dono_da_mao(app);
    int i = (app->mao_sob_mouse >= 0) ? app->mao_sob_mouse : app->selecionada;

    if (app->fase != FASE_ESCOLHER || i < 0 || app->jogo.mao[dono][i] == VAZIO) {
        return;
    }
    texto_centro(NOME_CARTA[app->jogo.mao[dono][i]], LARGURA_TELA / 2, ALTURA_TELA - 56, 30, COR_BRANCO);
}

/* ---------- telas completas ---------- */

/* Escolhe o modo de jogo, poe os nomes e comeca a partida. */
void comecar_partida_nova(App *app, int contra_computador, int dificuldade)
{
    app->contra_computador = contra_computador;
    app->dificuldade = dificuldade;
    if (contra_computador) {
        strcpy(app->nome[0], "Você");
        strcpy(app->nome[1], "Chef Gennaro");
    } else {
        strcpy(app->nome[0], "Jogador 1");
        strcpy(app->nome[1], "Jogador 2");
    }
    iniciar_partida(app);
    comecar_turno(app);
}

void interface_menu(App *app, const Entrada *e, Camera3D cam)
{
    Rectangle r = { 90, 250, 380, 66 };

    texto_esq("PIZZALIGN", 86, 44, 96, (Color){ 255, 226, 150, 255 });
    texto_esq("o jogo de cartas de pizza em 3D", 94, 148, 28, COR_BRANCO);
    desenhar_balao("Bem-vindo à minha pizzaria!", cam);

    if (botao(r, "1 jogador", e)) {
        app->tela = TELA_DIFICULDADE;
    }
    r.y += 84;
    if (botao(r, "2 jogadores", e)) {
        comecar_partida_nova(app, 0, 0);
    }
    r.y += 84;
    if (botao(r, "Regras e cartas", e)) {
        app->tela_anterior = TELA_MENU;
        app->tela = TELA_REGRAS;
    }
    r.y += 84;
    if (botao(r, "Sair", e)) {
        app->sair = 1;
    }
}

void interface_dificuldade(App *app, const Entrada *e)
{
    Rectangle r = { 90, 250, 380, 66 };

    texto_esq("Escolha o pizzaiolo", 86, 150, 44, (Color){ 255, 226, 150, 255 });

    if (botao(r, "Estagiário", e)) {
        comecar_partida_nova(app, 1, 1);
    }
    r.y += 84;
    if (botao(r, "Pizzaiolo", e)) {
        comecar_partida_nova(app, 1, 2);
    }
    r.y += 84;
    if (botao(r, "Mestre pizzaiolo", e)) {
        comecar_partida_nova(app, 1, 3);
    }
    r.y += 84;
    if (botao(r, "Voltar", e)) {
        app->tela = TELA_MENU;
    }
}

void interface_regras(App *app, const Entrada *e, Camera3D cam)
{
    int i;
    Rectangle painel = { 60, 470, 1160, 230 };

    texto_centro("Regras e cartas", LARGURA_TELA / 2, 18, 48, (Color){ 255, 226, 150, 255 });

    /* numero e nome embaixo de cada carta */
    for (i = 0; i < NUM_CARTAS; i++) {
        Vector3 pos = posicao_carta_das_regras(i);
        Vector2 tela;
        char texto[40];

        pos.z += 1.45f;
        tela = GetWorldToScreen(pos, cam);
        snprintf(texto, sizeof texto, "%d  %s", i + 1, NOME_CARTA[i]);
        texto_centro(texto, (int)tela.x, (int)tela.y, 20, COR_BRANCO);
    }

    desenhar_painel(painel);
    texto_esq("Jogue 1 carta por vez em uma casa vazia. Quando o tabuleiro encher, vence quem tiver mais pontos.", 90, 490, 22, COR_BRANCO);
    texto_esq("Você só pontua quando a carta jogada forma algo com as vizinhas:", 90, 530, 22, (Color){ 255, 226, 150, 255 });
    texto_esq("+1   fatias iguais: carta igual encostada (ao lado, em cima ou embaixo)", 90, 566, 22, COR_BRANCO);
    texto_esq("+2   fatias que se completam: as duas formam uma pizza inteira (2 e 6, 3 e 7, 4 e 8)", 90, 598, 22, COR_BRANCO);
    texto_esq("+1   por carta: 3 ou mais cartas seguidas do ciclo, em linha reta (ex.: 2, 3, 4)", 90, 630, 22, COR_BRANCO);
    texto_esq("Depois da carta 8 o ciclo volta para a 1.", 90, 662, 22, COR_BRANCO);

    if (botao((Rectangle){ 1040, 24, 200, 56 }, "Voltar", e) || e->esc) {
        app->tela = app->tela_anterior;
    }
}

void interface_jogo(App *app, const Entrada *e, Camera3D cam)
{
    desenhar_numeros_da_mao(app, cam);
    desenhar_popups(app, cam);
    desenhar_placar(app, 0, 16);
    desenhar_placar(app, 1, LARGURA_TELA - 316);
    desenhar_nome_da_carta(app);

    if (app->tempo_fala > 0.0f && app->contra_computador) {
        desenhar_balao(app->fala, cam);
    }
    if (app->fase == FASE_PENSAR) {
        int pontinhos = 1 + ((int)(app->tempo * 3.0f)) % 3;
        char texto[8] = "...";
        texto[pontinhos] = '\0';
        desenhar_balao(texto, cam);
    }

    {
        const char *dica = "Esc: pausa   R: regras   Botão direito: girar";
        int largura = MeasureText(dica, 18);
        DrawRectangleRounded((Rectangle){ (float)(LARGURA_TELA - largura - 34), (float)(ALTURA_TELA - 40), (float)(largura + 24), 30 }, 0.5f, 8, Fade(BLACK, 0.55f));
        DrawText(dica, LARGURA_TELA - largura - 22, ALTURA_TELA - 34, 18, COR_BRANCO);
    }

    /* 2 jogadores: tela que esconde a mao ate o proximo jogador estar pronto */
    if (app->fase == FASE_TROCA && !app->pausado) {
        char texto[64];
        escurecer_tela(0.45f);
        snprintf(texto, sizeof texto, "Vez de %s", app->nome[app->jogo.vez]);
        texto_centro(texto, LARGURA_TELA / 2, 250, 64, COR_JOGADOR[app->jogo.vez]);
        texto_centro("Clique para ver a sua mão", LARGURA_TELA / 2, 340, 30, COR_BRANCO);
    }

    /* fim de jogo */
    if (app->fase == FASE_FIM && app->tempo_fim > 1.2f) {
        Rectangle painel = { 380, 190, 520, 330 };
        char texto[96];
        int p0 = app->jogo.pontos[0];
        int p1 = app->jogo.pontos[1];

        escurecer_tela(0.4f);
        desenhar_painel(painel);
        if (p0 == p1) {
            snprintf(texto, sizeof texto, "Empate!");
        } else {
            snprintf(texto, sizeof texto, "%s venceu!", app->nome[(p1 > p0) ? 1 : 0]);
        }
        texto_centro(texto, LARGURA_TELA / 2, 220, 46, (Color){ 255, 226, 150, 255 });
        snprintf(texto, sizeof texto, "%d  x  %d", p0, p1);
        texto_centro(texto, LARGURA_TELA / 2, 285, 52, COR_BRANCO);
        if (botao((Rectangle){ 450, 372, 380, 60 }, "Jogar de novo", e)) {
            comecar_partida_nova(app, app->contra_computador, app->dificuldade);
        }
        if (botao((Rectangle){ 450, 444, 380, 60 }, "Menu principal", e)) {
            app->tela = TELA_MENU;
        }
    }

    /* pausa */
    if (app->pausado) {
        Rectangle painel = { 400, 170, 480, 380 };

        escurecer_tela(0.55f);
        desenhar_painel(painel);
        texto_centro("Pausa", LARGURA_TELA / 2, 196, 52, (Color){ 255, 226, 150, 255 });
        if (botao((Rectangle){ 450, 290, 380, 60 }, "Continuar", e)) {
            app->pausado = 0;
        }
        if (botao((Rectangle){ 450, 366, 380, 60 }, "Regras", e)) {
            app->pausado = 0;
            app->tela_anterior = TELA_JOGO;
            app->tela = TELA_REGRAS;
        }
        if (botao((Rectangle){ 450, 442, 380, 60 }, "Menu principal", e)) {
            app->pausado = 0;
            app->tela = TELA_MENU;
        }
    }
}

/* ================================================================== */
/*  ATUALIZACAO (o que muda a cada quadro)                             */
/* ================================================================== */

/* Joga a carta e comeca a animacao dela voando ate a casa. As regras sao
 * aplicadas na hora; a tela so mostra o resultado quando a carta pousa. */
void iniciar_voo(App *app, int indice, int lin, int col)
{
    Jogo *jogo = &app->jogo;
    int jogador = jogo->vez;
    Voo *v = &app->voo;

    v->carta = jogo->mao[jogador][indice];
    v->dono = jogador;
    v->indice_mao = indice;
    v->lin = lin;
    v->col = col;
    v->para = posicao_casa(lin, col);
    if (app->contra_computador && jogador == 1) {
        v->de = posicao_carta_do_chef();
    } else {
        v->de = posicao_mao(indice);
        v->de.y += app->elevacao[indice];
    }
    v->pontos = jogar_carta(jogo, jogador, indice, lin, col);
    v->tempo = 0.0f;
    v->ativo = 1;

    app->fase = FASE_VOO;
    app->selecionada = -1;

    if (app->contra_computador && jogador == 1) {
        snprintf(app->fala, sizeof app->fala, "%s", fala_do_chef(v->pontos, app->ultimos_pontos_humano));
        app->tempo_fala = 3.8f;
        app->humor_chef = (v->pontos >= 3) ? 2 : 1;
    } else {
        app->ultimos_pontos_humano = v->pontos;
    }
}

/* Solta confete em cima da mesa. */
void soltar_confete(App *app)
{
    int i;
    Color cores[5] = { { 255, 90, 80, 255 }, { 255, 200, 60, 255 }, { 90, 200, 120, 255 },
                       { 90, 170, 255, 255 }, { 255, 255, 255, 255 } };

    for (i = 0; i < MAX_CONFETE; i++) {
        Confete *c = &app->confete[i];
        c->ativo = 1;
        c->pos = (Vector3){ -9.0f + 18.0f * (rand() % 1000) / 1000.0f,
                            5.0f + 8.0f * (rand() % 1000) / 1000.0f,
                            -4.0f + 10.0f * (rand() % 1000) / 1000.0f };
        c->vel = (Vector3){ -0.6f + 1.2f * (rand() % 1000) / 1000.0f,
                            -1.5f - 1.5f * (rand() % 1000) / 1000.0f,
                            -0.6f + 1.2f * (rand() % 1000) / 1000.0f };
        c->cor = cores[rand() % 5];
        c->giro = (float)(rand() % 360);
        c->vel_giro = 120.0f + (float)(rand() % 300);
    }
}

/* A carta pousou: atualiza o placar, mostra os pontos e passa a vez. */
void terminar_voo(App *app)
{
    Voo *v = &app->voo;
    int i;

    v->ativo = 0;
    app->placar[v->dono] = app->jogo.pontos[v->dono];
    app->pulo[v->lin][v->col] = 1.0f;

    if (v->pontos > 0) {
        for (i = 0; i < MAX_POPUPS; i++) {
            if (!app->popups[i].ativo) {
                app->popups[i].ativo = 1;
                app->popups[i].pos = v->para;
                app->popups[i].pontos = v->pontos;
                app->popups[i].dono = v->dono;
                app->popups[i].tempo = 0.0f;
                break;
            }
        }
    }

    if (app->jogo.cartas_jogadas >= N * N) {
        int p0 = app->jogo.pontos[0];
        int p1 = app->jogo.pontos[1];

        app->fase = FASE_FIM;
        app->tempo_fim = 0.0f;
        soltar_confete(app);
        if (app->contra_computador) {
            if (p1 > p0) {
                snprintf(app->fala, sizeof app->fala, "Hoje a casa ganhou! Volte sempre!");
                app->humor_chef = 2;
            } else if (p0 > p1) {
                snprintf(app->fala, sizeof app->fala, "Você é o novo mestre pizzaiolo!");
                app->humor_chef = -1;
            } else {
                snprintf(app->fala, sizeof app->fala, "Empate! Foi por uma fatia!");
                app->humor_chef = 1;
            }
            app->tempo_fala = 1000.0f;
        }
        return;
    }

    app->jogo.vez = 1 - app->jogo.vez;
    comecar_turno(app);
    if (app->contra_computador && v->dono == 0) {
        app->surgir[v->indice_mao] = 0.0f;      /* a carta nova aparece na mao */
    }
}

/* Descobre qual carta da mao e qual casa estao sob o mouse (raio da camera ate a mesa). */
void descobrir_sob_o_mouse(App *app, const Entrada *e, Camera3D cam)
{
    Ray raio = GetScreenToWorldRay(e->mouse, cam);
    int dono = dono_da_mao(app);
    float melhor = 1000000.0f;
    int i, lin, col;

    app->mao_sob_mouse = -1;
    app->lin_sob_mouse = -1;
    app->col_sob_mouse = -1;

    for (i = 0; i < TAM_MAO; i++) {
        Vector3 c = posicao_mao(i);
        BoundingBox caixa;
        RayCollision acerto;

        if (app->jogo.mao[dono][i] == VAZIO) {
            continue;
        }
        caixa.min = (Vector3){ c.x - 1.1f, 0.0f, c.z - 1.1f };
        caixa.max = (Vector3){ c.x + 1.1f, 0.6f + app->elevacao[i], c.z + 1.1f };
        acerto = GetRayCollisionBox(raio, caixa);
        if (acerto.hit && acerto.distance < melhor) {
            melhor = acerto.distance;
            app->mao_sob_mouse = i;
        }
    }

    if (app->selecionada < 0) {
        return;
    }
    for (lin = 0; lin < N; lin++) {
        for (col = 0; col < N; col++) {
            Vector3 c = posicao_casa(lin, col);
            BoundingBox caixa;
            RayCollision acerto;

            if (app->jogo.tabuleiro[lin][col] != VAZIO) {
                continue;
            }
            caixa.min = (Vector3){ c.x - 1.15f, 0.0f, c.z - 1.15f };
            caixa.max = (Vector3){ c.x + 1.15f, 0.5f, c.z + 1.15f };
            acerto = GetRayCollisionBox(raio, caixa);
            if (acerto.hit && acerto.distance < melhor) {
                melhor = acerto.distance;
                app->mao_sob_mouse = -1;
                app->lin_sob_mouse = lin;
                app->col_sob_mouse = col;
            }
        }
    }
}

/* O que acontece na partida a cada quadro. */
void atualizar_partida(App *app, const Entrada *e, float dt, Camera3D cam)
{
    int i, j;

    /* quicada das cartas, cartas novas, cartas levantadas */
    for (i = 0; i < N; i++) {
        for (j = 0; j < N; j++) {
            app->pulo[i][j] -= dt * 3.0f;
            if (app->pulo[i][j] < 0.0f) {
                app->pulo[i][j] = 0.0f;
            }
        }
    }
    for (i = 0; i < TAM_MAO; i++) {
        float alvo = 0.0f;
        if (i == app->selecionada) {
            alvo = 0.9f;
        } else if (i == app->mao_sob_mouse && app->fase == FASE_ESCOLHER) {
            alvo = 0.35f;
        }
        app->elevacao[i] += (alvo - app->elevacao[i]) * fminf(1.0f, dt * 12.0f);
        app->surgir[i] = fminf(1.0f, app->surgir[i] + dt * 3.5f);
    }

    /* pontos subindo */
    for (i = 0; i < MAX_POPUPS; i++) {
        if (app->popups[i].ativo) {
            app->popups[i].tempo += dt;
            if (app->popups[i].tempo > 1.5f) {
                app->popups[i].ativo = 0;
            }
        }
    }

    /* confete caindo */
    for (i = 0; i < MAX_CONFETE; i++) {
        Confete *c = &app->confete[i];
        if (!c->ativo) {
            continue;
        }
        c->pos = Vector3Add(c->pos, Vector3Scale(c->vel, dt));
        c->giro += c->vel_giro * dt;
        if (c->pos.y < 0.05f) {
            c->pos.y = 0.05f;
            c->vel = (Vector3){ 0.0f, 0.0f, 0.0f };
            c->vel_giro = 0.0f;
        }
    }

    if (app->tempo_fala > 0.0f && app->tempo_fala < 500.0f) {
        app->tempo_fala -= dt;
    }

    if (app->pausado) {
        return;
    }

    switch (app->fase) {
    case FASE_TROCA:
        if (e->clicou) {
            app->fase = FASE_ESCOLHER;
        }
        break;

    case FASE_ESCOLHER:
        descobrir_sob_o_mouse(app, e, cam);

        if (e->tecla_carta > 0 && app->jogo.mao[dono_da_mao(app)][e->tecla_carta - 1] != VAZIO) {
            int escolhida = e->tecla_carta - 1;
            app->selecionada = (app->selecionada == escolhida) ? -1 : escolhida;
        }
        if (e->clicou) {
            if (app->mao_sob_mouse >= 0) {
                /* clicar na carta escolhida de novo desfaz a escolha */
                app->selecionada = (app->selecionada == app->mao_sob_mouse) ? -1 : app->mao_sob_mouse;
            } else if (app->selecionada >= 0 && app->lin_sob_mouse >= 0) {
                iniciar_voo(app, app->selecionada, app->lin_sob_mouse, app->col_sob_mouse);
            }
        }
        break;

    case FASE_PENSAR:
        app->espera -= dt;
        if (app->espera <= 0.0f) {
            int indice, lin, col;
            escolher_jogada(&app->jogo, 1, app->dificuldade, &indice, &lin, &col);
            iniciar_voo(app, indice, lin, col);
        }
        break;

    case FASE_VOO:
        app->voo.tempo += dt;
        if (app->voo.tempo >= DURACAO_VOO) {
            terminar_voo(app);
        }
        break;

    case FASE_FIM:
        app->tempo_fim += dt;
        break;
    }
}

/* Mexe a camera com o mouse (botao direito), roda do mouse e setas. */
void atualizar_camera(App *app, const Entrada *e, float dt)
{
    if (app->tela == TELA_JOGO) {
        app->yaw -= e->arrasto.x * 0.005f;
        app->pitch += e->arrasto.y * 0.004f;
        app->yaw += e->girar * 0.9f * dt;
        app->distancia -= e->roda * 0.8f;
    }
    app->yaw = Clamp(app->yaw, -0.9f, 0.9f);
    app->pitch = Clamp(app->pitch, 0.55f, 1.2f);
    app->distancia = Clamp(app->distancia, 13.0f, 24.0f);
}

/* Poe os valores iniciais do programa. */
void iniciar_app(App *app)
{
    memset(app, 0, sizeof *app);
    app->tela = TELA_MENU;
    app->tela_anterior = TELA_MENU;
    app->fase = FASE_ESCOLHER;
    app->contra_computador = 1;
    app->dificuldade = 2;
    app->humor_chef = 1;
    app->selecionada = -1;
    app->mao_sob_mouse = -1;
    app->lin_sob_mouse = -1;
    app->col_sob_mouse = -1;
    app->yaw = 0.0f;
    app->pitch = 0.78f;
    app->distancia = 18.5f;
    strcpy(app->nome[0], "Você");
    strcpy(app->nome[1], "Chef Gennaro");
    snprintf(app->fala, sizeof app->fala, "Bem-vindo à minha pizzaria!");
    app->tempo_fala = 1000.0f;
    novo_jogo(&app->jogo);
}

/* Um quadro inteiro: atualiza, desenha o 3D e depois a interface 2D. */
void quadro(App *app, const Entrada *e, float dt)
{
    Camera3D cam;

    app->tempo += dt;

    /* atalhos de teclado */
    if (app->tela == TELA_JOGO && e->esc && app->fase != FASE_FIM) {
        app->pausado = !app->pausado;
    }
    if (app->tela == TELA_JOGO && e->tecla_r && !app->pausado) {
        app->tela_anterior = TELA_JOGO;
        app->tela = TELA_REGRAS;
    }

    atualizar_camera(app, e, dt);
    if (app->tela == TELA_MENU || app->tela == TELA_DIFICULDADE) {
        app->yaw = 0.12f * sinf(app->tempo * 0.35f);       /* a camera balanca de leve no menu */
        app->pitch = 0.78f;
        app->distancia = 19.0f;
    }
    if (app->tela == TELA_REGRAS) {
        app->yaw = 0.0f;
        app->pitch = 1.2f;
        app->distancia = 15.0f;
    }
    cam = calcular_camera(app);

    if (app->tela == TELA_JOGO) {
        atualizar_partida(app, e, dt, cam);
    }

    SetShaderValue(sombreador, GetShaderLocation(sombreador, "posicaoCamera"), &cam.position, SHADER_UNIFORM_VEC3);

    BeginDrawing();
    ClearBackground((Color){ 50, 30, 28, 255 });
    DrawRectangleGradientV(0, 0, LARGURA_TELA, ALTURA_TELA, (Color){ 70, 34, 30, 255 }, (Color){ 140, 64, 44, 255 });

    BeginMode3D(cam);
    rlDisableBackfaceCulling();
    BeginShaderMode(sombreador);
    desenhar_cena(app);
    EndShaderMode();
    rlEnableBackfaceCulling();
    EndMode3D();

    switch (app->tela) {
    case TELA_MENU:
        interface_menu(app, e, cam);
        break;
    case TELA_DIFICULDADE:
        interface_dificuldade(app, e);
        break;
    case TELA_REGRAS:
        interface_regras(app, e, cam);
        break;
    case TELA_JOGO:
        interface_jogo(app, e, cam);
        break;
    }
    EndDrawing();
}

/* Le o mouse e o teclado. */
Entrada ler_entrada(void)
{
    Entrada e;

    e.mouse = GetMousePosition();
    e.arrasto = IsMouseButtonDown(MOUSE_BUTTON_RIGHT) ? GetMouseDelta() : (Vector2){ 0.0f, 0.0f };
    e.roda = GetMouseWheelMove();
    e.clicou = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    e.girar = (IsKeyDown(KEY_RIGHT) ? 1 : 0) - (IsKeyDown(KEY_LEFT) ? 1 : 0);
    e.esc = IsKeyPressed(KEY_ESCAPE);
    e.tecla_r = IsKeyPressed(KEY_R);
    e.tecla_carta = 0;
    if (IsKeyPressed(KEY_ONE)) {
        e.tecla_carta = 1;
    } else if (IsKeyPressed(KEY_TWO)) {
        e.tecla_carta = 2;
    } else if (IsKeyPressed(KEY_THREE)) {
        e.tecla_carta = 3;
    }
    return e;
}

int main(void)
{
    App app;

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(LARGURA_TELA, ALTURA_TELA, "Pizzalign 3D");
    SetExitKey(KEY_NULL);            /* o Esc e' da pausa; fechar e' pelo X da janela ou pelo botao Sair */
    SetTargetFPS(60);

    srand((unsigned)time(NULL));
    carregar_luz();
    iniciar_app(&app);

    while (!WindowShouldClose() && !app.sair) {
        Entrada e = ler_entrada();
        quadro(&app, &e, GetFrameTime());
    }

    UnloadShader(sombreador);
    CloseWindow();
    return 0;
}
