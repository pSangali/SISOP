#!/bin/sh
# Gera matriz grande e mede sequencial x paralelo. Mediana de N execucoes.
# Uso: sh tests/benchmark.sh [LINHAS] [COLUNAS] [DENSIDADE] [REPETICOES]
cd "$(dirname "$0")/.." || exit 1
L=${1:-6000}; C=${2:-6000}; D=${3:-0.40}; R=${4:-7}
M=tests/grande_${L}x${C}.txt
mkdir -p results
[ -f $M ] || python3 tests/gerar_matriz.py $L $C $D 42 > $M
mediana() { sort -n | awk '{a[NR]=$1} END{print a[int((NR+1)/2)]}'; }
tempo() { "$@" | sed -n 's/^Tempo: \(.*\) ms/\1/p'; }
objs() { "$@" | sed -n 's/^Objetos encontrados: //p'; }
OUT=results/desempenho.csv
echo "versao,threads,objetos,mediana_ms,aceleracao" > $OUT
TS=$(for i in $(seq $R); do tempo bin/sequencial $M; done | mediana)
echo "sequencial,1,$(objs bin/sequencial $M),$TS,1.00" | tee -a $OUT
for n in 1 2 4 6 8 12; do
  TP=$(for i in $(seq $R); do tempo bin/paralelo $M $n; done | mediana)
  echo "paralelo,$n,$(objs bin/paralelo $M $n),$TP,$(LC_ALL=C awk "BEGIN{printf \"%.2f\", $TS/$TP}")" | tee -a $OUT
done
