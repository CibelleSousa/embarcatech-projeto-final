#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "inc/ssd1306.h"
#include "inc/font.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "embarcatech-projeto-final.pio.h"

// Defines para o Display
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C
#define WIDTH 128
#define HEIGHT 64

// Defines para LEDs, PIO, Botões e Joystick
#define BTN_A 5
#define BTN_B 6
#define OUT_PIN 7
#define LED_R 13
#define LED_B 12
#define LED_G 11
#define BTN_JOY 22
#define VRY_PIN 27

// Variáveis globais
ssd1306_t ssd;
static volatile uint32_t last_time = 0;
static volatile float temp_max = 25.0;
static volatile float temp_min = 15.0;
static volatile uint8_t ideal =  20;

// Matriz de LEDs
double matrix [25] = {
    0.25, 0.25, 0.25, 0.25, 0.25,
    0.25, 0.25, 0.25, 0.25, 0.25,
    0.25, 0.25, 0.25, 0.25, 0.25,
    0.25, 0.25, 0.25, 0.25, 0.25,
    0.25, 0.25, 0.25, 0.25, 0.25
};

// Prototipação das funções
void setup();
void setupDisplay();
void turnoff();
void status(float value);
void display_float(float value);
void gpio_irq_handler(uint gpio, uint32_t events);
void update_matrix(float temperature, PIO pio, uint sm);
float adc_to_temperature(uint16_t adc_value);
uint32_t matrix_rgb(double r, double g, double b);

int main()
{
    // Variável para definir os valores lidos no Joystick em relação ao eixo y
    static volatile uint16_t vry_value;

    // Inicializações
    set_sys_clock_khz(128000, false);
    stdio_init_all();
    setup();
    setupDisplay();

    PIO pio = pio0;
    uint offset = pio_add_program(pio, &serial_program);
    uint sm = pio_claim_unused_sm(pio, true);
    serial_program_init(pio, sm, offset, OUT_PIN);

    ssd1306_draw_string(&ssd, "SALA DE OVOS", 5, 10);
    ssd1306_send_data(&ssd);
    sleep_ms(1000);

    while (true) {
        // Leitura do valor do ADC para VRY (Eixo Y do joystick)
        adc_select_input(0); 
        vry_value = adc_read();

        // Cálculo da simulação da temperatura
        float temperature = adc_to_temperature(vry_value);
        
        // Exibição da temperatura no Display
        display_float(temperature);
        
        // Liga o LED correspondente ao status da temperatura simulada
        status(temperature);
        
        // Regula a intensidade da iluminação da Matriz de LEDs conforme a temperatura simulada
        update_matrix(temperature, pio, sm);
        sleep_ms(1000);
    }
}

void setup(){
    // Configura LED GREEN
    gpio_init(LED_G);
    gpio_set_dir(LED_G, GPIO_OUT);

    // Configura LED BLUE
    gpio_init(LED_B);
    gpio_set_dir(LED_B, GPIO_OUT);

    // Configura LED RED
    gpio_init(LED_R);
    gpio_set_dir(LED_R, GPIO_OUT);

    // Configura o ADC
    adc_init();
    adc_gpio_init(VRY_PIN);

    // Configura o Botão A
    gpio_init(BTN_A);
    gpio_set_dir(BTN_A, GPIO_IN);
    gpio_pull_up(BTN_A);

    // Configura o Botão B
    gpio_init(BTN_B);
    gpio_set_dir(BTN_B, GPIO_IN);
    gpio_pull_up(BTN_B);

    // Configura o Botão do Joystick
    gpio_init(BTN_JOY);
    gpio_set_dir(BTN_JOY, GPIO_IN);
    gpio_pull_up(BTN_JOY);

    // Interrupções
    gpio_set_irq_enabled_with_callback(BTN_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(BTN_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(BTN_JOY, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

}

void setupDisplay(){
    // I2C Inicialização. Setando frequência a 400Khz.
    i2c_init(I2C_PORT, 400 * 1000);

    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C); // Seta a GPIO para I2C
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); // Seta a GPIO para I2C
    gpio_pull_up(I2C_SDA); // Pull up a data line
    gpio_pull_up(I2C_SCL); // Pull up a clock line
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); // Inicializa o display
    ssd1306_config(&ssd); // Configura o display
    ssd1306_send_data(&ssd); // Envia os dados para o display

    // Limpa o display. O display inicia com todos os pixels apagados.
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);
}

void turnoff(){
    // Desliga todos os LEDs
    gpio_put(LED_G, false);
    gpio_put(LED_B, false);
    gpio_put(LED_R, false);
}

void status(float value){
    // Liga apenas o LED de acordo com status da temperatura
    turnoff();
    if (value > ideal && value < ideal + 1){
        gpio_put(LED_G, true);
    }
    else if (value < ideal){
        gpio_put(LED_B, true);
    }
    else {
        gpio_put(LED_R, true);
    }
}

void display_float(float value){
    // Imprime temperatura no display
    char buffer[10];
    snprintf(buffer, sizeof(buffer), "%.2fC", value);

    ssd1306_fill(&ssd, false);
    ssd1306_draw_string(&ssd, "Temp-", 19,31);
    ssd1306_draw_string(&ssd, buffer, 62, 31);
    ssd1306_send_data(&ssd);
}

void gpio_irq_handler(uint gpio, uint32_t events){
    // Debouncing 
    uint32_t current_time = to_us_since_boot(get_absolute_time());

    if (current_time - last_time > 200000){
        last_time = current_time;
        ssd1306_fill(&ssd, false);
        // Muda a temperatura simulada de acordo com o botão acionado
        if (gpio == BTN_A){
            temp_max = 30.0;
            temp_min = 20.0;
            ideal = 25;
            ssd1306_draw_string(&ssd, "PRE - INCUBACAO", 5, 10);
        }
        else if (gpio == BTN_B){
            temp_max = 40.0;
            temp_min = 30.0;
            ideal = 37;
            ssd1306_draw_string(&ssd, "INCUBACAO", 5, 10);
        }
        else {
            temp_max = 25.0;
            temp_min = 15.0;
            ideal = 20;
            ssd1306_draw_string(&ssd, "SALA DE OVOS", 5, 10);
        }
        ssd1306_send_data(&ssd);
    }
}

void update_matrix(float temperature, PIO pio, uint sm){
    // Calcula o brilho proporcionalmente à temperatura (quanto mais frio, mais brilho)
    float brightness = (1.0 - ((temperature - temp_min) / (temp_max - temp_min))) / 2;
    if (brightness > 0.5) brightness = 0.5;
    if (brightness < 0.025) brightness = 0.025;

    // Atualiza todos os LEDs da matriz com o novo nível de brilho
    for (int i = 0; i < 25; i++) {
        matrix[i] = brightness;
    }

    // Ascende os LEDs na devida intensidade de brilho
    for (int i = 0; i < 25; i++) {
        uint32_t color = matrix_rgb(matrix[i], matrix[i], matrix[i]);
        pio_sm_put_blocking(pio, sm, color);
    }
}

float adc_to_temperature(uint16_t adc_value) {
    // Rotina que simula uma temperatura a partir de valores lidos no Joystick
    return temp_min + ((adc_value / 4095.0) * (temp_max - temp_min));
}

uint32_t matrix_rgb(double r, double g, double b) {
    unsigned char R, G, B;
    R = r * 255;
    G = g * 255;
    B = b * 255;
    return (G << 24) | (R << 16) | (B << 8);
}