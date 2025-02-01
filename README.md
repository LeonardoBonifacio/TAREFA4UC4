# TAREFA4U4C4 Utilização da matriz de leds(controlada por interrupções) , led rgb e botoes da placa bitdoglab
Tarefa passada pelo professor wilton na aula sincrona do dia 27/01

Link para video de demonstração ->    https://youtu.be/1B7g6z1wl30?si=bhNDSXdfBHwxH0Z7

### Descrição do Projeto
Este projeto implementa a logica para que a cada 100ms o led rgb mude de estado para ligado ou desligado fazendo com que ele pisque 5 vezes por segundos. Além de que a matriz de leds mostra os números de 0 a 9, sendo incrementado ou decrementado os números atraves dos botões A e B da bitdoglab por interrupções. O contador do código começa em 0 por isso caso aperte o botão A primmerio o primeiro número a ser mostrado é 1 e caso aperte o botão B o primeiro número a ser mostrado é 0.

OBS: Não utilizei toda a matriz de leds para formar os números pois achei que ficariam feios se ficassem maiores do que ja são

### Instruções de uso

Para rodar este código é necessário:

ter o arquivo .c , ter arquivo do cmakelist, e o arquivo ws2818b.pio em uma pasta para que você os possa transformar em um projeto utilizando a extensão da raspberry pi pico




