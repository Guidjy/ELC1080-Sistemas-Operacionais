#include "lista_encadeada.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>


// alloca memória para o descritor de uma lista
Lista *lista_cria(int tam_dado) {
    // aloca memória para o descritor de uma lista
    Lista *l = (Lista*) malloc(sizeof(Lista));
    if (l == NULL) {
        printf("ERRO NA ALOCAÇÃO DE MEMÓRIA PARA O DESCRITOR DE UMA LISTA\n");
        return NULL;
    }

    // inicializa os campos do descritor
    l->n_elem = 0;
    l->tam_dado = tam_dado;
    l->pri = NULL;
    l->ult = NULL;

    return l;
}


// libera a memória de todos os nós e do descritor de uma lista
void lista_destroi(Lista *self) {
    // libera os nós da lista
    NoLista *p = self->pri;
    NoLista *temp = NULL;
    while (p != NULL) {
        temp = p;
        p = p->prox;
        free(temp->pdado);
        free(temp);
    }

    // libera o descritor da lista
    free(self);
}


// cria um nó de uma lista e retorna um ponteiro para ele
static NoLista *no_lista_cria(void *pdado, int tam_dado) {
    // aloca memória para a estrutura do nó
    NoLista *n = (NoLista*) malloc(sizeof(NoLista));
    if (n == NULL) {
        printf("ERRO NA ALOCAÇÃO DE MEMÓRIA PARA A ESTRUTURA DE UM NÓ DA LISTA\n");
        return NULL;
    }
    n->ant = NULL;
    n->prox = NULL;

    // aloca memória para o dado da lista e copia o dado para o campo
    n->pdado = malloc(tam_dado);
    if (n->pdado == NULL) {
        printf("ERRO NA ALOCAÇÃO DE MEMÓRIA PARA O DADO DE UM NÓ DA LISTA\n");
        return NULL;
    }
    memmove(n->pdado, pdado, tam_dado);

    return n;
}


// insere um elemento no início da lista
void lista_insert(Lista *self, void *pdado) {
    // cria um novo nó
    NoLista *n = no_lista_cria(pdado, self->tam_dado);
    if (n == NULL) return;

    // insere no início da lista
    n->prox = self->pri;
    if (lista_vazia(self)) {
        self->ult = n;
    } else {
        self->pri->ant = n;
    }
    self->pri = n;

    self->n_elem++;
}


// insere um elemento no final da lista
void lista_append(Lista *self, void *pdado) {
    // cria um novo nó
    NoLista *n = no_lista_cria(pdado, self->tam_dado);
    if (n == NULL) return;

    // insere no final da lista
    if (lista_vazia(self)) {
        self->pri = n;
    } else {
        n->ant = self->ult;
        self->ult->prox = n;
    }
    self->ult = n;

    self->n_elem++;
}


// remove o primeiro elemento da lista e copia o dado removido para um ponteiro
void lista_deque(Lista *self, void *removido) {
    // verifica se a lista está vazia
    if (lista_vazia(self)) {
        printf("LISTA VAZIA. NÃO SE PODE REMOVER ELEMENTOS\n");
    }

    // remove o primeiro nó da lista e copia seu dado
    NoLista *no_removido = self->pri;
    self->pri = self->pri->prox;
    memcpy(removido, no_removido->pdado, self->tam_dado);
    free(no_removido->pdado);
    free(no_removido);

    self->n_elem--;
}


// remove o último elemento da lista e copia o dado removido para um ponteiro
void lista_pop(Lista *self, void *removido) {
    // verifica se a lista está vazia
    if (lista_vazia(self)) {
        printf("LISTA VAZIA. NÃO SE PODE REMOVER ELEMENTOS\n");
    }

    // remove o último nó da lista e copia seu dado
    NoLista *no_removido = self->ult;
    if (self->n_elem > 1) {
        self->ult = self->ult->ant;
    } else {
        self->pri = NULL;
        self->ult = NULL;
    }
    memcpy(removido, no_removido->pdado, self->tam_dado);
    free(no_removido->pdado);
    free(no_removido);

    self->n_elem--;
}


void lista_remove(Lista *self, void *removido, int pos) {
    if (self == NULL || pos < 0 || pos >= self->n_elem) {
        return; // posição inválida
    }

    NoLista *p = self->pri;
    int i = 0;

    // anda até o nó da posição desejada
    while (i < pos) {
        p = p->prox;
        i++;
    }

    // copia o dado removido para o buffer do chamador
    if (removido != NULL) {
        memcpy(removido, p->pdado, self->tam_dado);
    }

    // ajusta os ponteiros da lista
    if (p->ant != NULL) {
        p->ant->prox = p->prox;
    } else {
        // era o primeiro
        self->pri = p->prox;
    }

    if (p->prox != NULL) {
        p->prox->ant = p->ant;
    } else {
        // era o último
        self->ult = p->ant;
    }

    // libera memória do nó e do dado
    free(p->pdado);
    free(p);

    self->n_elem--;
}


// copia o dado do i-ésimo elemento para um ponteiro
void lista_get(Lista *self, int pos, void *pdado) {
    NoLista *p = self->pri;
    int i = 0;
    while (i < pos && p != NULL) {
        p = p->prox;
        i++;
    }
    if (p == NULL) return;
    memcpy(pdado, p->pdado, self->tam_dado);
}


// verifica se uma lista está vazia
bool lista_vazia(Lista *self) {
    return self->n_elem == 0;
}