; implemente um programa que imprime o número de instruções executadas e os segundos de execução, durante um certo tempo.


; dispositivo correspondente à tela do primeiro terminal
tela              DEFINE 2
; dispositivo que contém o número de instruções executadas
n_instrucoes      DEFINE 16
; dispositivo que contém quantos milisegundos passaram edsde o início da simulação
n_milisegundos    DEFINE 17

        ; executa três instrução arbitrárias
        CARGI 3000      ; A = 3000
        SUB mil         ; A -= 1000
        NOP

        ; lê o número de instruções executadas
        LE n_instrucoes
        ; escreve o número de instruções
        ESCR tela

        ; lê o tempo de execução da simulação em milisegundos
        LE n_milisegundos
        ; escreve o número de segundos da simulaçao
        ESCR tela

        ; encerra o programa
fim     PARA


; constantes
mil     valor 1000
                    
