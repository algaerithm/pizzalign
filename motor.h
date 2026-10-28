#ifndef MOTOR_H
#define MOTOR_H
#define MAX_LADO 5 //maior tabuleiro
#define VAZIA (-1) //casa sem carta

static const int FATIAS[8] = {0, 1, 3, 5, 6, 5, 3, 1}; // [caixa vazia = 0] - [uma fatia (esquerda)] - [metade (esquerda) = 2] - [faltando uma fatia (esquerda) = 3] - [pizza inteira = 4] - [faltando uma fatia (direita) = 5] - [metade (direita) = 6] - [uma fatia (direita) = 7]
static const int LADO[8] = {0, -1, -1, 1, 0, -1, 1, 1}; // lado da pizza: -1 = esquerda / 1 = direita / 0 = caixa vazia ou inteira

static int tamanho = 4; 
static int tabuleiro[MAX_LADO * MAX_LADO]; //tabuleiro tamanho x tamanho
static void novo_tabuleiro(int lado){ //casa vazia ou o tipo da carta
    tamanho = lado;
    for(int i = 0; i < MAX_LADO * MAX_LADO; i++){
        tabuleiro[i] = VAZIA;
    }
}

static int carta_na_casa(int lin, int col){ // qual carta está na linha e coluna / vazia se a casa estiver vazia
    if(lin < 0 || lin >= tamanho || col < 0 || col >= tamanho){
        return VAZIA;
    }
    return tabuleiro[lin * tamanho + col];
}

// SEGUE PRA PARTE 2

// 28/10/26 - 10:10 AM

static int se_completam(int a, int b){ // as cartas se completam quando formam uma pizza inteira
    return(a == 0 && b == 4) || (a == 4 && b == 0) || (a == 2 && b == 6) || (a == 6 && b == 2)
        || (a == 1 && b == 3) || (a == 3 && b == 1) || (a == 5 && b == 7) || (a == 7 && b == 5);
}

static int pontuar (int pos, int carta){ // Qtd de pontos por colocar a carta na posicao (com as regras implementadas)
    int lin = pos / tamanho;
    int col = pos % tamanho;
    int pontos = 0;

    tabuleiro[pos] = carta;

    int d_lin[4] = {-1, 1, 0, 0}; // vizinhas
    int d_col[4] = {0, 0, -1, 1};
    for (int k = 0; k < 4; k++){
        int v = carta_na_casa(lin + d_lin[k], col + d_col[k]);
        if (v == VAZIA){
            continue;
        }
        if (v == carta){
            pontos += 1;
        }
        else if (se_completam(carta, v)){
            pontos += 2;
        }
    }

    for (int sentido = 0; sentido < 2; sentido++){ //trios (0 - horizontal e 1 - vertical)
        int passo_lin = (sentido == 1);
        int passo_col = (sentido == 0);
        for (int inicio = -2; inicio <= 0; inicio++){
            int a = carta_na_casa (lin + inicio * passo_lin, col + inicio * passo_col);
            int b = carta_na_casa (lin + (inicio + 1) * passo_lin, col + (inicio + 1) * passo_col);
            int c = carta_na_casa (lin + (inicio + 2) * passo_lin, col + (inicio + 2) * passo_col);
            if (a == VAZIA || b == VAZIA || c == VAZIA){
                continue; //falta carta ou sai do tabuleiro
            }
            int esq = LADO[a] < 0 || LADO[b] < 0 || LADO[c] < 0;
            int dir = LADO[a] > 0 || LADO[b] > 0 || LADO[c] > 0;
            if (esq && dir){
                continue; //lados diferentes nao valem
            }
            int fa = FATIAS[a], fb = FATIAS[b], fc = FATIAS[c];
            if ((fa < fb && fb < fc) || (fa > fb && fb > fc)){

            } 
        }  
    }

    tabuleiro[pos] == VAZIA; 
    return pontos;
}
 
// segue pra parte 3

