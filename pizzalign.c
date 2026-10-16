/* PIZZALIGN - jogo de cartas de pizza no terminal.
 *   gcc -std=c99 -Wall -Wextra -pedantic -O2 pizzalign.c -o pizzalign
 *
 * Cada pizza tem 4 fatias. As 8 cartas formam um ciclo (depois da 8 volta a 1).
 * Pontos (so quando a carta jogada forma algo com as vizinhas):
 *   +1 carta igual encostada | +2 duas cartas que completam uma pizza (2+6, 3+7, 4+8)
 *   +1 por carta em sequencia crescente de 3+ cartas do ciclo, em linha reta
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#endif

#define N           3                      /* tabuleiro N x N */
#define NUM_CARTAS  8
#define TAM_BARALHO (NUM_CARTAS * 4)
#define TAM_MAO     3
#define VAZIO       (-1)
#define ALTURA      5                      /* cada carta tem 10 x 5 caracteres */
#define FIM_COR     "\033[0m"

/* garante que o baralho nunca acaba: maos iniciais + uma compra por jogada */
typedef char baralho_suficiente[TAM_BARALHO >= 2 * TAM_MAO + N * N ? 1 : -1];

static const char *COR[2] = {"\033[36m", "\033[33m"};

static const char *NOME[NUM_CARTAS] = {
    "Vazia", "fatia (esq)", "Meia (esq)", "3/4 (dir)",
    "Completa", "3/4 (esq)", "Meia (dir)", "fatia (dir)"
};

static const char *ARTE[NUM_CARTAS][ALTURA] = {
    {"  ......  ", " .      . ", ".        .", " .      . ", "  ......  "},
    {"  ......  ", " /:     . ", "|:o      .", " \\:     . ", "  '.....  "},
    {"  .==...  ", " /:o:   . ", "|:o:o    .", " \\:o:   . ", "  '==...  "},
    {"  .====.  ", " /:o:o: . ", "|:o:o:o  .", " \\:o:o: . ", "  '====.  "},
    {"  .====.  ", " /:o:o:o\\ ", "|:o:o:o:o|", " \\:o:o:o/ ", "  '====`  "},
    {"  .====.  ", " . o:o:o\\ ", ".  :o:o:o|", " . o:o:o/ ", "  .====`  "},
    {"  ...==.  ", " .   o:o\\ ", ".    :o:o|", " .   o:o/ ", "  ...==`  "},
    {"  ......  ", " .     o\\ ", ".      :o|", " .     o/ ", "  .....`  "}
};

static const char *FALAS[3][3] = {
    {"Mamma mia, que jogada!", "Isso e pizza de verdade!", "Ha! Essa foi saborosa!"},
    {"Eita! Voce queimou minha massa...", "Hmm, nada mal.", "Na proxima eu giro melhor!"},
    {"Deixa a massa descansar...", "O forno ainda esta esquentando.", "Mais uma fatia, por favor!"}
};

typedef struct {
    int tab[N][N];             /* carta de cada casa (VAZIO se livre) */
    int dono[N][N];            /* quem jogou a carta: 0 ou 1 */
    int baralho[TAM_BARALHO], topo;
    int mao[2][TAM_MAO];
    int pontos[2];
    int jogadas;               /* casas ja preenchidas */
    int vez;                   /* 0 ou 1 */
    int cpu;                   /* 1 = o jogador 2 e' o pizzaiolo */
    const char *nome[2];
} Jogo;

/* ------------------------------------------------------------------ */
/*  Pontuacao                                                          */
/* ------------------------------------------------------------------ */

/* Carta que completa uma pizza inteira com 'c' (ou -1 se nenhuma). */
static int par(int c)
{
    return c % 4 ? (c + 4) % 8 : -1;
}

/* Pontos de sequencia crescente (do ciclo) que passa pela posicao pos de uma linha,
 * lida nos dois sentidos. */
static int pontos_seq(const int *l, int pos)
{
    int pts = 0, dir;

    for (dir = 1; dir >= -1; dir -= 2) {
        int n = 1, i;
        for (i = pos; i + dir >= 0 && i + dir < N && l[i + dir] != VAZIO
                      && l[i + dir] == (l[i] + 1) % NUM_CARTAS; i += dir) {
            n++;
        }
        for (i = pos; i - dir >= 0 && i - dir < N && l[i - dir] != VAZIO
                      && l[i] == (l[i - dir] + 1) % NUM_CARTAS; i -= dir) {
            n++;
        }
        if (n >= 3) {
            pts += n;
        }
    }
    return pts;
}

/* Quantos pontos 'carta' daria na casa vazia (lin, col). O tabuleiro volta como estava. */
static int pontuar(int t[N][N], int lin, int col, int carta)
{
    static const int dl[4] = {-1, 1, 0, 0}, dc[4] = {0, 0, -1, 1};
    int linha[N], pts = 0, d, i;

    t[lin][col] = carta;
    for (d = 0; d < 4; d++) {
        int a = lin + dl[d], b = col + dc[d];
        if (a < 0 || a >= N || b < 0 || b >= N || t[a][b] == VAZIO) {
            continue;
        }
        if (t[a][b] == carta) {
            pts += 1;
        } else if (t[a][b] == par(carta)) {
            pts += 2;
        }
    }
    for (i = 0; i < N; i++) linha[i] = t[lin][i];
    pts += pontos_seq(linha, col);
    for (i = 0; i < N; i++) linha[i] = t[i][col];
    pts += pontos_seq(linha, lin);
    t[lin][col] = VAZIO;
    return pts;
}

/* ------------------------------------------------------------------ */
/*  Regras                                                             */
/* ------------------------------------------------------------------ */

static void novo_jogo(Jogo *j, int cpu)
{
    int i, k, tmp;

    memset(j, 0, sizeof *j);
    for (i = 0; i < N * N; i++) j->tab[i / N][i % N] = VAZIO;

    for (i = 0; i < TAM_BARALHO; i++) j->baralho[i] = i % NUM_CARTAS;
    for (i = TAM_BARALHO - 1; i > 0; i--) {
        k = rand() % (i + 1);
        tmp = j->baralho[i];
        j->baralho[i] = j->baralho[k];
        j->baralho[k] = tmp;
    }
    for (i = 0; i < 2 * TAM_MAO; i++) j->mao[i / TAM_MAO][i % TAM_MAO] = j->baralho[j->topo++];

    j->vez = rand() % 2;
    j->cpu = cpu;
    j->nome[0] = cpu ? "Voce" : "Jogador 1";
    j->nome[1] = cpu ? "Chef Gennaro" : "Jogador 2";
}

/* Joga a carta 'idx' da mao em (lin, col), repoe a mao e devolve os pontos ganhos. */
static int jogar_carta(Jogo *j, int p, int idx, int lin, int col)
{
    int carta = j->mao[p][idx];
    int pts = pontuar(j->tab, lin, col, carta);

    j->tab[lin][col] = carta;
    j->dono[lin][col] = p;
    j->pontos[p] += pts;
    j->jogadas++;
    j->mao[p][idx] = j->baralho[j->topo++];
    return pts;
}

/* ------------------------------------------------------------------ */
/*  Tela e entrada                                                     */
/* ------------------------------------------------------------------ */

static int ler_linha(const char *msg, char *buf, int n)
{
    fputs(msg, stdout);
    fflush(stdout);
    return fgets(buf, n, stdin) != NULL;
}

/* Le um numero entre min e max. Retorna -1 se a entrada acabou. */
static int ler_opcao(int min, int max)
{
    char t[32];
    int n;

    for (;;) {
        if (!ler_linha("> ", t, sizeof t)) return -1;
        if (sscanf(t, "%d", &n) == 1 && n >= min && n <= max) return n;
        printf("Escolha um numero entre %d e %d.\n", min, max);
    }
}

static void mostrar_borda(void)
{
    int c;
    for (c = 0; c < N; c++) fputs("+----------", stdout);
    puts("+");
}

static void mostrar_tabuleiro(const Jogo *j)
{
    int l, c, r;

    putchar('\n');
    for (l = 0; l < N; l++) {
        mostrar_borda();
        for (r = 0; r < ALTURA; r++) {
            for (c = 0; c < N; c++) {
                int carta = j->tab[l][c];
                putchar('|');
                if (carta != VAZIO) {
                    printf("%s%s%s", COR[j->dono[l][c]], ARTE[carta][r], FIM_COR);
                } else if (r == 2) {
                    printf("  [%d,%d]   ", l + 1, c + 1);
                } else {
                    fputs("          ", stdout);
                }
            }
            puts("|");
        }
    }
    mostrar_borda();
}

static void mostrar_placar(const Jogo *j)
{
    printf("%s%s%s %d  x  %s%s%s %d\n", COR[0], j->nome[0], FIM_COR, j->pontos[0],
           COR[1], j->nome[1], FIM_COR, j->pontos[1]);
}

static void mostrar_mao(const Jogo *j, int p)
{
    int i, r;

    for (i = 0; i < TAM_MAO; i++) printf("    [%d]     ", i + 1);
    putchar('\n');
    for (r = 0; r < ALTURA; r++) {
        for (i = 0; i < TAM_MAO; i++) printf("%s  ", ARTE[j->mao[p][i]][r]);
        putchar('\n');
    }
    for (i = 0; i < TAM_MAO; i++) printf("[%d] %-14s", i + 1, NOME[j->mao[p][i]]);
    putchar('\n');
}

/* ------------------------------------------------------------------ */
/*  Jogadas                                                            */
/* ------------------------------------------------------------------ */

/* Mostra a tela e le a jogada. Retorna 1 se jogou, 0 se quis sair. */
static int turno_humano(Jogo *j, int p, int *pts)
{
    char txt[64], extra;
    int carta, l, c;

    mostrar_tabuleiro(j);
    mostrar_placar(j);
    printf("\nMao de %s%s%s:\n", COR[p], j->nome[p], FIM_COR);
    mostrar_mao(j, p);
    puts("Jogada: carta linha coluna (ex: 2 3 1)  |  q sair");

    for (;;) {
        printf("%s", j->nome[p]);
        if (!ler_linha("> ", txt, sizeof txt) || txt[0] == 'q' || txt[0] == 'Q') return 0;
        if (sscanf(txt, "%d %d %d %c", &carta, &l, &c, &extra) != 3) {
            puts("Use: carta linha coluna (ex: 2 3 1)");
            continue;
        }
        carta--; l--; c--;
        if (carta < 0 || carta >= TAM_MAO || l < 0 || l >= N || c < 0 || c >= N) {
            puts("Numero fora do limite.");
            continue;
        }
        if (j->tab[l][c] != VAZIO) {
            puts("Essa casa ja esta ocupada.");
            continue;
        }
        *pts = jogar_carta(j, p, carta, l, c);
        return 1;
    }
}

/* Maior pontuacao que o adversario poderia fazer em seguida, com qualquer carta. */
static int melhor_resposta(int t[N][N])
{
    int melhor = 0, l, c, k;

    for (l = 0; l < N; l++) {
        for (c = 0; c < N; c++) {
            if (t[l][c] != VAZIO) continue;
            for (k = 0; k < NUM_CARTAS; k++) {
                int p = pontuar(t, l, c, k);
                if (p > melhor) melhor = p;
            }
        }
    }
    return melhor;
}

/* Escolhe a jogada que mais pontua, descontando os pontos que deixa para o adversario. */
static void escolher_jogada(Jogo *j, int p, int *idx, int *lin, int *col)
{
    int melhor = -1000000, empates = 0, i, l, c;

    for (i = 0; i < TAM_MAO; i++) {
        int carta = j->mao[p][i];
        for (l = 0; l < N; l++) {
            for (c = 0; c < N; c++) {
                int v;
                if (j->tab[l][c] != VAZIO) continue;
                v = pontuar(j->tab, l, c, carta) * 10;
                if (j->jogadas + 1 < N * N) {
                    j->tab[l][c] = carta;
                    v -= melhor_resposta(j->tab) * 6;
                    j->tab[l][c] = VAZIO;
                }
                if (v > melhor) {
                    melhor = v;
                    empates = 0;
                }
                /* sorteia entre as jogadas empatadas sem precisar guardar a lista */
                if (v == melhor && rand() % ++empates == 0) {
                    *idx = i; *lin = l; *col = c;
                }
            }
        }
    }
}

static const char *fala_do_chef(int pontos_chef, int pontos_humano)
{
    return FALAS[pontos_chef >= 3 ? 0 : pontos_humano >= 3 ? 1 : 2][rand() % 3];
}

/* ------------------------------------------------------------------ */
/*  Partida e menu                                                     */
/* ------------------------------------------------------------------ */

static void jogar_partida(Jogo *j)
{
    char buf[8], aviso[64];
    int humano = 0, pts = 0;

    while (j->jogadas < N * N) {
        int p = j->vez;

        if (j->cpu && p == 1) {
            int idx = 0, l = 0, c = 0, carta;
            escolher_jogada(j, p, &idx, &l, &c);
            carta = j->mao[p][idx];
            pts = jogar_carta(j, p, idx, l, c);
            printf("\n%s: \"%s\"\n%s jogou %s em [%d,%d]\n",
                j->nome[1], fala_do_chef(pts, humano), j->nome[1], NOME[carta], l + 1, c + 1);
        } else {
            if (!j->cpu) {   /* esconde a mao do outro jogador */
                snprintf(aviso, sizeof aviso, "\033[2J\033[HVez de %s. Enter para continuar... ", j->nome[p]);
                if (!ler_linha(aviso, buf, sizeof buf)) return;
            }
            if (!turno_humano(j, p, &pts)) return;
            humano = pts;
            printf("+%d\n", pts);
            if (!j->cpu) {
                mostrar_tabuleiro(j);
                if (!ler_linha("Enter para passar a vez... ", buf, sizeof buf)) return;
            }
        }
        j->vez = 1 - p;
    }

    puts("\n=== FIM DE JOGO ===");
    mostrar_tabuleiro(j);
    mostrar_placar(j);
    if (j->pontos[0] == j->pontos[1]) {
        puts("Empate!");
    } else {
        int v = j->pontos[1] > j->pontos[0];
        printf("%s%s%s venceu!\n", COR[v], j->nome[v], FIM_COR);
        if (j->cpu) {
            printf("%s: \"%s\"\n", j->nome[1], v ? "Hoje a casa ganhou! Volte sempre!" : "Voce e o novo mestre pizzaiolo!");
        }
    }
}

/* Retorna 0 quando o jogador quer sair. */
static int menu(void)
{
    Jogo j;
    int op;

    puts("\n=========== PIZZALIGN ===========\n1) 1 jogador (contra o pizzaiolo)\n2) 2 jogadores\n0) Sair");
    op = ler_opcao(0, 2);
    if (op <= 0) return 0;
    novo_jogo(&j, op == 1);
    jogar_partida(&j);
    return 1;
}

int main(void)
{
#ifdef _WIN32   /* liga as cores ANSI no terminal do Windows */
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD modo;
    if (GetConsoleMode(h, &modo)) SetConsoleMode(h, modo | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif
    srand((unsigned)time(NULL));
    while (menu()) {
        /* volta ao menu depois de cada partida */
    }
    puts("Ate a proxima fornada!");
    return 0;
}
