#ifndef MOTOR_H
#define MOTOR_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define larguraTab 5
#define alturaTab 10
#define CASAS_CARTA 2

#define VAZIA 0
#define PIZZA_INTEIRA 6

#define CARTA_CAIXA 1
#define CARTA_FATIA_E 2
#define CARTA_FATIA_D 3
#define CARTA_METADE_E 4
#define CARTA_METADE_D 5
#define CARTA_FALTA_E 6
#define CARTA_FALTA_D 7
#define CARTA_INTEIRA 8
#define TOMATE 9

#define PONTOS_PIZZA 3
#define PONTOS_IGUAL 1
#define MAX_PARES 16
#define TAM_NOME 32

#define CARTA_VALIDA(c) ((c) >= CARTA_CAIXA && (c) <= TOMATE)
#define CARTA_PONTUAVEL(c) ((c) >= CARTA_CAIXA && (c) <= CARTA_INTEIRA)
#define POSICAO_VALIDA(x, y) ((x) >= 0 && (x) < larguraTab && (y) >= 0 && (y) < alturaTab)

typedef struct
{
    int x;
    int y;
    int pedacos;
} Peca;

typedef struct NoFatia
{
    int pedacos;
    struct NoFatia *proximo;
} NoFatia;

typedef struct
{
    int lin1, col1, valor1;
    int lin2, col2, valor2;
    int pontos;
} Par;

typedef struct
{
    int **tab;
    Peca fatiaAtual;
    NoFatia *filaFatias;
    int pontuacao;
    bool gameOver;

    int pecasFixadas;
    int ganhoUltima;
    Par pares[MAX_PARES];
    int numPares;
} EstadoJogo;

typedef struct Recorde
{
    char nome[TAM_NOME];
    int pontos;
    struct Recorde *prox;
} Recorde;

EstadoJogo *estado_criar(void);

void estado_liberar(EstadoJogo *jogo);

void iniciarJogo(EstadoJogo *jogo);

void fila_adicionar(NoFatia **inicio, int pedacos);
int fila_remover(NoFatia **inicio);
void fila_liberar(NoFatia *inicio);
void peca_proxima(EstadoJogo *jogo);

bool pode_mover(const EstadoJogo *jogo, int dx, int dy);
void mover_peca(EstadoJogo *jogo, int dx, int dy);
void descer_peca(EstadoJogo *jogo);

void fixar_peca(EstadoJogo *jogo);

int carta_complemento(int carta);

int carta_aleatoria(void);

int pontos_do_par(int a, int b);

int resolver_rodada(EstadoJogo *jogo);

void aplicar_gravidade(EstadoJogo *jogo);

void recorde_inserir(Recorde **inicio, const char *nome, int pontos);
Recorde *recordes_carregar(const char *arquivo);
int recordes_salvar(const Recorde *inicio, const char *arquivo);
void recordes_liberar(Recorde *inicio);

#endif