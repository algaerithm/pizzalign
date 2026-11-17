#!/usr/bin/env python3
"""
gerar_sons.py - cria os efeitos sonoros e a musica ambiente do Pizzalign, sem baixar nada.

Tudo e sintetizado com contas simples (senoides somadas e envelopes), entao os sons sao
originais do projeto. Os efeitos saem em .wav e a musica em .ogg (menor para embutir).

Uso (de dentro da pasta do jogo):
    python ferramentas/gerar_sons.py
Precisa de numpy (pip install numpy) e do programa ffmpeg (so para converter a musica em .ogg).
Depois rode gerar_embutidos.py e compile de novo.

Para usar outra musica, basta trocar assets/musica.ogg por um arquivo seu com o mesmo nome.
"""
import os
import subprocess
import wave

import numpy as np

AQUI = os.path.dirname(os.path.abspath(__file__))
SAIDA = os.path.join(AQUI, "..", "assets")
TAXA = 22050          # amostras por segundo dos efeitos
TAXA_MUSICA = 32000


# ================================================================
#  ferramentas
# ================================================================
def tempo(dur, taxa=TAXA):
    return np.arange(int(dur * taxa)) / taxa


def nota(freq, dur, queda, parciais=((1, 1.0),), ataque=0.004, taxa=TAXA):
    """Uma nota: soma de senoides (parciais) que somem aos poucos. queda = segundos para cair a ~37%."""
    t = tempo(dur, taxa)
    som = np.zeros_like(t)
    for multiplo, forca in parciais:
        som += forca * np.sin(2 * np.pi * freq * multiplo * t) * np.exp(-t / (queda / multiplo ** 0.6))
    som *= np.minimum(1.0, t / ataque)                       # entrada suave (sem estalo)
    som *= np.minimum(1.0, (dur - t) / 0.01)                 # saida suave
    return som


def deslize(f0, f1, dur, queda, taxa=TAXA):
    """Tom que escorrega de f0 para f1 (batidas e 'pops')."""
    t = tempo(dur, taxa)
    freq = f0 * (f1 / f0) ** (t / dur)
    fase = 2 * np.pi * np.cumsum(freq) / taxa
    som = np.sin(fase) * np.exp(-t / queda)
    return som * np.minimum(1.0, t / 0.002) * np.minimum(1.0, (dur - t) / 0.01)


def chiado(dur, queda, suave=6, taxa=TAXA, semente=1):
    """Ruido curto e abafado (o 'corpo' de uma batida)."""
    t = tempo(dur, taxa)
    r = np.random.default_rng(semente).uniform(-1, 1, len(t))
    r = np.convolve(r, np.ones(suave) / suave, mode="same")  # media movel = corta os agudos
    return r * np.exp(-t / queda)


def juntar(partes, taxa=TAXA):
    """Mistura sons que comecam em momentos diferentes. partes = [(inicio em segundos, som), ...]"""
    fim = max(int(ini * taxa) + len(s) for ini, s in partes)
    saida = np.zeros(fim)
    for ini, s in partes:
        i = int(ini * taxa)
        saida[i:i + len(s)] += s
    return saida


def salvar_wav(nome, som, volume=0.8, taxa=TAXA):
    som = som / max(1e-9, np.max(np.abs(som))) * volume
    dados = (som * 32767).astype(np.int16)
    with wave.open(os.path.join(SAIDA, nome), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(taxa)
        w.writeframes(dados.tobytes())


SINO = ((1, 1.0), (2, 0.45), (3, 0.22), (4.2, 0.08))        # timbre de sininho / marimba
MADEIRA = ((1, 1.0), (2.8, 0.25))                            # toque seco de madeira

DO4, RE4, MI4, FA4, SOL4, LA4, SI4 = 261.63, 293.66, 329.63, 349.23, 392.00, 440.00, 493.88
DO5, RE5, MI5, FA5, SOL5, LA5, SI5, DO6 = 523.25, 587.33, 659.25, 698.46, 783.99, 880.00, 987.77, 1046.50


# ================================================================
#  efeitos
# ================================================================
def gerar_efeitos():
    # passar o mouse / mudar a opcao: um toque bem leve
    salvar_wav("som_passar.wav", nota(1320, 0.06, 0.012, MADEIRA), 0.5)
    # clicar: "pop" que desce
    salvar_wav("som_clique.wav", juntar([(0, deslize(620, 300, 0.09, 0.03)), (0, 0.25 * chiado(0.02, 0.006))]), 0.8)
    # mover a carta para o lado: toque seco e curto
    salvar_wav("som_mover.wav", nota(520, 0.05, 0.012, MADEIRA), 0.5)
    # carta pousando: batida grave e macia
    salvar_wav("som_pouso.wav", juntar([(0, deslize(190, 70, 0.16, 0.05)), (0, 0.5 * chiado(0.05, 0.012, suave=10))]), 0.9)
    # cartas iguais (+1): duas notas subindo
    salvar_wav("som_iguais.wav", juntar([(0.00, nota(MI5, 0.22, 0.09, SINO)), (0.08, nota(LA5, 0.34, 0.12, SINO))]), 0.8)
    # pizza completa (+3): acorde subindo, como um sininho de balcao
    salvar_wav("som_pizza.wav", juntar([(0.00, nota(DO5, 0.50, 0.16, SINO)), (0.07, nota(MI5, 0.50, 0.16, SINO)),
                                        (0.14, nota(SOL5, 0.55, 0.18, SINO)), (0.21, nota(DO6, 0.80, 0.28, SINO))]), 0.85)
    # subiu de nivel: escadinha rapida
    salvar_wav("som_nivel.wav", juntar([(0.00, nota(SOL4, 0.18, 0.07, SINO)), (0.06, nota(DO5, 0.18, 0.07, SINO)),
                                        (0.12, nota(MI5, 0.18, 0.07, SINO)), (0.18, nota(SOL5, 0.45, 0.16, SINO))]), 0.7)
    # fim de jogo: frase descendo, mais lenta
    salvar_wav("som_fim.wav", juntar([(0.00, nota(SOL4, 0.40, 0.16, SINO)), (0.18, nota(MI4, 0.40, 0.16, SINO)),
                                      (0.36, nota(DO4, 0.45, 0.18, SINO)), (0.60, nota(DO4 / 2 * 1.5, 1.00, 0.36, SINO))]), 0.8)


# ================================================================
#  musica ambiente: uma valsa de cantina (3 tempos), em loop
# ================================================================
def corda(freq, dur, brilho=1.0, queda=0.9, taxa=TAXA_MUSICA):
    """Corda dedilhada (bandolim/violao): muitos harmonicos, os agudos somem primeiro.
    Duas cordas levemente desafinadas entre si dao o som 'cheio' do bandolim."""
    t = tempo(dur, taxa)
    som = np.zeros_like(t)
    for desafino in (0.9985, 1.0015):
        f = freq * desafino
        k = 1
        while f * k < taxa * 0.45 and k <= 14:
            forca = np.sin(k * np.pi * 0.22) / k                      # dedilhada a 22% da corda
            som += forca * np.sin(2 * np.pi * f * k * t) * np.exp(-t * (1.0 / queda + 1.4 * k / brilho))
            k += 1
    return som * np.minimum(1.0, t / 0.003)


def gerar_musica():
    batida = 60.0 / 126                                               # 126 batidas por minuto
    hz = lambda semitons: 440.0 * 2 ** (semitons / 12.0)              # semitons a partir do La 4
    N = {"B3": -10, "C4": -9, "D4": -7, "E4": -5, "F4": -4, "G4": -2, "G#4": -1, "A4": 0, "B4": 2,
         "C5": 3, "D5": 5, "E5": 7, "F5": 8, "G5": 10, "G#5": 11, "A5": 12, "B5": 14, "C6": 15}

    # acordes: (baixo do 1o tempo, baixo alternado, notas do "pa-pa" nos tempos 2 e 3)
    ACORDES = {
        "C":  (-21, -26, (-14, -9, -5)),      # do  | sol  | sol do mi
        "G7": (-26, -19, (-14, -10, -4)),     # sol | re   | sol si fa
        "F":  (-16, -21, (-12, -9, -4)),      # fa  | do   | la do fa
        "Am": (-24, -17, (-12, -9, -5)),      # la  | mi   | la do mi
        "Dm": (-19, -24, (-12, -7, -4)),      # re  | la   | la re fa
        "E7": (-17, -22, (-13, -10, -7)),     # mi  | si   | sol# si re
    }
    # cada compasso: (acorde, [(nota, tempos), ...]) somando 3 tempos
    PARTE_A = [
        ("C",  [("E5", 1), ("G5", 1), ("E5", 1)]), ("C",  [("C5", 2), ("E5", 1)]),
        ("G7", [("D5", 1), ("F5", 1), ("D5", 1)]), ("G7", [("B4", 2), ("D5", 1)]),
        ("G7", [("F5", 1), ("E5", 1), ("D5", 1)]), ("G7", [("G5", 2), ("F5", 1)]),
        ("C",  [("E5", 1), ("D5", 1), ("C5", 1)]), ("C",  [("E5", 3)]),
        ("F",  [("A5", 1), ("G5", 1), ("F5", 1)]), ("F",  [("A5", 2), ("F5", 1)]),
        ("C",  [("G5", 1), ("F5", 1), ("E5", 1)]), ("C",  [("G5", 2), ("E5", 1)]),
        ("G7", [("F5", 1), ("E5", 1), ("D5", 1)]), ("G7", [("B4", 1), ("D5", 1), ("F5", 1)]),
        ("C",  [("E5", 1), ("D5", 1), ("C5", 1)]), ("C",  [("C5", 3)]),
    ]
    PARTE_B = [
        ("Am", [("A4", 1), ("C5", 1), ("E5", 1)]), ("Am", [("A5", 2), ("E5", 1)]),
        ("Dm", [("F5", 1), ("E5", 1), ("D5", 1)]), ("Dm", [("A4", 2), ("D5", 1)]),
        ("G7", [("B4", 1), ("D5", 1), ("F5", 1)]), ("G7", [("G5", 2), ("F5", 1)]),
        ("C",  [("E5", 1), ("G5", 1), ("C6", 1)]), ("C",  [("G5", 3)]),
        ("Am", [("A5", 1), ("G5", 1), ("E5", 1)]), ("Am", [("C5", 2), ("E5", 1)]),
        ("Dm", [("D5", 1), ("F5", 1), ("A5", 1)]), ("Dm", [("F5", 2), ("D5", 1)]),
        ("E7", [("E5", 1), ("G#5", 1), ("B5", 1)]), ("E7", [("G#5", 2), ("E5", 1)]),
        ("Am", [("A5", 1), ("E5", 1), ("C5", 1)]), ("G7", [("D5", 2), ("B4", 1)]),
    ]
    compassos = PARTE_A + PARTE_B
    total = int(round(len(compassos) * 3 * batida * TAXA_MUSICA))
    faixa = np.zeros(total)

    def tocar(inicio, som, volume):
        """Soma o som na faixa; o que passar do fim volta para o comeco (o loop emenda sem corte)."""
        i = int(round(inicio * TAXA_MUSICA)) % total
        som = som * volume
        while len(som) > 0:
            cabe = min(len(som), total - i)
            faixa[i:i + cabe] += som[:cabe]
            som, i = som[cabe:], 0

    for c, (acorde, melodia) in enumerate(compassos):
        t0 = c * 3 * batida
        baixo, baixo2, papa = ACORDES[acorde]
        tocar(t0, corda(hz(baixo if c % 2 == 0 else baixo2), 1.6, brilho=0.6, queda=1.1), 0.50)       # "um"
        for tempo_do_compasso in (1, 2):                                                              # "pa-pa"
            for j, n in enumerate(papa):
                tocar(t0 + tempo_do_compasso * batida + j * 0.012, corda(hz(n), 0.9, brilho=0.8, queda=0.45), 0.13)
        pos = 0.0
        for nome, dur in melodia:                                                                     # melodia
            inicio = t0 + pos * batida
            tocar(inicio, corda(hz(N[nome]), 1.8, brilho=1.3, queda=0.8), 0.36)
            if dur >= 2:                                             # nota longa: o bandolim repica a nota
                for r in range(1, int(dur * 2)):
                    tocar(inicio + r * batida / 2, corda(hz(N[nome]), 1.2, brilho=1.1, queda=0.5), 0.17)
            pos += dur

    eco = np.zeros(total)                                             # um pouco de sala: ecos curtos e fracos
    for atraso, forca in ((0.071, 0.22), (0.113, 0.16), (0.187, 0.11), (0.251, 0.07)):
        eco += forca * np.roll(faixa, int(atraso * TAXA_MUSICA))      # roll = o eco do fim cai no comeco
    faixa = faixa + eco
    faixa = faixa / np.max(np.abs(faixa)) * 0.7

    wav = os.path.join(SAIDA, "_musica_tmp.wav")
    with wave.open(wav, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(TAXA_MUSICA)
        w.writeframes((faixa * 32767).astype(np.int16).tobytes())
    ogg = os.path.join(SAIDA, "musica.ogg")
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", wav, "-c:a", "libvorbis", "-q:a", "3", ogg], check=True)
    os.remove(wav)
    return total / TAXA_MUSICA


if __name__ == "__main__":
    gerar_efeitos()
    segundos = gerar_musica()
    print("sons gerados em", os.path.normpath(SAIDA), "- musica com %.1f s" % segundos)
