# include "num_rand.h"

#include <stdlib.h>
#include <assert.h>


#define LIMITE_SUPERIOR 101


struct num_rand_t {
    // contém o número aleatório gerado
    int num;
};


num_rand_t *num_rand_cria() {
    num_rand_t *self;
    self = malloc(sizeof(num_rand_t));
    assert(self != NULL);

    self->num = rand() % LIMITE_SUPERIOR;

    return self;
}


void num_rand_destroi(num_rand_t *self) {
    free(self);
}


err_t num_rand_leitura(void *disp, int id, int *pvalor) {
    num_rand_t *self = disp;
    err_t err = ERR_OK;

    switch(id) {
        case 0:
            *pvalor = self->num;
            self->num = rand() % LIMITE_SUPERIOR;
            break;
        default:
            err = ERR_END_INV;
    }
    
    return err;
}