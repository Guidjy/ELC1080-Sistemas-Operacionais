#include "processo.h"
#include <stdlib.h>
#include <assert.h>


#define N_PROCESSOS 10


struct processo_t {
    // conteúdo dos registradores do processo
    int id;
    int pc;
    int a;
    int x;
};


struct tabela_proc_t {
    int n_processos;        // numero de processos na tabela
    processo_t *processos;  // vetor de processos da tabela
};


processo_t *processo_cria() {
    processo_t *processo = malloc(sizeof(processo_t));
    assert(processo != NULL);
    
    return processo;
}


tabela_proc_t *tabela_proc_cria() {
    tabela_proc_t *tabela = malloc(sizeof(tabela_proc_t));
    assert(tabela != NULL);
    tabela->n_processos = 0;
    
    tabela->processos = malloc(N_PROCESSOS * sizeof(processo_t));
    assert(tabela->processos != NULL);

    return tabela;
}


void tabela_proc_insere(tabela_proc_t *self, processo_t proc, int pos) {
    assert(0 < pos && pos < N_PROCESSOS);
    self->processos[pos] = proc;
}