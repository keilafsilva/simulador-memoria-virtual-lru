/*
 * Simulador de Memoria Virtual - Algoritmo de substituicao: LRU
 * UNISINOS - Analise e Aplicacao de Sistemas Operacionais
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#define TAMANHO_PAGINA      8192
#define TAMANHO_MEM_FISICA  (64 * 1024)
#define TAMANHO_MEM_VIRTUAL (1024 * 1024)

#define NUM_FRAMES   (TAMANHO_MEM_FISICA  / TAMANHO_PAGINA)
#define NUM_PAGINAS  (TAMANHO_MEM_VIRTUAL / TAMANHO_PAGINA)

#define NUM_THREADS  2

typedef struct {
    int presente;
    int frame;
} EntradaTabelaPaginas;

typedef struct {
    int ocupado;
    int pid_dono;
    int pagina_logica;
    long ultimo_acesso;
    unsigned char dados[TAMANHO_PAGINA];
} Frame;

typedef struct {
    int pid;
    size_t tamanho_processo;
    EntradaTabelaPaginas tabela[NUM_PAGINAS];
    unsigned char *dados;
} Processo;

Frame memoria_fisica[NUM_FRAMES];
Processo processos[NUM_THREADS];

long relogio_logico = 0;
pthread_mutex_t lock_memoria;

void inicializar_memoria_fisica(void) {
    for (int i = 0; i < NUM_FRAMES; i++) {
        memoria_fisica[i].ocupado = 0;
        memoria_fisica[i].pid_dono = -1;
        memoria_fisica[i].pagina_logica = -1;
        memoria_fisica[i].ultimo_acesso = 0;
    }
}

void inicializar_processo(Processo *p, int pid, size_t tamanho) {
    p->pid = pid;
    p->tamanho_processo = tamanho;
    for (int i = 0; i < NUM_PAGINAS; i++) {
        p->tabela[i].presente = 0;
        p->tabela[i].frame = -1;
    }

    p->dados = malloc(tamanho);
    for (size_t i = 0; i < tamanho; i++) {
        p->dados[i] = (unsigned char) ((pid * 1000 + i) % 256);
    }
}

void imprimir_estado_memoria(void) {
    printf("---- Estado atual da memoria fisica (%d frames) ----\n", NUM_FRAMES);
    for (int i = 0; i < NUM_FRAMES; i++) {
        if (memoria_fisica[i].ocupado) {
            printf("  Frame %d: PID %d | pagina %d | ultimo_acesso=%ld\n",
                   i, memoria_fisica[i].pid_dono,
                   memoria_fisica[i].pagina_logica,
                   memoria_fisica[i].ultimo_acesso);
        } else {
            printf("  Frame %d: livre\n", i);
        }
    }
    printf("-----------------------------------------------------\n\n");
}

void decompor_endereco_virtual(unsigned int endereco_virtual,
                                int *numero_pagina, int *deslocamento) {
    *numero_pagina = endereco_virtual / TAMANHO_PAGINA;
    *deslocamento  = endereco_virtual % TAMANHO_PAGINA;
}

int procurar_frame_livre(void) {
    for (int i = 0; i < NUM_FRAMES; i++) {
        if (!memoria_fisica[i].ocupado) {
            return i;
        }
    }
    return -1;
}

/* Retorna o frame ocupado com menor ultimo_acesso (vitima do LRU) */
int escolher_vitima_lru(void) {
    int vitima = -1;
    long menor_acesso = -1;

    for (int i = 0; i < NUM_FRAMES; i++) {
        if (memoria_fisica[i].ocupado) {
            if (vitima == -1 || memoria_fisica[i].ultimo_acesso < menor_acesso) {
                vitima = i;
                menor_acesso = memoria_fisica[i].ultimo_acesso;
            }
        }
    }
    return vitima;
}

unsigned int mmu_traduzir(Processo *p, unsigned int endereco_virtual) {
    int pagina, deslocamento;
    decompor_endereco_virtual(endereco_virtual, &pagina, &deslocamento);

    pthread_mutex_lock(&lock_memoria);

    relogio_logico++;

    printf("[PID %d] Endereco virtual 0x%05X -> pagina %d, deslocamento %d\n",
           p->pid, endereco_virtual, pagina, deslocamento);

    int frame_destino;

    if (p->tabela[pagina].presente) {
        frame_destino = p->tabela[pagina].frame;
        printf("[PID %d] Pagina %d presente no frame %d (HIT)\n",
               p->pid, pagina, frame_destino);
    } else {
        printf("[PID %d] Pagina %d NAO presente -> FALTA DE PAGINA\n",
               p->pid, pagina);

        int frame_livre = procurar_frame_livre();

        if (frame_livre != -1) {
            frame_destino = frame_livre;
            printf("[PID %d] Frame livre encontrado: frame %d\n",
                   p->pid, frame_destino);
        } else {
            int vitima = escolher_vitima_lru();
            int pid_antigo = memoria_fisica[vitima].pid_dono;
            int pagina_antiga = memoria_fisica[vitima].pagina_logica;

            printf("[PID %d] Sem frame livre -> substituindo frame %d "
                   "(era PID %d, pagina %d)\n",
                   p->pid, vitima, pid_antigo, pagina_antiga);

            if (pid_antigo >= 0 && pid_antigo < NUM_THREADS &&
                pagina_antiga >= 0 && pagina_antiga < NUM_PAGINAS) {
                processos[pid_antigo].tabela[pagina_antiga].presente = 0;
                processos[pid_antigo].tabela[pagina_antiga].frame = -1;
            }

            frame_destino = vitima;
        }

        memoria_fisica[frame_destino].ocupado = 1;
        memoria_fisica[frame_destino].pid_dono = p->pid;
        memoria_fisica[frame_destino].pagina_logica = pagina;

        size_t inicio_pagina = (size_t) pagina * TAMANHO_PAGINA;
        size_t bytes_a_copiar = TAMANHO_PAGINA;
        if (inicio_pagina + bytes_a_copiar > p->tamanho_processo) {
            bytes_a_copiar = p->tamanho_processo - inicio_pagina;
        }
        memcpy(memoria_fisica[frame_destino].dados,
               p->dados + inicio_pagina,
               bytes_a_copiar);

        p->tabela[pagina].presente = 1;
        p->tabela[pagina].frame = frame_destino;
    }

    memoria_fisica[frame_destino].ultimo_acesso = relogio_logico;

    unsigned int endereco_fisico =
        (frame_destino * TAMANHO_PAGINA) + deslocamento;

    unsigned char conteudo = memoria_fisica[frame_destino].dados[deslocamento];

    printf("[PID %d] Endereco fisico final: 0x%04X | Conteudo acessado: "
           "%u (0x%02X)\n\n", p->pid, endereco_fisico, conteudo, conteudo);

    if (relogio_logico % 5 == 0) {
        imprimir_estado_memoria();
    }

    pthread_mutex_unlock(&lock_memoria);

    return endereco_fisico;
}

void *rotina_processo(void *arg) {
    Processo *p = (Processo *) arg;

    srand(time(NULL) ^ p->pid);

    int num_acessos = 10;
    unsigned int ponteiro_sequencial = 0;

    for (int i = 0; i < num_acessos; i++) {
        unsigned int endereco_virtual;

        /* alterna entre acesso sequencial e aleatorio para evidenciar o LRU */
        if ((i / 3) % 2 == 0) {
            endereco_virtual = ponteiro_sequencial % (unsigned int) p->tamanho_processo;
            ponteiro_sequencial += TAMANHO_PAGINA / 2;
        } else {
            endereco_virtual = rand() % (unsigned int) p->tamanho_processo;
        }

        mmu_traduzir(p, endereco_virtual);

        usleep(100000);
    }

    return NULL;
}

int main(void) {
    pthread_t threads[NUM_THREADS];

    inicializar_memoria_fisica();
    pthread_mutex_init(&lock_memoria, NULL);

    inicializar_processo(&processos[0], 0, 300 * 1024);
    inicializar_processo(&processos[1], 1, 500 * 1024);

    printf("=== Iniciando simulador de memoria virtual (LRU) ===\n");
    printf("Frames disponiveis: %d | Paginas por processo: %d\n\n",
           NUM_FRAMES, NUM_PAGINAS);

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, rotina_processo, &processos[i]);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    pthread_mutex_destroy(&lock_memoria);

    for (int i = 0; i < NUM_THREADS; i++) {
        free(processos[i].dados);
    }

    printf("=== Simulacao encerrada ===\n");
    return 0;
}
