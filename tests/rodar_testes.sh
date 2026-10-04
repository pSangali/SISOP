#!/bin/sh
# Compara sequencial x paralelo nas 5 matrizes obrigatorias (com valor esperado)
# e em matrizes aleatorias (sequencial e' a referencia). Uso: make test
cd "$(dirname "$0")/.." || exit 1
SEQ=bin/sequencial; PAR=bin/paralelo
ESP="3 4 5 6 7"; falhas=0
conta() { "$@" | sed -n 's/^Objetos encontrados: //p'; }

echo "== Matrizes obrigatorias =="
printf "%-4s %-9s %-8s %-11s %s\n" Ex Dim Esperado Sequencial "Paralelo(2/3/4/8 threads)"
i=1
for e in $ESP; do
  f=tests/ex$i.txt
  dim=$(head -1 $f | tr ' ' 'x')
  s=$(conta $SEQ $f)
  p=""
  for n in 2 3 4 8; do
    r=$(conta $PAR $f $n); p="$p$r "
    [ "$r" = "$e" ] || falhas=$((falhas+1))
  done
  [ "$s" = "$e" ] || falhas=$((falhas+1))
  printf "%-4s %-9s %-8s %-11s %s\n" $i $dim $e $s "$p"
  i=$((i+1))
done

echo "== Matrizes aleatorias (sequencial x paralelo) =="
tmp=${TMPDIR:-/tmp}/matriz_teste_$$.txt
for seed in 1 2 3 4 5 6 7 8 9 10 11 12; do
  for dens in 0.2 0.45 0.7; do
    l=$((seed*3+2)); c=$((seed*5+1))
    python3 tests/gerar_matriz.py $l $c $dens $seed > $tmp
    s=$(conta $SEQ $tmp)
    for n in 2 3 4 7 16; do
      p=$(conta $PAR $tmp $n)
      if [ "$s" != "$p" ]; then
        echo "FALHA: ${l}x${c} dens=$dens seed=$seed threads=$n seq=$s par=$p"
        falhas=$((falhas+1))
      fi
    done
  done
done
rm -f $tmp
if [ $falhas -eq 0 ]; then echo "TODOS OS TESTES PASSARAM"; else echo "$falhas FALHA(S)"; exit 1; fi
