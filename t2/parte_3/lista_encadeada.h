#ifndef LISTA_ENCADEADA_H
#define LISTA_ENCADEADA_H


#include <stdbool.h>


typedef struct NoLista {
    void *pdado;           // ponteiro para o dado do nó
    struct NoLista *ant;   // ponteiro para o nó anterior da lista
    struct NoLista *prox;  // ponteiro para o próximo nó da lista
} NoLista;

typedef struct Lista {
    int n_elem;    // número de elementos na lista
    int tam_dado;  // tamanho, em bytes, dos dados nos nós da lista
    NoLista *pri;  // ponteiro para o primeiro nó da lista
    NoLista *ult;  // ponteiro para o último nó da lista
} Lista;


// alloca memória para o descritor de uma lista e retorna um ponteiro para ele
Lista *lista_cria(int tam_dado);

// libera a memória de todos os nós e do descritor de uma lista
void lista_destroi(Lista *self);

// insere um elemento no início da lista
void lista_insert(Lista *self, void *pdado);

// insere um elemento no final da lista
void lista_append(Lista *self, void *pdado);

// remove o primeiro elemento da lista e copia o dado removido para um ponteiro
void lista_deque(Lista *self, void *removido);

// remove o último elemento da lista e copia o dado removido para um ponteiro
void lista_pop(Lista *self, void *removido);

// remove um elemento da lista por índice
void lista_remove(Lista *self, void *removido, int pos);

// copia o dado do i-ésimo elemento para um ponteiro
void lista_get(Lista *self, int pos, void *pdado);

// verifica se uma lista está vazia
bool lista_vazia(Lista *self);


#endif