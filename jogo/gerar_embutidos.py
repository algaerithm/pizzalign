#!/usr/bin/env python3
"""
gerar_embutidos.py - transforma as imagens, fontes e sons da pasta assets/ no arquivo assets_embutidos.h,
que o main.c usa para o jogo abrir com tudo dentro do proprio executavel (sem depender de pastas).

Quando trocar qualquer arquivo de assets/ (logo, cartas, fonte, sons), rode:
    python gerar_embutidos.py
e compile o jogo de novo.

Arquivos esperados em assets/:
    fundo.png        foto de fundo de todas as telas (a rua)
    fundo_borrado.png  miniatura desfocada do fundo (cobre as sobras em tela cheia)
    logo.png         adesivo da logo: chapeu de chef + bigode + "Pizzalign" (fundo transparente)
    mascote.png      so o chapeu + bigode (aparece no topo da partida)
    icone.png        icone da janela (64 x 64)
    carta_caixa.png, carta_fatia_esq/dir.png, carta_metade_esq/dir.png,
    carta_falta_esq/dir.png, carta_inteira.png     as 8 cartas de pizza
    tomate.png       bloco tomate
    fonte_titulo.ttf Caveat Bold    fonte_texto.ttf Caveat SemiBold (peso 600)
    som_*.wav        efeitos sonoros          musica.ogg   musica ambiente (gerados por ferramentas/gerar_sons.py)
Os paineis, os botoes e as teclas sao desenhados em codigo no main.c.
"""
import os

AQUI = os.path.dirname(os.path.abspath(__file__))
PASTA = os.path.join(AQUI, "assets")

ITENS = [
    ("FUNDO_PNG", "fundo.png"), ("FUNDO_BORRADO_PNG", "fundo_borrado.png"),
    ("LOGO_PNG", "logo.png"), ("MASCOTE_PNG", "mascote.png"), ("ICONE_PNG", "icone.png"),
    ("FONTE_TITULO_TTF", "fonte_titulo.ttf"), ("FONTE_TEXTO_TTF", "fonte_texto.ttf"),
    ("CARTA_CAIXA_PNG", "carta_caixa.png"), ("CARTA_FATIA_E_PNG", "carta_fatia_esq.png"),
    ("CARTA_FATIA_D_PNG", "carta_fatia_dir.png"), ("CARTA_METADE_E_PNG", "carta_metade_esq.png"),
    ("CARTA_METADE_D_PNG", "carta_metade_dir.png"), ("CARTA_FALTA_E_PNG", "carta_falta_esq.png"),
    ("CARTA_FALTA_D_PNG", "carta_falta_dir.png"), ("CARTA_INTEIRA_PNG", "carta_inteira.png"),
    ("TOMATE_PNG", "tomate.png"),
    ("SOM_PASSAR_WAV", "som_passar.wav"), ("SOM_CLIQUE_WAV", "som_clique.wav"), ("SOM_MOVER_WAV", "som_mover.wav"),
    ("SOM_POUSO_WAV", "som_pouso.wav"), ("SOM_IGUAIS_WAV", "som_iguais.wav"), ("SOM_PIZZA_WAV", "som_pizza.wav"),
    ("SOM_NIVEL_WAV", "som_nivel.wav"), ("SOM_FIM_WAV", "som_fim.wav"), ("MUSICA_OGG", "musica.ogg"),
]

saida = ["/* assets_embutidos.h - GERADO por gerar_embutidos.py; nao edite a mao. */",
         "#ifndef ASSETS_EMBUTIDOS_H", "#define ASSETS_EMBUTIDOS_H", ""]
for nome, arquivo in ITENS:
    dados = open(os.path.join(PASTA, arquivo), "rb").read()
    saida.append(f"/* {arquivo} ({len(dados)} bytes) */")
    saida.append(f"static const unsigned char {nome}[] = {{")
    for i in range(0, len(dados), 24):
        saida.append(",".join(str(b) for b in dados[i:i + 24]) + ",")
    saida.append("};")
    saida.append(f"static const int {nome}_TAM = {len(dados)};")
    saida.append("")
saida.append("#endif")
open(os.path.join(AQUI, "assets_embutidos.h"), "w", newline="\n").write("\n".join(saida) + "\n")   # mesma quebra de linha no Windows e no Linux
print("assets_embutidos.h gerado com", len(ITENS), "arquivos")
