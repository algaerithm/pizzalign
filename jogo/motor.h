#ifndef MOTOR_H
#define MOTOR_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* O tabuleiro e feito de CARTAS. Cada carta ocupa 2x2 quadradinhos do desenho,
   entao 5 x 10 cartas = 10 x 20 quadradinhos na tela. */
#define larguraTab    5     /* colunas de cartas */
#define alturaTab     10    /* linhas de cartas (a de cima e a linha 0) */
#define CASAS_CARTA   2     /* quadradinhos por lado de uma carta (so para desenhar) */

#define VAZIA         0     /* casa sem carta */
#define PIZZA_INTEIRA 6     /* soma que fecha uma pizza */

/* As cartas de pizza (iguais aos desenhos). O numero guardado no tabuleiro e o da carta. */
#define CARTA_CAIXA     1   /* caixa vazia            valor 0 */
#define CARTA_FATIA_E   2   /* uma fatia, lado esq.   valor 1 */
#define CARTA_FATIA_D   3   /* uma fatia, lado dir.   valor 1 */
#define CARTA_METADE_E  4   /* metade esquerda        valor 3 */
#define CARTA_METADE_D  5   /* metade direita         valor 3 */
#define CARTA_FALTA_E   6   /* falta a fatia esq.     valor 5 */
#define CARTA_FALTA_D   7   /* falta a fatia dir.     valor 5 */
#define CARTA_INTEIRA   8   /* pizza inteira          valor 6 */
#define TOMATE          9   /* bloco que nunca pontua: so ocupa espaco */

#define PONTOS_PIZZA    3   /* par que soma 6 */
#define PONTOS_IGUAL    1   /* par de cartas iguais */
#define MAX_PARES       16
#define TAM_NOME        32

/* Macros auxiliares para validacao de cartas e coordenadas */
#define CARTA_VALIDA(c)      ((c) >= CARTA_CAIXA && (c) <= TOMATE)
#define CARTA_PONTUAVEL(c)   ((c) >= CARTA_CAIXA && (c) <= CARTA_INTEIRA)
#define POSICAO_VALIDA(x, y) ((x) >= 0 && (x) < larguraTab && (y) >= 0 && (y) < alturaTab)

/* ---------- ESTRUTURAS ---------- */

/* A carta que esta caindo ("pedacos" guarda o numero da carta: CARTA_... ou TOMATE) */
typedef struct {
    int x;          /* coluna */
    int y;          /* linha */
    int pedacos;
} Peca;

/* No da lista encadeada com as proximas cartas (fila) */
typedef struct NoFatia {
    int pedacos;
    struct NoFatia *proximo;
} NoFatia;

/* Um par que pontuou (a interface usa para mostrar o efeito) */
typedef struct {
    int lin1, col1, valor1;
    int lin2, col2, valor2;
    int pontos;
} Par;

/* O estado inteiro do jogo (matriz alocada com malloc) */
typedef struct {
    int **tab;            /* matriz alturaTab x larguraTab */
    Peca fatiaAtual;      /* carta caindo agora */
    NoFatia *filaFatias;  /* lista com as proximas cartas */
    int pontuacao;        /* total dos dois jogadores */
    bool gameOver;

    int pecasFixadas;     /* quantas cartas ja pousaram (a interface usa para trocar a vez) */
    int ganhoUltima;      /* pontos que a ultima carta que pousou rendeu (com as cascatas) */
    Par pares[MAX_PARES]; /* pares que pontuaram na ultima carta que pousou */
    int numPares;
} EstadoJogo;

/* No da lista encadeada do placar (top scores) */
typedef struct Recorde {
    char nome[TAM_NOME];
    int pontos;
    struct Recorde *prox;
} Recorde;

/* ---------- ESTADO (alocacao dinamica) ---------- */

/* Aloca e inicializa a estrutura principal do jogo e sua matriz. */
EstadoJogo *estado_criar(void);

/* Libera toda a memoria associada ao tabuleiro, fila de cartas e estado. */
void estado_liberar(EstadoJogo *jogo);

/* Reseta pontuacao, limpa o tabuleiro e preenche a fila inicial. */
void iniciarJogo(EstadoJogo *jogo);

/* ---------- FILA (lista encadeada) ---------- */

void fila_adicionar(NoFatia **inicio, int pedacos);
int fila_remover(NoFatia **inicio);
void fila_liberar(NoFatia *inicio);
void peca_proxima(EstadoJogo *jogo);

/* ---------- MOVIMENTO ---------- */

/* Verifica se a peca atual pode se deslocar (dx, dy) sem colidir ou sair da borda. */
bool pode_mover(const EstadoJogo *jogo, int dx, int dy);
void mover_peca(EstadoJogo *jogo, int dx, int dy);
void descer_peca(EstadoJogo *jogo);

/* ---------- POUSAR, PONTUAR E CAIR ---------- */

/* Fixa a peca atual no tabuleiro e resolve todas as combinacoes em cascata. */
void fixar_peca(EstadoJogo *jogo);

/* Retorna o ID da carta complementar que soma 6 (ou 0 se nao houver complemento). */
int carta_complemento(int carta);

/* Sorteia uma carta valida para entrar na fila. */
int carta_aleatoria(void);

/* Calcula a pontuacao obtida pela combinacao entre duas cartas adjacentes. */
int pontos_do_par(int a, int b);

/* Varre o tabuleiro em busca de pares, contabiliza pontos e aplica gravidade. */
int resolver_rodada(EstadoJogo *jogo);

/* Faz cartas suspensas cairem para preencher espacos vazios abaixo delas. */
void aplicar_gravidade(EstadoJogo *jogo);

/* ---------- PLACAR (lista encadeada + arquivo) ---------- */

void recorde_inserir(Recorde **inicio, const char *nome, int pontos);
Recorde *recordes_carregar(const char *arquivo);
int recordes_salvar(const Recorde *inicio, const char *arquivo);
void recordes_liberar(Recorde *inicio);

#endif /* MOTOR_H */