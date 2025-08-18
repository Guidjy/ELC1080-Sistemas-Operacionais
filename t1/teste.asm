; dispositivo que gera um número aleatório
num_rand        DEFINE 18
; aloca espaço na memória para o número aleatório
segredo         ESPACO 1
; gera um número aleatório
                LE num_rand
; guarda o número na memória
                ARMM segredo
; zera o valor de A para ver se vai funcionar
                CARGI 100
; carrega o número aleatório da memória
                SUB segredo