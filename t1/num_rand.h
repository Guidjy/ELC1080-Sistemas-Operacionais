// relogio.h
// dispositivo de E/S que gera um número aleatório quando lido
// simulador de computador
// so25b

#ifndef NUM_RAND_H
#define NUM_RAND_H

#include "err.h"

typedef struct num_rand_t num_rand_t;

// cria e inicializa um número aleatório
num_rand_t *num_rand_cria();

// destrói um número aleatório
void num_rand_destroi(num_rand_t *self);

// leitura do número aleatório
err_t num_rand_leitura(void *disp, int id, int *pvalor);


#endif  // NUM_RAND_H