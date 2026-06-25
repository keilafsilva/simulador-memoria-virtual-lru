# Simulador de Memória Virtual — LRU

Trabalho da disciplina de Análise e Aplicação de Sistemas Operacionais (UNISINOS).

## Descrição

Simula a tradução de endereços virtuais para endereços físicos usando paginação,
com substituição de páginas pelo algoritmo **LRU (Least Recently Used)**.

- Memória física: 64 KB → **8 frames** de 8 KB
- Memória virtual: 1 MB por processo → **128 páginas** de 8 KB
- Cada processo é simulado por uma **thread** (pthread), com sua própria tabela
  de páginas privada
- Os 8 frames da memória física são um recurso **global**, compartilhado entre
  as threads e protegido por **mutex**

## Funcionamento

Cada thread gera 10 endereços virtuais, alternando entre acessos sequenciais
(simulando varredura de um vetor) e acessos aleatórios (sem localidade), o que
evidencia o comportamento do LRU no log.

Para cada acesso:

1. O endereço virtual é decomposto em `(página, deslocamento)`.
2. Se a página já está na RAM → **HIT**.
3. Se não está:
   - Se existe frame livre, usa-o.
   - Se não existe, escolhe a **vítima LRU** (frame ocupado com o menor
     `ultimo_acesso`), desmapeia a página antiga e carrega a nova página nesse
     frame.
4. O endereço físico final e o conteúdo do byte acessado são exibidos.

A cada 5 acessos (contador global `relogio_logico`), o estado de todos os
frames é impresso.

## Compilação e execução

```bash
gcc -Wall -Wextra -pthread -o sim main.c
./sim
```

Para validar concorrência/memória durante o desenvolvimento:

```bash
gcc -Wall -Wextra -pthread -fsanitize=address -o sim main.c
./sim
```

## Estruturas principais

| Estrutura               | Papel                                                        |
|--------------------------|---------------------------------------------------------------|
| `EntradaTabelaPaginas`   | Entrada da tabela de páginas de um processo (presente/frame) |
| `Frame`                  | Frame da memória física (dono, página, último acesso, dados) |
| `Processo`               | Processo simulado: tabela de páginas + dados                 |

## Sincronização

`lock_memoria` protege todo acesso aos frames, ao relógio lógico e às tabelas
de páginas durante uma tradução de endereço (`mmu_traduzir`), evitando
condições de corrida entre as threads que disputam os mesmos frames.

## Aderência ao enunciado (Trabalho II — SO)

| Requisito                                              | Onde está no código                                  |
|---------------------------------------------------------|--------------------------------------------------------|
| Memória principal 64 KB / virtual 1 MB / blocos de 8 KB | `TAMANHO_MEM_FISICA`, `TAMANHO_MEM_VIRTUAL`, `TAMANHO_PAGINA` |
| Mínimo 2 processos leves (1 B a 1 MB)                   | `NUM_THREADS`, `inicializar_processo` (300 KB e 500 KB) |
| MMU com tabela de páginas e tradução de endereço        | `mmu_traduzir`, `EntradaTabelaPaginas`                |
| Nova instrução/endereço exibido na saída padrão         | `printf` de endereço virtual em `mmu_traduzir`        |
| Página presente → mostra conteúdo                       | bloco `HIT` em `mmu_traduzir`                         |
| Falta de página → aviso na saída padrão                 | bloco `FALTA DE PAGINA`                               |
| (a) Frame livre disponível → carrega direto              | `procurar_frame_livre`                                |
| (b) Sem frame livre → substituição                       | `escolher_vitima_lru` (algoritmo escolhido: **LRU**)  |
| Visualização clara dos resultados                        | logs por acesso + `imprimir_estado_memoria` a cada 5 acessos |
| Técnica escolhida                                        | multithreading (pthreads) com mutex global             |
