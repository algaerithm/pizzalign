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

