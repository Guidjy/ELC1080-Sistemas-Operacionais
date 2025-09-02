#ifndef PROCESSO_H
#define PROCESSO_H


typedef struct processo_t processo_t;

typedef struct tabela_proc_t tabela_proc_t;


// cria um processo
processo_t *processo_cria();

// cria uma tabela de processos
tabela_proc_t *tabela_proc_cria();

// insere um processo na tabela
void tabela_proc_insere(tabela_proc_t *self, processo_t proc, int pos);


#endif  // PROCESSO_H