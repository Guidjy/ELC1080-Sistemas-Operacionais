// so.c
// sistema operacional
// simulador de computador
// so25b

// ---------------------------------------------------------------------
// INCLUDES {{{1
// ---------------------------------------------------------------------

#include "so.h"
#include "dispositivos.h"
#include "dispositivos.h"
#include "err.h"
#include "irq.h"
#include "memoria.h"
#include "programa.h"
#include "fila.h"
#include "metricas.h"

#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>


// ---------------------------------------------------------------------
// CONSTANTES E TIPOS {{{1
// ---------------------------------------------------------------------

// intervalo entre interrupções do relógio
#define INTERVALO_INTERRUPCAO 50   // em instruções executadas
#define QUANTUM 10

#define N_PROCESSOS 5   // número máximo de processos
#define SEM_PROCESSO -1  // indica que não tem um processo corrente

#define SEM_DISPOSITIVO -1;  // indica que não tem um dispositivo que causou bloqueio
#define N_TERMINAIS 4

#define ESCALONADOR 2
#define SEM_ESCALONADOR 0
#define ROUND_ROBIN 1
#define PRIORIDADE 2


enum estado_t {
  PRONTO,
  EXECUCAO,
  ESPERA,
  BLOQUEADO,
  FINALIZADO,
};
typedef enum estado_t estado_t;


struct processo_t {
  int pid;
  int regPC;
  int regA;
  int regX;
  int regERRO;

  int terminal;
  estado_t estado;
  char *executavel;

  int dispositivo_causou_bloqueio;
  int pid_esperando;  // pid do processo que está esperando morrer

  int quantum;
  float prioridade;
};


struct so_t {
  cpu_t *cpu;
  mem_t *mem;
  es_t *es;
  console_t *console;
  bool erro_interno;

  int regA, regX, regPC, regERRO; // cópia do estado da CPU

  // t2: tabela de processos, processo corrente, pendências, etc
  processo_t *tabela_de_processos;
  int n_processos_tabela;
  processo_t *processo_corrente;  // se pid == SEM_PROCESSO, não tem processo
  Fila *processos_prontos;

  // vetor com os pids dos processos que estão usando cada terminal (0 == TERM_A, 1 == TERM_B...)
  int terminais_usados[4];
};


// função de tratamento de interrupção (entrada no SO)
static int so_trata_interrupcao(void *argC, int reg_A);

// funções auxiliares
// carrega o programa contido no arquivo na memória do processador; retorna end. inicial
static int so_carrega_programa(so_t *self, char *nome_do_executavel);
// copia para str da memória do processador, até copiar um 0 (retorna true) ou tam bytes
static bool copia_str_da_mem(int tam, char str[tam], mem_t *mem, int ender);
// imprime a tabela de processos
static void tablea_proc_imprime(so_t *self)
{
  for (int i = 0; i < N_PROCESSOS; i++)
  {
    int pid = self->tabela_de_processos[i].pid;
    int regA = self->tabela_de_processos[i].regA;
    int regX = self->tabela_de_processos[i].regX;
    //int regPC = self->tabela_de_processos[i].regPC;
    char *exe = self->tabela_de_processos[i].executavel;
    int estado = self->tabela_de_processos[i].estado;
    int terminal = self->tabela_de_processos[i].terminal;
    console_printf("pid: %d || regA: %d || regX: %d || EXE: %s || t: %d || estado: %d", pid, regA, regX, exe, terminal, estado);
  }
}


// ---------------------------------------------------------------------
// Funções de processos
// ---------------------------------------------------------------------


static bool associa_terminal_a_processo(so_t *so, processo_t *proc)
{
  for (int i = 0; i < N_TERMINAIS; i++)
  {
    if (so->terminais_usados[i] == SEM_PROCESSO)
    {
      so->terminais_usados[i] = proc->pid;
      switch (i)
      {
        case 0:
          proc->terminal = D_TERM_A;
          break;
        case 1:
          proc->terminal = D_TERM_B;
          break;
        case 2:
          proc->terminal = D_TERM_C;
          break;
        default:
          proc->terminal = D_TERM_D;
      }
      return true;
    }
  }
  return false;  // não tem um terminal disponível
}


int processo_cria(so_t *so, char *nome_do_executavel)
{
  if (so->n_processos_tabela == N_PROCESSOS)
  {
    console_printf("TABELA DE PROCESSOS ESTÁ CHEIA\n");
  }

  // insere um novo processo na tabela
  int i = 0;
  while (i < N_PROCESSOS)
  {
    if (so->tabela_de_processos[i].pid == SEM_PROCESSO)
    {
      so->tabela_de_processos[i].pid = i + 1;
      so->tabela_de_processos[i].executavel = malloc(sizeof(nome_do_executavel));
      strcpy(so->tabela_de_processos[i].executavel, nome_do_executavel);
      so->tabela_de_processos[i].estado = PRONTO;
      so->tabela_de_processos[i].dispositivo_causou_bloqueio = SEM_DISPOSITIVO;
      so->tabela_de_processos[i].pid_esperando = SEM_PROCESSO;
      so->tabela_de_processos[i].quantum = QUANTUM;
      so->tabela_de_processos[i].prioridade = 0.5;

      // metricas
      metricas.processos_pid[i] = i + 1;
      metricas.processos_estado[i] = PRONTO;
      metricas.processos_recem_criado[i] = true;

      break;
    }
    i++;
  }

  // carrega o programa na memória
  int endereco_inicial = so_carrega_programa(so, nome_do_executavel);
  so->tabela_de_processos[i].regPC = endereco_inicial;

  // verifica se o endereço é válido
  if (endereco_inicial < 0) {
    console_printf("SO: problema na carga de um programa");
    so->erro_interno = true;
    return -1;
  }

  // vê se tem um terminal disponível e associa ao processo
  if (!associa_terminal_a_processo(so, &so->tabela_de_processos[i]))
  {
    so->tabela_de_processos[i].terminal = -1;
    console_printf("TERMINAL NÃO ASSOCIADO");
  }

  // insere na fila de processo prontos
  fila_enque(so->processos_prontos, so->tabela_de_processos[i].pid);

  // imprime tabela para debugar
  console_printf("Processo criado\n");
  //tablea_proc_imprime(so);

  metricas.n_processos_criados++;
  so->n_processos_tabela++;
  return so->tabela_de_processos[i].pid;
}


void processo_mata(so_t *so, int pid)
{
  // verifica se hà processos para serem deletados
  if (so->n_processos_tabela <= 0)
  {
    console_printf("SO TENTOU MATAR UM PROCESSO QUANDO NÃO HÁ PROCESSOS CORRENTES\n");
    return;
  }

  so->n_processos_tabela--;

  if (pid == 0)
  {
    // mata o processo corrente
    free(so->processo_corrente->executavel);
    so->processo_corrente->estado = FINALIZADO;
    so->processo_corrente->pid = SEM_PROCESSO;
    so->processo_corrente->terminal = -1;
    for (int i = 0; i < N_TERMINAIS; i++)
    {
      if (so->terminais_usados[i] == so->processo_corrente->pid)
      {
        so->terminais_usados[i] = SEM_PROCESSO;
      }
    }
    fila_deque(so->processos_prontos);
  }
  else
  {
    for (int i = 0; i < N_PROCESSOS; i++)
    {
      if (so->tabela_de_processos[i].pid == pid)
      {
        free(so->tabela_de_processos[i].executavel);
        so->tabela_de_processos[i].estado = FINALIZADO;
        so->tabela_de_processos[i].pid = SEM_PROCESSO;
        so->tabela_de_processos[i].terminal = -1;
        for (int j = 0; j < N_TERMINAIS; j++)
        {
          if (so->terminais_usados[j] == so->tabela_de_processos[i].pid)
          {
            so->terminais_usados[j] = SEM_PROCESSO;
          }
        }
      }

    }
  }

  // verifica se tinha algum processo esperado a morte desse
  for (int i = 0; i < N_PROCESSOS; i++)
  {
    if (so->tabela_de_processos[i].pid_esperando == pid)
    {
      so->tabela_de_processos[i].estado = PRONTO;
      fila_enque(so->processos_prontos, so->tabela_de_processos[i].pid);
    }
  }
}


void processo_troca_corrente(so_t *self)
{
  // por enquanto, acha o primeiro processo existente na tabela
  int i = 0;
  while (i < N_PROCESSOS)
  {
    if (self->tabela_de_processos[i].pid != SEM_PROCESSO && self->tabela_de_processos[i].estado == PRONTO)
    {
      self->processo_corrente = &self->tabela_de_processos[i];
      self->processo_corrente->estado = EXECUCAO;
      break;
    }
    i++;
  }
}


bool todos_processos_encerrados(so_t *self)
{
  for (int i = 0; i < N_PROCESSOS; i++)
  {
    if (self->tabela_de_processos[i].pid != SEM_PROCESSO) return false;
  }

  return true;
}


// verifica se um processo com o pid existe
static bool processo_existe(so_t *self, int pid)
{
  for (int i = 0; i < N_PROCESSOS; i++)
  {
    if (self->tabela_de_processos[i].pid == pid) return true;
  }
  return false;
}


// atualiza a prioridade de um processo
static void processo_atualiza_prioridade(processo_t *proc)
{
  // prio = (prio + t_exec/t_quantum) / 2
  proc->prioridade = (proc->prioridade + proc->quantum / QUANTUM) / 2;
}


// ---------------------------------------------------------------------
// CRIAÇÃO {{{1
// ---------------------------------------------------------------------

so_t *so_cria(cpu_t *cpu, mem_t *mem, es_t *es, console_t *console)
{
  so_t *self = malloc(sizeof(*self));
  if (self == NULL) return NULL;

  self->cpu = cpu;
  self->mem = mem;
  self->es = es;
  self->console = console;
  self->erro_interno = false;

  // cria tabela de processo
  self->tabela_de_processos = malloc(N_PROCESSOS * sizeof(processo_t));
  assert(self->tabela_de_processos != NULL);
  for (int i = 0; i < N_PROCESSOS; i++) 
  {
    self->tabela_de_processos[i].pid = SEM_PROCESSO;
  }
  self->n_processos_tabela = 0;
  self->processo_corrente = &self->tabela_de_processos[0];
  self->processos_prontos = fila_cria();

  // inicializa vetor de terminais usados
  for (int i = 0; i < N_TERMINAIS; i++)
  {
    self->terminais_usados[i] = SEM_PROCESSO;
  }

  // quando a CPU executar uma instrução CHAMAC, deve chamar a função
  //   so_trata_interrupcao, com primeiro argumento um ptr para o SO
  cpu_define_chamaC(self->cpu, so_trata_interrupcao, self);

  return self;
}

void so_destroi(so_t *self)
{
  cpu_define_chamaC(self->cpu, NULL, NULL);
  free(self);
}


// ---------------------------------------------------------------------
// TRATAMENTO DE INTERRUPÇÃO {{{1
// ---------------------------------------------------------------------

// funções auxiliares para o tratamento de interrupção
static void so_salva_estado_da_cpu(so_t *self);
static void so_trata_irq(so_t *self, int irq);
static void so_trata_pendencias(so_t *self);
static void so_escalona(so_t *self);
static int so_despacha(so_t *self);

// função a ser chamada pela CPU quando executa a instrução CHAMAC, no tratador de
//   interrupção em assembly
// essa é a única forma de entrada no SO depois da inicialização
// na inicialização do SO, a CPU foi programada para chamar esta função para executar
//   a instrução CHAMAC
// a instrução CHAMAC só deve ser executada pelo tratador de interrupção
//
// o primeiro argumento é um ponteiro para o SO, o segundo é a identificação
//   da interrupção
// o valor retornado por esta função é colocado no registrador A, e pode ser
//   testado pelo código que está após o CHAMAC. No tratador de interrupção em
//   assembly esse valor é usado para decidir se a CPU deve retornar da interrupção
//   (e executar o código de usuário) ou executar PARA e ficar suspensa até receber
//   outra interrupção
static int so_trata_interrupcao(void *argC, int reg_A)
{
  so_t *self = argC;
  irq_t irq = reg_A;
  // esse print polui bastante, recomendo tirar quando estiver com mais confiança
  console_printf("SO: recebi IRQ %d (%s)", irq, irq_nome(irq));
  // salva o estado da cpu no descritor do processo que foi interrompido
  so_salva_estado_da_cpu(self);
  // faz o atendimento da interrupção
  so_trata_irq(self, irq);
  // faz o processamento independente da interrupção
  so_trata_pendencias(self);
  // escolhe o próximo processo a executar
  so_escalona(self);
  // recupera o estado do processo escolhido
  return so_despacha(self);
}

static void so_salva_estado_da_cpu(so_t *self)
{
  // t2: salva os registradores que compõem o estado da cpu no descritor do
  //   processo corrente. os valores dos registradores foram colocados pela
  //   CPU na memória, nos endereços CPU_END_PC etc. O registrador X foi salvo
  //   pelo tratador de interrupção (ver trata_irq.asm) no endereço 59
  // se não houver processo corrente, não faz nada

  // verifica se há um processo corrente
  if (self->processo_corrente->pid == SEM_PROCESSO)
  {
    return;
  }

  // pega os valores dos registradores da memória
  if (mem_le(self->mem, CPU_END_A, &self->regA) != ERR_OK
      || mem_le(self->mem, CPU_END_PC, &self->regPC) != ERR_OK
      || mem_le(self->mem, CPU_END_erro, &self->regERRO) != ERR_OK
      || mem_le(self->mem, 59, &self->regX)) {
    console_printf("SO: erro na leitura dos registradores");
    self->erro_interno = true;
  }

  // salva os registradores que compõem o estado da cpu no descritor do processo corrente
  self->processo_corrente->regA = self->regA;
  self->processo_corrente->regPC = self->regPC;
  self->processo_corrente->regERRO = self->regERRO;
  self->processo_corrente->regX = self->regX;

  // console_printf("salva estado - corrente - %d, %d, %d, %d", self->processo_corrente->regA, self->processo_corrente->regPC, self->processo_corrente->regERRO, self->processo_corrente->regX);
  // console_printf("salva estado - so       - %d, %d, %d, %d", self->regA, self->regPC, self->regERRO, self->regX);
}

static void so_trata_pendencias(so_t *self)
{
  // t2: realiza ações que não são diretamente ligadas com a interrupção que
  //   está sendo atendida:
  // - E/S pendente
  // - desbloqueio de processos
  // - contabilidades
  // - etc

  // "na função que trata de pendências, o SO deve verificar o estado dos dispositivos 
  // que causaram bloqueio e realizar operações pendentes e desbloquear processos se for o caso"

  // verifica o estado dos dispositivos bloqueados
  for (int i = 0; i < N_PROCESSOS; i++)
  {
    processo_t *p = &self->tabela_de_processos[i];
    if (p->estado == BLOQUEADO)
    {
      // verifica o dispositivo que causou o bloqueio
      int dispositivo = p->dispositivo_causou_bloqueio;
      int operacao = dispositivo % 4;

      // verifica o estado do dispositivo
      int disponivel;
      int estado = es_le(self->es, dispositivo, &disponivel);
      if (estado != ERR_OK)
      {
        // realiza a operação pendente
        // leitura
        if (operacao == 1)
        {
          int dado;
          if (es_le(self->es, dispositivo-1, &dado) != ERR_OK) {
            console_printf("SO: problema no acesso ao teclado");
            self->erro_interno = true;
            return;
          }
          p->regA = dado;

          metricas.n_irq_teclado++;
        }
        // escrita
        if (operacao == 3)
        {
          int dado = p->regX;
          if (es_escreve(self->es, dispositivo-1, dado) != ERR_OK) {
            console_printf("SO: problema no acesso à tela");
            self->erro_interno = true;
            return;
          }
          p->regA = 0;

          metricas.n_irq_tela++;
        }
      }

      // desbloqueia o processo
      p->estado = PRONTO;
      p->dispositivo_causou_bloqueio = SEM_DISPOSITIVO;
      fila_enque(self->processos_prontos, p->pid);

    }
  }

}

static void so_escalona(so_t *self)
{
  // escolhe o próximo processo a executar, que passa a ser o processo
  //   corrente; pode continuar sendo o mesmo de antes ou não
  // t2: na primeira versão, escolhe um processo pronto caso o processo
  //   corrente não possa continuar executando, senão deixa o mesmo processo.
  //   depois, implementa um escalonador melhor

  // verifica se o processo corrente está em execução
  if (self->processo_corrente->estado == EXECUCAO) return;

  switch (ESCALONADOR)
  {
    case ROUND_ROBIN:
      // pega o primeiro processo da fila de processos prontos
      int pid_escalonado = fila_get(self->processos_prontos, 0);
      for (int i = 0; i < N_PROCESSOS; i++)
      {
        if (self->tabela_de_processos[i].pid == pid_escalonado && pid_escalonado != -1)
        {
          // torna-o o processo corrente
          self->processo_corrente = &self->tabela_de_processos[i];
          metricas.n_preempcoes++;
        }
      }
      break;

    case PRIORIDADE:
      // pega o indice do processo com a maior prioridade na tabela de processos (menor valor do campo ->prioridade)
      int indice_maior_prioridade = SEM_PROCESSO;
      float maior_prioridade = QUANTUM;
      for (int i = 0; i < N_PROCESSOS; i++)
      {
        if (self->tabela_de_processos[i].estado == FINALIZADO) continue;

        if (self->tabela_de_processos[i].prioridade < maior_prioridade && self->tabela_de_processos[i].estado == PRONTO)
        {
          indice_maior_prioridade = i;
          maior_prioridade = self->tabela_de_processos[i].prioridade;
        }
      }
      // escalona o processo de maior prioridade
      if (indice_maior_prioridade != SEM_PROCESSO)
      {
        self->processo_corrente = &self->tabela_de_processos[indice_maior_prioridade];
        metricas.n_preempcoes++;
      }
      else
      {
        // apenas tem um processo na tabela - deixa ele corrente
        processo_troca_corrente(self);
      }
      break;

    default:
      console_printf("NENHUM\n");
      // bota o primeiro processo PRONTO para executar
      processo_troca_corrente(self);
      metricas.n_preempcoes++;
  }

  // (métricas) verifica se o so está oscioso
  int n_proc_bloqueados = 0;
  for (int i = 0; i < N_PROCESSOS; i++)
  {
    if (self->tabela_de_processos[i].pid !=- SEM_PROCESSO && self->tabela_de_processos[i].estado == BLOQUEADO)
    {
      n_proc_bloqueados++;
    }
  }
  if (n_proc_bloqueados == self->n_processos_tabela)
  {
    metricas.so_oscioso = true;
  }
  else
  {
    metricas.so_oscioso = false;
  }

  // imprime tabela para debugar
  console_printf("Processo escalonado\n");
  tablea_proc_imprime(self);

  // verifica se todos os processos encerraram
  if (todos_processos_encerrados(self))
  {
    console_printf("TODOS PROCESSOS ENCERRARAM - %d\n", metricas.n_processos_criados);
    metricas_imprime();
  }
}

static int so_despacha(so_t *self)
{
  // t2: se houver processo corrente, coloca o estado desse processo onde ele
  //   será recuperado pela CPU (em CPU_END_PC etc e 59) e retorna 0,
  //   senão retorna 1
  // o valor retornado será o valor de retorno de CHAMAC, e será colocado no 
  //   registrador A para o tratador de interrupção (ver trata_irq.asm).

  // verifica se há processo corrente
  if (self->processo_corrente->pid == SEM_PROCESSO)
  {
    return 1;
  }

  //console_printf("despacha estado - corrente - %d, %d, %d, %d", self->processo_corrente->regA, self->processo_corrente->regPC, self->processo_corrente->regERRO, self->processo_corrente->regX);
  //console_printf("despacha estado - so       - %d, %d, %d, %d", self->regA, self->regPC, self->regERRO, self->regX);

  if (mem_escreve(self->mem, CPU_END_A, self->processo_corrente->regA) != ERR_OK
      || mem_escreve(self->mem, CPU_END_PC, self->processo_corrente->regPC) != ERR_OK
      || mem_escreve(self->mem, CPU_END_erro, self->processo_corrente->regERRO) != ERR_OK
      || mem_escreve(self->mem, 59, self->processo_corrente->regX)) {
    console_printf("SO: erro na escrita dos registradores");
    self->erro_interno = true;
  }
  if (self->erro_interno) return 1;
  else return 0;
}


// ---------------------------------------------------------------------
// TRATAMENTO DE UMA IRQ {{{1
// ---------------------------------------------------------------------

// funções auxiliares para tratar cada tipo de interrupção
static void so_trata_reset(so_t *self);
static void so_trata_irq_chamada_sistema(so_t *self);
static void so_trata_irq_err_cpu(so_t *self);
static void so_trata_irq_relogio(so_t *self);
static void so_trata_irq_desconhecida(so_t *self, int irq);

static void so_trata_irq(so_t *self, int irq)
{
  // verifica o tipo de interrupção que está acontecendo, e atende de acordo
  switch (irq) {
    case IRQ_RESET:
      metricas.n_irq_reset++;
      so_trata_reset(self);
      break;
    case IRQ_SISTEMA:
      metricas.n_irq_sistema++;
      so_trata_irq_chamada_sistema(self);
      break;
    case IRQ_ERR_CPU:
      metricas.n_irq_err_cpu++;
      so_trata_irq_err_cpu(self);
      break;
    case IRQ_RELOGIO:
      metricas.n_irq_relogio++;
      so_trata_irq_relogio(self);
      break;
    default:
    metricas.n_irq_desconhecida++;
      so_trata_irq_desconhecida(self, irq);
  }
}

// chamada uma única vez, quando a CPU inicializa
static void so_trata_reset(so_t *self)
{
  // coloca o tratador de interrupção na memória
  // quando a CPU aceita uma interrupção, passa para modo supervisor,
  //   salva seu estado à partir do endereço CPU_END_PC, e desvia para o
  //   endereço CPU_END_TRATADOR
  // colocamos no endereço CPU_END_TRATADOR o programa de tratamento
  //   de interrupção (escrito em asm). esse programa deve conter a
  //   instrução CHAMAC, que vai chamar so_trata_interrupcao (como
  //   foi definido na inicialização do SO)
  int ender = so_carrega_programa(self, "trata_int.maq");
  if (ender != CPU_END_TRATADOR) {
    console_printf("SO: problema na carga do programa de tratamento de interrupção");
    self->erro_interno = true;
  }

  // programa o relógio para gerar uma interrupção após INTERVALO_INTERRUPCAO
  if (es_escreve(self->es, D_RELOGIO_TIMER, INTERVALO_INTERRUPCAO) != ERR_OK) {
    console_printf("SO: problema na programação do timer");
    self->erro_interno = true;
  }

  // t2: deveria criar um processo para o init, e inicializar o estado do
  //   processador para esse processo com os registradores zerados, exceto
  //   o PC e o modo.
  // como não tem suporte a processos, está carregando os valores dos
  //   registradores diretamente no estado da CPU mantido pelo SO; daí vai
  //   copiar para o início da memória pelo despachante, de onde a CPU vai
  //   carregar para os seus registradores quando executar a instrução RETI
  //   em bios.asm (que é onde está a instrução CHAMAC que causou a execução
  //   deste código

  // coloca o programa init na memória
  int pid = processo_cria(self, "init.maq");
  processo_troca_corrente(self);
  console_printf("TROCOU PRO INIT");
  self->processo_corrente->estado = EXECUCAO;
  self->processo_corrente->regA = pid;
  /*
  if (ender != 100) {
    console_printf("SO: problema na carga do programa inicial");
    self->erro_interno = true;
    return;
  }
  */
}

// interrupção gerada quando a CPU identifica um erro
static void so_trata_irq_err_cpu(so_t *self)
{
  // Ocorreu um erro interno na CPU
  // O erro está codificado em CPU_END_erro
  // Em geral, causa a morte do processo que causou o erro
  // Ainda não temos processos, causa a parada da CPU
  // t2: com suporte a processos, deveria pegar o valor do registrador erro
  //   no descritor do processo corrente, e reagir de acordo com esse erro
  //   (em geral, matando o processo)
  err_t err = self->processo_corrente->regERRO;
  processo_mata(self, 0);
  console_printf("SO: IRQ não tratada -- erro na CPU: %s", err_nome(err));
  self->erro_interno = true;
}

// interrupção gerada quando o timer expira
static void so_trata_irq_relogio(so_t *self)
{
  // rearma o interruptor do relógio e reinicializa o timer para a próxima interrupção
  err_t e1, e2;
  e1 = es_escreve(self->es, D_RELOGIO_INTERRUPCAO, 0); // desliga o sinalizador de interrupção
  e2 = es_escreve(self->es, D_RELOGIO_TIMER, INTERVALO_INTERRUPCAO);
  if (e1 != ERR_OK || e2 != ERR_OK) {
    console_printf("SO: problema da reinicialização do timer");
    self->erro_interno = true;
  }
  // t2: deveria tratar a interrupção
  //   por exemplo, decrementa o quantum do processo corrente, quando se tem
  //   um escalonador com quantum
  //console_printf("SO: interrupção do relógio (não tratada)");
  self->processo_corrente->quantum--;
  if (self->processo_corrente->quantum <= 0 && self->processo_corrente->estado != BLOQUEADO)
  {
    self->processo_corrente->quantum = 10;
    processo_atualiza_prioridade(self->processo_corrente);
    fila_deque(self->processos_prontos);
    // ATENÇÃO: talvez não funcione sapoha
    fila_enque(self->processos_prontos, self->processo_corrente->pid);
  }
}

// foi gerada uma interrupção para a qual o SO não está preparado
static void so_trata_irq_desconhecida(so_t *self, int irq)
{
  console_printf("SO: não sei tratar IRQ %d (%s)", irq, irq_nome(irq));
  self->erro_interno = true;
}


// ---------------------------------------------------------------------
// CHAMADAS DE SISTEMA {{{1
// ---------------------------------------------------------------------

// funções auxiliares para cada chamada de sistema
static void so_chamada_le(so_t *self);
static void so_chamada_escr(so_t *self);
static void so_chamada_cria_proc(so_t *self);
static void so_chamada_mata_proc(so_t *self);
static void so_chamada_espera_proc(so_t *self);

static void so_trata_irq_chamada_sistema(so_t *self)
{
  // a identificação da chamada está no registrador A
  // t2: com processos, o reg A deve estar no descritor do processo corrente
  // int id_chamada = self->regA;  antigo
  int id_chamada = self->processo_corrente->regA;
  console_printf("SO: chamada de sistema %d", id_chamada);
  switch (id_chamada) {
    case SO_LE:
      so_chamada_le(self);
      break;
    case SO_ESCR:
      so_chamada_escr(self);
      break;
    case SO_CRIA_PROC:
      so_chamada_cria_proc(self);
      break;
    case SO_MATA_PROC:
      so_chamada_mata_proc(self);
      break;
    case SO_ESPERA_PROC:
      so_chamada_espera_proc(self);
      break;
    default:
      console_printf("SO: chamada de sistema desconhecida (%d)", id_chamada);
      // t2: deveria matar o processo
      processo_mata(self, 0);
      self->erro_interno = true;
  }
}

// implementação da chamada se sistema SO_LE
// faz a leitura de um dado da entrada corrente do processo, coloca o dado no reg A
static void so_chamada_le(so_t *self)
{
  // implementação com espera ocupada
  //   t2: deveria realizar a leitura somente se a entrada estiver disponível,
  //     senão, deveria bloquear o processo.
  //   no caso de bloqueio do processo, a leitura (e desbloqueio) deverá
  //     ser feita mais tarde, em tratamentos pendentes em outra interrupção,
  //     ou diretamente em uma interrupção específica do dispositivo, se for
  //     o caso
  // implementação lendo direto do terminal A
  //   t2: deveria usar dispositivo de entrada corrente do processo
  for (;;) {  // espera ocupada!
    int estado;
    int terminal = self->processo_corrente->terminal + 1;
    if (es_le(self->es, terminal, &estado) != ERR_OK) {
      console_printf("SO: problema no acesso ao estado do teclado");
      // bloqueia o processo
      self->processo_corrente->estado = BLOQUEADO;
      processo_atualiza_prioridade(self->processo_corrente);
      fila_deque(self->processos_prontos);
      self->processo_corrente->dispositivo_causou_bloqueio = terminal;
      self->erro_interno = true;
      return;
    }
    if (estado != 0) break;
    // como não está saindo do SO, a unidade de controle não está executando seu laço.
    // esta gambiarra faz pelo menos a console ser atualizada
    // t2: com a implementação de bloqueio de processo, esta gambiarra não
    //   deve mais existir.
    console_tictac(self->console);
  }
  int dado;
  int terminal = self->processo_corrente->terminal + 0;
  if (es_le(self->es, terminal, &dado) != ERR_OK) {
    console_printf("SO: problema no acesso ao teclado");
    self->erro_interno = true;
    return;
  }
  // escreve no reg A do processador
  // (na verdade, na posição onde o processador vai pegar o A quando retornar da int)
  // t2: se houvesse processo, deveria escrever no reg A do processo
  // t2: o acesso só deve ser feito nesse momento se for possível; se não, o processo
  //   é bloqueado, e o acesso só deve ser feito mais tarde (e o processo desbloqueado)
  self->regA = dado;
  self->processo_corrente->regA = dado;
}

// implementação da chamada se sistema SO_ESCR
// escreve o valor do reg X na saída corrente do processo
static void so_chamada_escr(so_t *self)
{
  // implementação com espera ocupada
  //   t2: deveria bloquear o processo se dispositivo ocupado
  // implementação escrevendo direto do terminal A
  //   t2: deveria usar o dispositivo de saída corrente do processo
  for (;;) {
    int estado;

    // usa um terminal diferente dependendo do pid do processo
    int terminal = self->processo_corrente->terminal + 3;

    if (es_le(self->es, terminal, &estado) != ERR_OK) {
      console_printf("SO: problema no acesso ao estado da tela");
      // bloqueia o processo
      self->processo_corrente->estado = BLOQUEADO;
      processo_atualiza_prioridade(self->processo_corrente);
      fila_deque(self->processos_prontos);
      self->processo_corrente->dispositivo_causou_bloqueio = terminal;
      self->erro_interno = true;
      return;
    }
    if (estado != 0) break;
    // como não está saindo do SO, a unidade de controle não está executando seu laço.
    // esta gambiarra faz pelo menos a console ser atualizada
    // t2: não deve mais existir quando houver suporte a processos, porque o SO não poderá
    //   executar por muito tempo, permitindo a execução do laço da unidade de controle
    console_tictac(self->console);
  }
  int dado;
  // está lendo o valor de X e escrevendo o de A direto onde o processador colocou/vai pegar
  // t2: deveria usar os registradores do processo que está realizando a E/S
  // t2: caso o processo tenha sido bloqueado, esse acesso deve ser realizado em outra execução
  //   do SO, quando ele verificar que esse acesso já pode ser feito.
  dado = self->processo_corrente->regX;
  int terminal = self->processo_corrente->terminal + 2;
  if (es_escreve(self->es, terminal, dado) != ERR_OK) {
    console_printf("SO: problema no acesso à tela");
    self->erro_interno = true;
    return;
  }
  self->regA = 0;
  self->processo_corrente->regA = 0;
}

// implementação da chamada se sistema SO_CRIA_PROC
// cria um processo
static void so_chamada_cria_proc(so_t *self)
{
  // ainda sem suporte a processos, carrega programa e passa a executar ele
  // quem chamou o sistema não vai mais ser executado, coitado!
  // t2: deveria criar um novo processo

  // em X está o endereço onde está o nome do arquivo
  int ender_proc;
  // t2: deveria ler o X do descritor do processo criador
  ender_proc = self->processo_corrente->regX;
  console_printf("proc corrente: %d", self->processo_corrente->pid);
  char nome[100];
  int pid;
  if (copia_str_da_mem(100, nome, self->mem, ender_proc)) {
    pid = processo_cria(self, nome);

    /*
    if (ender_carga > 0) {
      // t2: deveria escrever no PC do descritor do processo criado
      // isso está sendo feito em processo_cria
      return;
    } // else?
    */
  }
  // deveria escrever -1 (se erro) ou o PID do processo criado (se OK) no reg A
  //   do processo que pediu a criação
  self->processo_corrente->regA = pid;
  self->regA = pid;
}

// implementação da chamada se sistema SO_MATA_PROC
// mata o processo com pid X (ou o processo corrente se X é 0)
static void so_chamada_mata_proc(so_t *self)
{
  // t2: deveria matar um processo
  // ainda sem suporte a processos, retorna erro -1
  // console_printf("SO: SO_MATA_PROC não implementada");
  // self->regA = -1;
  // self->processo_corrente->regA = -1;
  processo_mata(self, self->processo_corrente->regX);  // mata o processo corrente
}

// implementação da chamada se sistema SO_ESPERA_PROC
// espera o fim do processo com pid X
static void so_chamada_espera_proc(so_t *self)
{
  // t2: deveria bloquear o processo se for o caso (e desbloquear na morte do esperado)
  // ainda sem suporte a processos, retorna erro -1
  /*
  console_printf("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-");
  console_printf("SO: SO_ESPERA_PROC não implementada - pid: %d", self->processo_corrente->regX);
  console_printf("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-");
  self->regA = -1;
  */

  // verifica se o processo a se esperar é válido
  if (self->processo_corrente->regX == self->processo_corrente->pid || 
    !processo_existe(self, self->processo_corrente->regX))
  {
    console_printf("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-");
    console_printf("PROCESSO INVÁLIDO");
    console_printf("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-");
    self->erro_interno = true;
    return;
  }

  console_printf("[%d] vai esperar o fim de [%d]", self->processo_corrente->pid, self->processo_corrente->regX);

  // bloqueia o processo chamador
  self->processo_corrente->estado = BLOQUEADO;
  processo_atualiza_prioridade(self->processo_corrente);
  fila_deque(self->processos_prontos);
  self->processo_corrente->pid_esperando = self->processo_corrente->regX;
}


// ---------------------------------------------------------------------
// CARGA DE PROGRAMA {{{1
// ---------------------------------------------------------------------

// carrega o programa na memória
// retorna o endereço de carga ou -1
static int so_carrega_programa(so_t *self, char *nome_do_executavel)
{
  // programa para executar na nossa CPU
  programa_t *prog = prog_cria(nome_do_executavel);
  if (prog == NULL) {
    console_printf("Erro na leitura do programa '%s'\n", nome_do_executavel);
    return -1;
  }

  int end_ini = prog_end_carga(prog);
  int end_fim = end_ini + prog_tamanho(prog);

  for (int end = end_ini; end < end_fim; end++) {
    if (mem_escreve(self->mem, end, prog_dado(prog, end)) != ERR_OK) {
      console_printf("Erro na carga da memória, endereco %d\n", end);
      return -1;
    }
  }

  prog_destroi(prog);
  console_printf("SO: carga de '%s' em %d-%d", nome_do_executavel, end_ini, end_fim);
  return end_ini;
}


// ---------------------------------------------------------------------
// ACESSO À MEMÓRIA DOS PROCESSOS {{{1
// ---------------------------------------------------------------------

// copia uma string da memória do simulador para o vetor str.
// retorna false se erro (string maior que vetor, valor não char na memória,
//   erro de acesso à memória)
// t2: deveria verificar se a memória pertence ao processo
static bool copia_str_da_mem(int tam, char str[tam], mem_t *mem, int ender)
{
  for (int indice_str = 0; indice_str < tam; indice_str++) {
    int caractere;
    if (mem_le(mem, ender + indice_str, &caractere) != ERR_OK) {
      return false;
    }
    if (caractere < 0 || caractere > 255) {
      return false;
    }
    str[indice_str] = caractere;
    if (caractere == 0) {
      return true;
    }
  }
  // estourou o tamanho de str
  return false;
}

// vim: foldmethod=marker
