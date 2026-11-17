#include "motor.h"
#include <string.h>

#define FILA_TAMANHO_INICIAL 3

#ifndef CARTA_VALIDA
#define CARTA_VALIDA(c)      ((c) >= CARTA_CAIXA && (c) <= TOMATE)
#endif

#ifndef CARTA_PONTUAVEL
#define CARTA_PONTUAVEL(c)   ((c) >= CARTA_CAIXA && (c) <= CARTA_INTEIRA)
#endif

#ifndef POSICAO_VALIDA
#define POSICAO_VALIDA(x, y) ((x) >= 0 && (x) < larguraTab && (y) >= 0 && (y) < alturaTab)
#endif

/* ---------- ESTADO (alocacao dinamica) ---------- */

EstadoJogo *estado_criar(void) {
    EstadoJogo *jogo = (EstadoJogo *)malloc(sizeof(EstadoJogo));
    if (!jogo) return NULL;

    jogo->tab = (int **)malloc(alturaTab * sizeof(int *));
    if (!jogo->tab) {
        free(jogo);
        return NULL;
    }

    for (int i = 0; i < alturaTab; i++) {
        jogo->tab[i] = (int *)calloc(larguraTab, sizeof(int));
        if (!jogo->tab[i]) {
            for (int j = 0; j < i; j++) free(jogo->tab[j]);
            free(jogo->tab);
            free(jogo);
            return NULL;
        }
    }

    jogo->filaFatias = NULL;
    jogo->pontuacao = 0;
    jogo->gameOver = false;
    jogo->pecasFixadas = 0;
    jogo->ganhoUltima = 0;
    jogo->numPares = 0;
    return jogo;
}

void estado_liberar(EstadoJogo *jogo) {
    if (!jogo) return;
    if (jogo->tab) {
        for (int i = 0; i < alturaTab; i++) {
            free(jogo->tab[i]);
        }
        free(jogo->tab);
    }
    fila_liberar(jogo->filaFatias);
    free(jogo);
}

void iniciarJogo(EstadoJogo *jogo) {
    if (!jogo || !jogo->tab) return;

    for (int i = 0; i < alturaTab; i++) {
        for (int j = 0; j < larguraTab; j++) {
            jogo->tab[i][j] = VAZIA;
        }
    }

    jogo->pontuacao = 0;
    jogo->gameOver = false;
    jogo->pecasFixadas = 0;
    jogo->ganhoUltima = 0;
    jogo->numPares = 0;

    fila_liberar(jogo->filaFatias);
    jogo->filaFatias = NULL;

    /* Preenche a fila inicial de proximas cartas */
    for (int i = 0; i < FILA_TAMANHO_INICIAL; i++) {
        fila_adicionar(&jogo->filaFatias, carta_aleatoria());
    }

    peca_proxima(jogo);
}

/* ---------- FILA (lista encadeada) ---------- */

void fila_adicionar(NoFatia **inicio, int pedacos) {
    if (!inicio) return;
    NoFatia *novo = (NoFatia *)malloc(sizeof(NoFatia));
    if (!novo) return;
    novo->pedacos = pedacos;
    novo->proximo = NULL;

    if (*inicio == NULL) {
        *inicio = novo;
    } else {
        NoFatia *atual = *inicio;
        while (atual->proximo != NULL) {
            atual = atual->proximo;
        }
        atual->proximo = novo;
    }
}

int fila_remover(NoFatia **inicio) {
    if (!inicio || *inicio == NULL) {
        return carta_aleatoria();
    }
    NoFatia *removido = *inicio;
    int valor = removido->pedacos;
    *inicio = removido->proximo;
    free(removido);
    return valor;
}

void fila_liberar(NoFatia *inicio) {
    NoFatia *atual = inicio;
    while (atual != NULL) {
        NoFatia *prox = atual->proximo;
        free(atual);
        atual = prox;
    }
}

void peca_proxima(EstadoJogo *jogo) {
    if (!jogo || !jogo->tab) return;

    /* Pega a proxima carta da fila */
    jogo->fatiaAtual.pedacos = fila_remover(&jogo->filaFatias);

    /* Adiciona uma nova carta no fim da fila para mante-la preenchida */
    fila_adicionar(&jogo->filaFatias, carta_aleatoria());

    /* Posiciona no topo centro */
    jogo->fatiaAtual.x = larguraTab / 2;
    jogo->fatiaAtual.y = 0;

    /* Se a casa de nascer ja esta ocupada, o tabuleiro encheu: Game Over */
    if (jogo->tab[jogo->fatiaAtual.y][jogo->fatiaAtual.x] != VAZIA) {
        jogo->gameOver = true;
    }
}

/* ---------- MOVIMENTO ---------- */

bool pode_mover(const EstadoJogo *jogo, int dx, int dy) {
    if (!jogo || !jogo->tab) return false;
    int nx = jogo->fatiaAtual.x + dx;
    int ny = jogo->fatiaAtual.y + dy;

    if (!POSICAO_VALIDA(nx, ny)) {
        return false;
    }

    /* So anda para casas vazias (a carta nunca entra em cima de outra) */
    return jogo->tab[ny][nx] == VAZIA;
}

void mover_peca(EstadoJogo *jogo, int dx, int dy) {
    if (pode_mover(jogo, dx, dy)) {
        jogo->fatiaAtual.x += dx;
        jogo->fatiaAtual.y += dy;
    }
}

void descer_peca(EstadoJogo *jogo) {
    if (!jogo || jogo->gameOver) return;

    if (pode_mover(jogo, 0, 1)) {
        jogo->fatiaAtual.y++;
    } else {
        fixar_peca(jogo);
    }
}

/* ---------- POUSAR, PONTUAR E CAIR ---------- */

/* A carta que completa a pizza com esta (so encaixa o lado equivalente):
 *   fatia esq. <-> falta fatia esq.     fatia dir. <-> falta fatia dir.
 *   metade esq. <-> metade dir.         caixa vazia <-> pizza inteira
 * Tomate e casa vazia nao completam nada (0). */
int carta_complemento(int carta) {
    static const int complemento[TOMATE + 1] = {
        [VAZIA]          = 0,
        [CARTA_CAIXA]    = CARTA_INTEIRA,
        [CARTA_FATIA_E]  = CARTA_FALTA_E,
        [CARTA_FATIA_D]  = CARTA_FALTA_D,
        [CARTA_METADE_E] = CARTA_METADE_D,
        [CARTA_METADE_D] = CARTA_METADE_E,
        [CARTA_FALTA_E]  = CARTA_FATIA_E,
        [CARTA_FALTA_D]  = CARTA_FATIA_D,
        [CARTA_INTEIRA]  = CARTA_CAIXA,
        [TOMATE]         = 0
    };

    if (!CARTA_VALIDA(carta)) return 0;
    return complemento[carta];
}

/* Sorteia uma carta. A caixa vazia e a inteira saem menos (so casam entre si: 0 + 6). */
int carta_aleatoria(void) {
    static const int sorteio[] = {
        CARTA_FATIA_E,  CARTA_FATIA_E,  CARTA_FATIA_E,  CARTA_FATIA_D,  CARTA_FATIA_D,  CARTA_FATIA_D,
        CARTA_METADE_E, CARTA_METADE_E, CARTA_METADE_E, CARTA_METADE_D, CARTA_METADE_D, CARTA_METADE_D,
        CARTA_FALTA_E,  CARTA_FALTA_E,  CARTA_FALTA_E,  CARTA_FALTA_D,  CARTA_FALTA_D,  CARTA_FALTA_D,
        CARTA_CAIXA,    CARTA_CAIXA,    CARTA_INTEIRA,  CARTA_INTEIRA
    };
    return sorteio[rand() % (int)(sizeof(sorteio) / sizeof(sorteio[0]))];
}

/* Regra de pontuacao de DUAS cartas vizinhas (a e b sao cartas):
 *   - uma completa a outra (so o lado equivalente) -> 3 pontos
 *   - as duas sao exatamente a mesma carta          -> 1 ponto
 *   - qualquer outra coisa (nao encaixa, tomate, vazio) -> 0, nao pontua.
 * Vale para cartas de qualquer jogador. */
int pontos_do_par(int a, int b) {
    if (!CARTA_PONTUAVEL(a) || !CARTA_PONTUAVEL(b)) return 0;
    if (b == carta_complemento(a)) return PONTOS_PIZZA;
    if (a == b) return PONTOS_IGUAL;
    return 0;
}

void aplicar_gravidade(EstadoJogo *jogo) {
    if (!jogo || !jogo->tab) return;

    for (int c = 0; c < larguraTab; c++) {
        int destino = alturaTab - 1; /* proxima casa livre de baixo para cima */
        for (int r = alturaTab - 1; r >= 0; r--) {
            if (jogo->tab[r][c] != VAZIA) {
                if (r != destino) {
                    jogo->tab[destino][c] = jogo->tab[r][c];
                    jogo->tab[r][c] = VAZIA;
                }
                destino--;
            }
        }
    }
}

/* Uma rodada: acha os pares que pontuam olhando cada carta com a vizinha da direita e a de baixo
 * (assim todo par lado a lado ou um em cima do outro e visto uma vez). Primeiro os que somam 6,
 * depois os iguais; cada carta entra em no maximo um par. Os pares somem do tabuleiro, as cartas
 * de cima descem e quem chamou repete ate nao sobrar par (cascata). Devolve os pontos da rodada. */
int resolver_rodada(EstadoJogo *jogo) {
    if (!jogo || !jogo->tab) return 0;

    bool usada[alturaTab][larguraTab];
    memset(usada, 0, sizeof usada);
    int total = 0;
    const int dl[2] = {0, 1}, dc[2] = {1, 0}; /* direita e baixo */

    for (int passada = 0; passada < 2; passada++) { /* 0 = soma 6, 1 = iguais */
        for (int r = alturaTab - 1; r >= 0; r--) {
            for (int c = 0; c < larguraTab; c++) {
                if (usada[r][c]) continue;
                for (int d = 0; d < 2; d++) {
                    int r2 = r + dl[d], c2 = c + dc[d];
                    if (r2 >= alturaTab || c2 >= larguraTab || usada[r2][c2]) continue;

                    int a = jogo->tab[r][c], b = jogo->tab[r2][c2];
                    int pontos = pontos_do_par(a, b);
                    bool soma6 = (pontos == PONTOS_PIZZA);
                    if (pontos == 0 || soma6 != (passada == 0)) continue;

                    usada[r][c] = usada[r2][c2] = true;
                    total += pontos;
                    if (jogo->numPares < MAX_PARES) {
                        jogo->pares[jogo->numPares++] = (Par){r, c, a, r2, c2, b, pontos};
                    }
                    break;
                }
            }
        }
    }

    if (total == 0) return 0;

    for (int r = 0; r < alturaTab; r++)
        for (int c = 0; c < larguraTab; c++)
            if (usada[r][c]) jogo->tab[r][c] = VAZIA;

    jogo->pontuacao += total;
    aplicar_gravidade(jogo);
    return total;
}

/* A carta pousa na casa onde parou e fica la (empilha). So some se formar par. */
void fixar_peca(EstadoJogo *jogo) {
    if (!jogo || !jogo->tab || jogo->gameOver) return;
    if (!POSICAO_VALIDA(jogo->fatiaAtual.x, jogo->fatiaAtual.y)) return;

    jogo->numPares = 0;
    jogo->ganhoUltima = 0;
    jogo->tab[jogo->fatiaAtual.y][jogo->fatiaAtual.x] = jogo->fatiaAtual.pedacos;
    jogo->pecasFixadas++;

    int ganho;
    while ((ganho = resolver_rodada(jogo)) > 0) {
        jogo->ganhoUltima += ganho;
    }

    peca_proxima(jogo);
}

/* ---------- PLACAR (lista encadeada + arquivo) ---------- */

void recorde_inserir(Recorde **inicio, const char *nome, int pontos) {
    if (!inicio || !nome) return;

    Recorde *novo = (Recorde *)malloc(sizeof(Recorde));
    if (!novo) return;

    snprintf(novo->nome, TAM_NOME, "%s", nome);
    novo->pontos = pontos;
    novo->prox = NULL;

    if (*inicio == NULL || (*inicio)->pontos < pontos) {
        novo->prox = *inicio;
        *inicio = novo;
    } else {
        Recorde *atual = *inicio;
        while (atual->prox != NULL && atual->prox->pontos >= pontos) {
            atual = atual->prox;
        }
        novo->prox = atual->prox;
        atual->prox = novo;
    }
}

Recorde *recordes_carregar(const char *arquivo) {
    if (!arquivo) return NULL;

    FILE *f = fopen(arquivo, "r");
    if (!f) return NULL;

    Recorde *inicio = NULL;
    char nome[TAM_NOME];
    int pontos;

    while (fscanf(f, "%31s %d", nome, &pontos) == 2) {
        recorde_inserir(&inicio, nome, pontos);
    }

    fclose(f);
    return inicio;
}

int recordes_salvar(const Recorde *inicio, const char *arquivo) {
    if (!arquivo) return 0;

    FILE *f = fopen(arquivo, "w");
    if (!f) return 0;

    const Recorde *atual = inicio;
    while (atual != NULL) {
        fprintf(f, "%s %d\n", atual->nome, atual->pontos);
        atual = atual->prox;
    }

    fclose(f);
    return 1;
}

void recordes_liberar(Recorde *inicio) {
    Recorde *atual = inicio;
    while (atual != NULL) {
        Recorde *prox = atual->prox;
        free(atual);
        atual = prox;
    }
}