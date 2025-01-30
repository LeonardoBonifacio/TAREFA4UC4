#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "pico/bootrom.h"


// Biblioteca gerada pelo arquivo .pio durante compilação.
#include "ws2818b.pio.h"

// Definição do número de LEDs e pinos.
#define LED_COUNT 25
#define MATRIZ_LED_PIN 7
#define BUTTON_A 5
#define BUTTON_B 6
#define BUTTON_JOYSTICK 22 
#define LED_RED 13
#define LED_GREEN 11
#define LED_BLUE 12
#define DEBOUNCE_DELAY_MS 500  // Tempo de debounce em milissegundos 


// Arrays com index das posições relativas a cada led rgb presente na matriz de leds 
// Partindo do zero(no canto inferior direito) ate 24 (no canto superior esquerdo)
int index_posicoes_zero[]   = {1,2,3,4,5,8,11,14,15,18,21,22,23,24};       // Tamanho == 14
int index_posicoes_um[]     = {2,7,12,14,16,17,22};                        // Tamanho == 7
int index_posicoes_dois[]   = {1,2,3,4,5,11,12,13,14,18,21,22,23,24};      // Tamanho == 14
int index_posicoes_tres[]   = {1,2,3,4,8,11,12,13,14,18,21,22,23,24};      // Tamanho == 14
int index_posicoes_quatro[] = {1,8,11,12,13,14,15,18,21,24} ;              // Tamanho == 10
int index_posicoes_cinco[]  = {1,2,3,4,8,11,12,13,14,15,21,22,23,24};      // Tamanho == 14
int index_posicoes_seis[]   = {1,2,3,4,5,8,11,12,13,14,15,21,22,23,24};    // Tamanho == 15
int index_posicoes_sete[]   = {1,8,11,14,15,18,21,22,23,24};               // Tamanho == 10
int index_posicoes_oito[]   = {1,2,3,4,5,8,11,12,13,14,15,18,21,22,23,24}; // Tamanho == 16
int index_posicoes_nove[]   = {1,8,11,12,13,14,15,18,21,22,23,24};         // Tamanho == 12

// Váriaveis volatile para indicar ao compilador que elas serão alteradas por eventos externos

// Variável usada para saber em quanto está o contador que será mostrado na matriz de leds
// Caso a interrupção seja acionada pelo botão B seja acionada esta variável é decrementada
// E o contrario acontece caso a interrupção seja acionada pelo botão A
static volatile int incrementa_ou_decrementa_led = 0;

static volatile bool atualiza_leds = false;  // Flag para sinalizar atualização da matriz
static volatile uint32_t ultima_alteracao_led_vermelho = 0;  // Para controle de tempo do LED vermelho

volatile uint32_t ultimo_tempo_button_a = 0;  // Para armazenar o tempo da última interrupção acionada pelo bottão A
volatile uint32_t ultimo_tempo_button_b = 0;  // Para armazenar o tempo da última interrupção acionada pelo bottão B


// Definição de pixel GRB
struct pixel_t {
  uint8_t G, R, B; // Três valores de 8-bits compõem um pixel.
};
typedef struct pixel_t pixel_t;
typedef pixel_t npLED_t; // Mudança de nome de "struct pixel_t" para "npLED_t" por clareza.

// Declaração do buffer de pixels que formam a matriz.
npLED_t leds[LED_COUNT];

// Variáveis para uso da máquina PIO.
PIO np_pio;
uint sm;



// Inicializa a máquina PIO para controle da matriz de LEDs.
void npInit(uint pin) {

  // Cria programa PIO.
  uint offset = pio_add_program(pio0, &ws2818b_program);
  np_pio = pio0;

  // Toma posse de uma máquina PIO.
  sm = pio_claim_unused_sm(np_pio, false);
  if (sm < 0) {
    np_pio = pio1;
    sm = pio_claim_unused_sm(np_pio, true); // Se nenhuma máquina estiver livre, panic!
  }

  // Inicia programa na máquina PIO obtida.
  ws2818b_program_init(np_pio, sm, offset, pin, 800000.f);

  // Limpa buffer de pixels.
  for (uint i = 0; i < LED_COUNT; ++i) {
    leds[i].R = 0;
    leds[i].G = 0;
    leds[i].B = 0;
  }
}


// Atribui uma cor RGB a um LED.
void npSetLED(const uint index, const uint8_t r, const uint8_t g, const uint8_t b) {
  leds[index].R = r;
  leds[index].G = g;
  leds[index].B = b;
}

// Limpa o buffer de pixels.
void npClear() {
  for (uint i = 0; i < LED_COUNT; ++i)
    npSetLED(i, 0, 0, 0);
}


// Escreve os dados do buffer nos LEDs.
void npWrite() {
  // Escreve cada dado de 8-bits dos pixels em sequência no buffer da máquina PIO.
  for (uint i = 0; i < LED_COUNT; ++i) {
    pio_sm_put_blocking(np_pio, sm, leds[i].G);
    pio_sm_put_blocking(np_pio, sm, leds[i].R);
    pio_sm_put_blocking(np_pio, sm, leds[i].B);
  }
  sleep_us(100); // Espera 100us, sinal de RESET do datasheet.
}

// Inicializa as gpio referentes aos botões e leds, alem de colocar os 3 botões em pull_up 
void inicializar_leds_e_botoes(){
    gpio_init(BUTTON_A);
    gpio_set_dir(BUTTON_A,GPIO_IN);
    gpio_pull_up(BUTTON_A);

    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_B,GPIO_IN);
    gpio_pull_up(BUTTON_B);

    gpio_init(BUTTON_JOYSTICK);
    gpio_set_dir(BUTTON_JOYSTICK,GPIO_IN);
    gpio_pull_up(BUTTON_JOYSTICK);

    gpio_init(LED_RED);
    gpio_set_dir(LED_RED,GPIO_OUT);
    gpio_put(LED_RED,false);

    gpio_init(LED_GREEN);
    gpio_set_dir(LED_GREEN,GPIO_OUT);
    gpio_put(LED_GREEN,false);

    gpio_init(LED_BLUE);
    gpio_set_dir(LED_BLUE,GPIO_OUT);
    gpio_put(LED_BLUE,false);
}


// Desenha na matriz de leds nas posicoes solicitadas
void desenhar_na_matriz(int posicoes[], int tamanho_posicoes, int red, int green, int blue){
    for (int i = 0; i < tamanho_posicoes; i++)
    {
        npSetLED(posicoes[i],red,green,blue);
    }
    npWrite();
    
}

// Desenha baseado em que número o contador está
void mostra_numero_baseado_no_contador(){
    switch (incrementa_ou_decrementa_led) 
    {
    case 0:
        desenhar_na_matriz(index_posicoes_zero,14,255,255,255);// Branco
        break;
    case 1:
        desenhar_na_matriz(index_posicoes_um,7,255,0,0); // Vermelho
        break;
    case 2:
        desenhar_na_matriz(index_posicoes_dois,14,255,127,0); // Amarelo
        break;
    case 3:
        desenhar_na_matriz(index_posicoes_tres,14,169,169,169); // Cinza
        break;
    case 4:
        desenhar_na_matriz(index_posicoes_quatro,10,0,255,0); // Verde
        break;
    case 5:
        desenhar_na_matriz(index_posicoes_cinco,14,0,0,255); // Azul
        break;
    case 6:
        desenhar_na_matriz(index_posicoes_seis,15,255,140,0); // Laranja
        break;
    case 7:
        desenhar_na_matriz(index_posicoes_sete,10,139,0,255); // Roxo
        break;
    case 8:
        desenhar_na_matriz(index_posicoes_oito,16,139,69,19); // Branco
        break;
    case 9:
        desenhar_na_matriz(index_posicoes_nove,12,255,20,147); // Rosa
        break;
    }
}


// Função que captura a interrupção global e baseada em qual gpio mandou a interrupção ela ativa a lógica correspondente
static void gpio_irq_handler(uint gpio, uint32_t events) {
    if (gpio == BUTTON_A) {
        // Interrupção que trata de aumentar o número mostrado pela matriz de leds
        uint32_t tempo_atual = time_us_32() / 1000;  // Obtém o tempo atual em milissegundos e o armazena

        // Se o tempo passado for menor que o atraso  de debounce(500ms) retorne imediatamente
        if (tempo_atual - ultimo_tempo_button_a < DEBOUNCE_DELAY_MS) return;

        // O tempo atual corresponde ao último tempo que o botão foi pressionado, ja que ele passou pela verificação acima
        ultimo_tempo_button_a = tempo_atual;

        // impede do contador de ficar maior que 9 e aumenta ele para que seja atualizado no loop principal
        if (incrementa_ou_decrementa_led + 1 < 10) incrementa_ou_decrementa_led++;
        
        // Muda o estado da flag para que ocorra a atualização dos leds apos o contador ser incrementado
        atualiza_leds = true;

    } else if (gpio == BUTTON_B) {
        // Interrupção que trata de diminuir o número mostrado pela matriz de leds
        uint32_t tempo_atual = time_us_32() / 1000; // Obtém o tempo atual em milissegundos e o armazena

        // Se o tempo passado for menor que o atraso  de debounce(500ms) retorne imediatamente
        if (tempo_atual - ultimo_tempo_button_b < DEBOUNCE_DELAY_MS) return;

        // O tempo atual corresponde ao último tempo que o botão foi pressionado, ja que ele passou pela verificação acima
        ultimo_tempo_button_b = tempo_atual;

        // impede do contador de ficar menor que 0 e diminui ele para que seja atualizado no loop principal
        if (incrementa_ou_decrementa_led - 1 >= 0) incrementa_ou_decrementa_led--;

        // Muda o estado da flag para que ocorra a atualização dos leds apos o contador ser decrementado
        atualiza_leds = true;

    } else if (gpio == BUTTON_JOYSTICK) {
        // Interrupção para habilitar o modo de gravação do microcontrolador
        reset_usb_boot(0,0);
    }
}


int main() {
    inicializar_leds_e_botoes();
    npInit(MATRIZ_LED_PIN);
    npWrite();

    // Registra interrupções para todos os botões
    gpio_set_irq_enabled_with_callback(BUTTON_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(BUTTON_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(BUTTON_JOYSTICK, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    while (true) {
        uint32_t tempo_atual = time_us_32(); // Captura o tempo desde que o programa foi iniciado em microssegundos

        // Atualiza matriz de LEDs se necessário
        if (atualiza_leds) {
            npClear();// Limpa a matriz de Leds
            mostra_numero_baseado_no_contador();
            atualiza_leds = false;  // Reseta flag
        }

        // Pisca o LED vermelho 5 vezes por segundo
        // Cada ciclo "Custa" 200 ms , 100ms do led ligado e 100ms dele desligado, 
        // totalizando 5 ciclos por segundo ou seja ele pisca 5 vezes por segundo
        // Calcula o tempo que passou desde a ultima vez que o led alterou seu estado e 
        // verifica se pelo menos 100ms se passaram desde a ultima alteração
        if (tempo_atual - ultima_alteracao_led_vermelho >= 100000) {  // 100ms = 100000µs
            gpio_put(LED_RED, !gpio_get(LED_RED));  // Inverte estado
            ultima_alteracao_led_vermelho = tempo_atual;
        }
    }
}

