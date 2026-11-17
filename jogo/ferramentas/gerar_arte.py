#!/usr/bin/env python3
"""
gerar_arte.py - desenha a arte do Pizzalign (logo, mascote, cartas, tomate, icone da janela)
e prepara a foto de fundo (ferramentas/fundo_original.png).

Tudo no mesmo traco: contorno grosso de "canetao", cores chapadas e a paleta da bandeira italiana,
para combinar com o chapeu de chef da logo.

Uso (de dentro da pasta do jogo):
    python ferramentas/gerar_arte.py ferramentas/chapeu_original.png assets/fonte_titulo.ttf assets
Precisa de Pillow e numpy (pip install pillow numpy). Depois rode gerar_embutidos.py.
"""
import math
import os
import sys

import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

# ---------- paleta (a mesma do main.c) ----------
TINTA = (24, 20, 18)
VERDE = (71, 137, 101)
VERMELHO = (176, 42, 4)
PAPEL = (255, 248, 232)
BRANCO = (255, 253, 247)
QUEIJO = (246, 196, 69)
QUEIJO_ESCURO = (233, 170, 48)
CROSTA = (214, 140, 58)
PAPELAO = (214, 168, 112)
PAPELAO_CLARO = (232, 198, 150)
TOMATE = (206, 52, 30)
TOMATE_CLARO = (236, 104, 74)

SS = 4            # desenha 4x maior e reduz no fim (bordas lisas)
LADO = 320        # tamanho final de cada carta
TRACO = 13        # espessura do contorno, em pixels da carta final


# ================================================================
#  LOGO: chapeu + bigode da imagem original, com "Pizzalign" em Caveat
# ================================================================
def limpar_chapeu(caminho):
    """Recorta chapeu + bigode (sem o texto ITALIAN), refaz as 4 cores chapadas e deixa o fundo transparente."""
    orig = Image.open(caminho).convert("RGB").crop((100, 95, 520, 463))
    grande = orig.resize((orig.width * 4, orig.height * 4), Image.LANCZOS).filter(ImageFilter.GaussianBlur(2.2))
    a = np.asarray(grande).astype(np.int16)
    r, g = a[..., 0], a[..., 1]
    lum = a.mean(axis=2)
    croma = a.max(axis=2) - a.min(axis=2)
    # 0 = branco, 1 = verde, 2 = vermelho, 3 = tinta
    classe = np.where(croma < 45, np.where(lum < 128, 3, 0), np.where(r > g, 2, 1)).astype(np.uint8)

    idx = Image.fromarray(classe).copy()                 # copia: precisa poder pintar por cima
    for canto in [(0, 0), (idx.width - 1, 0), (0, idx.height - 1), (idx.width - 1, idx.height - 1),
                  (idx.width // 2, idx.height - 1), (idx.width // 2, 0)]:
        if idx.getpixel(canto) == 0:
            ImageDraw.floodfill(idx, canto, 9)          # 9 = fora do desenho
    classe = np.asarray(idx)

    cores = np.zeros(classe.shape + (4,), np.uint8)
    for valor, cor in [(0, BRANCO), (1, VERDE), (2, VERMELHO), (3, TINTA)]:
        cores[classe == valor] = cor + (255,)
    img = Image.fromarray(cores, "RGBA")
    img = img.resize((img.width // 2, img.height // 2), Image.LANCZOS)   # 2x o original, com bordas lisas
    return img.crop(img.getbbox())


def com_borda_de_adesivo(arte, margem=22):
    """Borda clara em volta do desenho (como um adesivo recortado), para ele ler sobre qualquer fundo."""
    tela = Image.new("RGBA", (arte.width + margem * 4, arte.height + margem * 4), (0, 0, 0, 0))
    tela.alpha_composite(arte, (margem * 2, margem * 2))
    alfa = tela.getchannel("A")
    halo = alfa.filter(ImageFilter.GaussianBlur(margem * 0.62)).point(lambda v: 255 if v > 14 else 0)
    fundo = halo.copy()                                  # tapa os buracos internos: o adesivo e uma peca so
    ImageDraw.floodfill(fundo, (0, 0), 128)
    halo = fundo.point(lambda v: 0 if v == 128 else 255)
    fora = halo.filter(ImageFilter.GaussianBlur(4)).point(lambda v: 255 if v > 60 else 0).filter(ImageFilter.GaussianBlur(1.2))
    halo = halo.filter(ImageFilter.GaussianBlur(1.2))
    saida = Image.new("RGBA", tela.size, (0, 0, 0, 0))
    saida.paste(Image.new("RGBA", tela.size, TINTA + (255,)), (0, 0), fora)      # filete de tinta por fora
    saida.paste(Image.new("RGBA", tela.size, PAPEL + (255,)), (0, 0), halo)
    saida.alpha_composite(tela)
    return saida.crop(saida.getbbox())


def gerar_logo(chapeu, fonte_ttf):
    fonte = ImageFont.truetype(fonte_ttf, 300)
    caixa = fonte.getbbox("Pizzalign")
    larg_txt, alt_txt = caixa[2] - caixa[0], caixa[3] - caixa[1]
    largura = max(chapeu.width, larg_txt) + 8
    arte = Image.new("RGBA", (largura, chapeu.height + alt_txt + 6), (0, 0, 0, 0))
    arte.alpha_composite(chapeu, ((largura - chapeu.width) // 2, 0))
    d = ImageDraw.Draw(arte)
    d.text(((largura - larg_txt) // 2 - caixa[0], chapeu.height + 2 - caixa[1]), "Pizzalign", font=fonte, fill=TINTA)
    return com_borda_de_adesivo(arte)


# ================================================================
#  CARTAS: a mesma pizza cortada em 6 fatias
# ================================================================
def raio_tremido(ang, raio, fase=0.0):
    """Circulo levemente irregular: parece desenhado a mao."""
    return raio * (1 + 0.014 * math.sin(3 * ang + 0.7 + fase) + 0.009 * math.sin(5 * ang + 2.1 + fase))


def setor(cx, cy, raio, a0, a1, fase=0.0, passos=140):
    """Poligono de um pedaco da pizza entre os angulos a0 e a1 (graus). 360 graus = pizza inteira."""
    inteiro = abs(a1 - a0) >= 360
    pts = [] if inteiro else [(cx, cy)]
    for i in range(passos + 1):
        ang = math.radians(a0 + (a1 - a0) * i / passos)
        rr = raio_tremido(ang, raio, fase)
        pts.append((cx + rr * math.cos(ang), cy + rr * math.sin(ang)))
    return pts


def contorno(d, pts, largura, cor=TINTA):
    d.line(pts + [pts[0], pts[1]], fill=cor, width=largura, joint="curve")
    for p in pts[:: max(1, len(pts) // 60)]:            # arredonda as quinas
        d.ellipse([p[0] - largura / 2, p[1] - largura / 2, p[0] + largura / 2, p[1] + largura / 2], fill=cor)


def folha(d, x, y, ang, tam):
    """Folhinha de manjericao."""
    pts = []
    for i in range(24):
        t = 2 * math.pi * i / 24
        px, py = tam * math.cos(t), tam * 0.48 * math.sin(t)
        pts.append((x + px * math.cos(ang) - py * math.sin(ang), y + px * math.sin(ang) + py * math.cos(ang)))
    d.polygon(pts, fill=VERDE)


def gerar_pizza(a0, a1, escala=1.0):
    """Desenha o pedaco da pizza entre a0 e a1 graus, centralizado na carta."""
    S = LADO * SS
    R = 138 * SS * escala
    cx = cy = S * 1.5                                    # desenha numa tela folgada e recorta depois
    tela = Image.new("RGBA", (S * 3, S * 3), (0, 0, 0, 0))

    forma = setor(cx, cy, R, a0, a1)
    mascara = Image.new("L", tela.size, 0)
    ImageDraw.Draw(mascara).polygon(forma, fill=255)

    miolo = Image.new("RGBA", tela.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(miolo)
    d.polygon(setor(cx, cy, R, 0, 360), fill=CROSTA)
    d.polygon(setor(cx, cy, R - 23 * SS * escala, 0, 360, fase=1.3), fill=QUEIJO)
    for k in range(6):                                   # recheio igual em todas as fatias
        meio = math.radians(k * 60)
        for dr, da, raio in [(86, 0.16, 15.5), (47, -0.30, 11.5)]:
            px = cx + dr * SS * escala * math.cos(meio + da)
            py = cy + dr * SS * escala * math.sin(meio + da)
            rr = raio * SS * escala
            d.ellipse([px - rr, py - rr, px + rr, py + rr], fill=VERMELHO)
            d.ellipse([px - rr * 0.45, py - rr * 0.6, px - rr * 0.05, py - rr * 0.2], fill=(204, 78, 44))
        fx = cx + 92 * SS * escala * math.cos(meio - 0.27)
        fy = cy + 92 * SS * escala * math.sin(meio - 0.27)
        folha(d, fx, fy, meio + 1.1, 12 * SS * escala)
        d.ellipse([cx + 66 * SS * escala * math.cos(meio + 0.42) - 4 * SS, cy + 66 * SS * escala * math.sin(meio + 0.42) - 4 * SS,
                   cx + 66 * SS * escala * math.cos(meio + 0.42) + 4 * SS, cy + 66 * SS * escala * math.sin(meio + 0.42) + 4 * SS],
                  fill=QUEIJO_ESCURO)
    for k in range(3):                                   # cortes das 6 fatias
        ang = math.radians(30 + k * 60)
        dx, dy = R * 1.1 * math.cos(ang), R * 1.1 * math.sin(ang)
        d.line([(cx - dx, cy - dy), (cx + dx, cy + dy)], fill=TINTA, width=int(4.5 * SS))

    tela.paste(miolo, (0, 0), mascara)
    contorno(ImageDraw.Draw(tela), forma, TRACO * SS)

    caixa = tela.getbbox()                               # centraliza o pedaco na carta
    recorte = tela.crop(caixa)
    carta = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    carta.alpha_composite(recorte, ((S - recorte.width) // 2, (S - recorte.height) // 2))
    return carta.resize((LADO, LADO), Image.LANCZOS)


def gerar_caixa():
    """Caixa de pizza aberta e vazia, vista de cima."""
    S = LADO * SS
    carta = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(carta)
    m, t = 26 * SS, TRACO * SS
    d.rounded_rectangle([m, m, S - m, S - m], radius=26 * SS, fill=TINTA)
    d.rounded_rectangle([m + t, m + t, S - m - t, S - m - t], radius=16 * SS, fill=PAPELAO)
    b = m + t + 15 * SS
    d.rounded_rectangle([b, b, S - b, S - b], radius=10 * SS, fill=PAPELAO_CLARO)
    cx = cy = S / 2
    R = 86 * SS
    for i in range(18):                                  # marca tracejada de onde a pizza vai
        a0, a1 = math.radians(i * 20), math.radians(i * 20 + 11)
        arco = [(cx + R * math.cos(a0 + (a1 - a0) * j / 8), cy + R * math.sin(a0 + (a1 - a0) * j / 8)) for j in range(9)]
        d.line(arco, fill=(176, 126, 74), width=int(5.5 * SS), joint="curve")
    for px, py, rr in [(-26, 18, 5), (30, -22, 4), (8, 40, 3.5), (-40, -30, 3.5), (44, 26, 3)]:   # farelos
        d.ellipse([cx + (px - rr) * SS, cy + (py - rr) * SS, cx + (px + rr) * SS, cy + (py + rr) * SS], fill=(190, 140, 84))
    return carta.resize((LADO, LADO), Image.LANCZOS)


def gerar_tomate():
    """Tomate emburrado: o bloco que so atrapalha."""
    S = LADO * SS
    carta = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(carta)
    cx, cy = S / 2, S / 2 + 14 * SS
    corpo = []
    for i in range(160):
        ang = 2 * math.pi * i / 160
        rr = raio_tremido(ang, 1.0, 0.4)
        corpo.append((cx + 126 * SS * rr * math.cos(ang), cy + 112 * SS * rr * math.sin(ang)))
    d.polygon(corpo, fill=TOMATE)
    brilho = Image.new("L", carta.size, 0)               # reflexo em arco no canto de cima
    db = ImageDraw.Draw(brilho)
    db.ellipse([cx - 98 * SS, cy - 88 * SS, cx + 20 * SS, cy + 30 * SS], fill=255)
    db.ellipse([cx - 82 * SS, cy - 70 * SS, cx + 52 * SS, cy + 62 * SS], fill=0)
    carta.paste(Image.new("RGBA", carta.size, TOMATE_CLARO + (255,)), (0, 0), brilho)
    d = ImageDraw.Draw(carta)
    contorno(d, corpo, TRACO * SS)

    topo = (cx, cy - 104 * SS)                           # folhas
    estrela = []
    for i in range(10):
        ang = math.radians(-90 + i * 36)
        rr = (62 if i % 2 == 0 else 20) * SS
        estrela.append((topo[0] + rr * math.cos(ang), topo[1] + rr * 0.62 * math.sin(ang) + 6 * SS))
    d.polygon(estrela, fill=VERDE)
    contorno(d, estrela, int(TRACO * 0.8 * SS))
    d.polygon(estrela, fill=VERDE)
    d.line([(topo[0], topo[1] + 2 * SS), (topo[0] + 10 * SS, topo[1] - 34 * SS)], fill=TINTA, width=int(TRACO * 0.95 * SS))

    oy = cy + 2 * SS                                     # carinha
    for lado in (-1, 1):
        ox = cx + lado * 40 * SS
        d.ellipse([ox - 11 * SS, oy - 13 * SS, ox + 11 * SS, oy + 13 * SS], fill=TINTA)
        d.line([(ox - lado * 26 * SS, oy - 36 * SS), (ox + lado * 22 * SS, oy - 20 * SS)], fill=TINTA, width=int(9 * SS))
    boca = [(cx + (-26 + 52 * j / 12) * SS, oy + (48 - 9 * math.sin(math.pi * j / 12)) * SS) for j in range(13)]
    d.line(boca, fill=TINTA, width=int(8.5 * SS), joint="curve")
    return carta.resize((LADO, LADO), Image.LANCZOS)


# ================================================================
#  FUNDO: a foto da rua, e uma copia pequena e borrada dela
# ================================================================
def gerar_fundo(saida):
    """fundo.png e a foto como ela e. fundo_borrado.png e uma miniatura desfocada: em tela cheia o jogo
    estica essa miniatura por tras, para as sobras dos lados nao ficarem pretas."""
    foto = Image.open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "fundo_original.png")).convert("RGB")
    foto.save(f"{saida}/fundo.png", optimize=True)
    mini = foto.resize((foto.width // 6, foto.height // 6), Image.LANCZOS).filter(ImageFilter.GaussianBlur(3))
    mini.save(f"{saida}/fundo_borrado.png", optimize=True)


# ================================================================
def main():
    if len(sys.argv) != 4:
        print(__doc__)
        sys.exit(1)
    chapeu_png, fonte_ttf, saida = sys.argv[1:]

    chapeu = limpar_chapeu(chapeu_png)
    logo = gerar_logo(chapeu, fonte_ttf)
    logo = logo.resize((720, round(720 * logo.height / logo.width)), Image.LANCZOS)
    logo.save(f"{saida}/logo.png", optimize=True)

    mascote = com_borda_de_adesivo(chapeu)
    mascote = mascote.resize((360, round(360 * mascote.height / mascote.width)), Image.LANCZOS)
    mascote.save(f"{saida}/mascote.png", optimize=True)

    icone = Image.new("RGBA", (max(mascote.size),) * 2, (0, 0, 0, 0))
    icone.alpha_composite(mascote, ((icone.width - mascote.width) // 2, (icone.height - mascote.height) // 2))
    icone.resize((64, 64), Image.LANCZOS).save(f"{saida}/icone.png", optimize=True)

    direita = {
        "carta_inteira": gerar_pizza(0, 360),
        "carta_metade_dir": gerar_pizza(-90, 90),
        "carta_fatia_dir": gerar_pizza(-30, 30, escala=1.22),
        "carta_falta_dir": gerar_pizza(30, 330),
    }
    for nome, img in direita.items():
        img.save(f"{saida}/{nome}.png", optimize=True)
        if nome != "carta_inteira":                      # o lado esquerdo e o espelho do direito
            img.transpose(Image.FLIP_LEFT_RIGHT).save(f"{saida}/{nome.replace('_dir', '_esq')}.png", optimize=True)
    gerar_caixa().save(f"{saida}/carta_caixa.png", optimize=True)
    gerar_tomate().save(f"{saida}/tomate.png", optimize=True)
    gerar_fundo(saida)
    print("arte gerada em", saida)


if __name__ == "__main__":
    main()
