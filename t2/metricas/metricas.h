#ifndef METRICAS_H
#define METRICAS_H


typedef struct metricas {
    int n_processos_criados;
    int tempo_total_execucao;
    int tempo_total_ocioso;

    int n_irq_reset;
    int n_irq_err_cpu;
    int n_irq_sistema;
    int n_irq_teclado;
    int n_irq_tela;

    int n_preempcoes;
    int *processos_pid;
    int *tempo_retorno_processo;
    int *n_prontos;
    int *tempo_pronto;
    int *n_bloqueados;
    int *tempo_bloqueado;
    int *n_execoes;
    int *tempo_execucao;
    int *tempo_medio_resposta;
    
} metricas_t;


// torna uma instância da struct globalmente acessível
extern metricas_t metricas;

#endif  // METRICAS_H