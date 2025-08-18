; dispositivo do número aleatório
num_rand    DEFINE 18
; dispositivo correspondente à tela do primeiro terminal
tela        DEFINE 2

        ; lê um número aleatório
        LE num_rand
        ; imprime no terminal
        ESCR tela

; encerra o programa
fim         PARA