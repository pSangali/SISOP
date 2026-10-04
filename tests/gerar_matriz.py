#!/usr/bin/env python3
"""Gera matriz binaria aleatoria. Uso: gerar_matriz.py LINHAS COLUNAS DENSIDADE SEMENTE > arquivo.txt"""
import random
import sys

linhas, colunas, dens, semente = int(sys.argv[1]), int(sys.argv[2]), float(sys.argv[3]), int(sys.argv[4])
rnd = random.Random(semente)
out = [f"{linhas} {colunas}"]
for _ in range(linhas):
    out.append(" ".join("1" if rnd.random() < dens else "0" for _ in range(colunas)))
print("\n".join(out))
