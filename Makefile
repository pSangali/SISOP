CC      = cc
CFLAGS  = -std=c89 -Wall -Wextra -pedantic -O2
BIN     = bin

all: $(BIN)/sequencial $(BIN)/paralelo

$(BIN):
	mkdir -p $(BIN)

$(BIN)/sequencial: src/conta-objetos-sequencial.c | $(BIN)
	$(CC) $(CFLAGS) $< -o $@

$(BIN)/paralelo: src/conta-objetos-paralelo.c | $(BIN)
	$(CC) $(CFLAGS) -pthread $< -o $@

# Valida as 5 matrizes obrigatorias + aleatorias (seq x paralelo)
test: all
	sh tests/rodar_testes.sh

# Verifica corridas de dados com ThreadSanitizer
tsan: | $(BIN)
	$(CC) -std=c89 -g -O1 -fsanitize=thread -pthread src/conta-objetos-paralelo.c -o $(BIN)/paralelo_tsan
	for i in 1 2 3 4 5; do setarch -R $(BIN)/paralelo_tsan tests/ex$$i.txt 4 | grep Objetos; done

# Gera matriz grande e roda benchmark (resultados em results/desempenho.csv)
bench: all
	sh tests/benchmark.sh

clean:
	rm -rf $(BIN)

.PHONY: all test tsan bench clean
