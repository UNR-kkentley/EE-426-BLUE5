/* STM32 NUCLEO-F446RE: CMSIS-only DTMF generator + SSD1306 I2C OLED.
 * Clock: reset-default 16 MHz HSI.
 * OLED: 128x64 SSD1306, 7-bit I2C address 0x3C, PB8=SCL PB9=SDA.
 * Keypad: rows PA10/PB3/PB5/PB4; columns PB10/PA8/PA9/PC7.
 * Audio: PA4 DAC1, 16 kHz TIM6 ISR. LED: PA5.
 */
#include "stm32f446xx.h"
#include <stdint.h>
#include <string.h>

#define FS            16000U
#define OLED_ADDR     0x3CU  /* change to 0x3D if needed */
#define OLED_W        128U
#define OLED_PAGES    8U
#define I2C_TIMEOUT   100000U

static volatile uint32_t phase1, phase2, step1, step2, audio_ticks;
static volatile uint8_t tone_enabled;

static const int16_t sine[32] = {
     0,146,287,417,530,624,693,736,750,736,693,624,530,417,287,146,
     0,-146,-287,-417,-530,-624,-693,-736,-750,-736,-693,-624,-530,-417,-287,-146
};

static void LED_Init(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    GPIOA->MODER = (GPIOA->MODER & ~(3U << 10)) | (1U << 10);
    GPIOA->BSRR = (1U << 21);
}

static void DAC_Init(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_DACEN;
    GPIOA->MODER |= (3U << 8);
    GPIOA->PUPDR &= ~(3U << 8);
    DAC->CR = DAC_CR_EN1;
    DAC->DHR12R1 = 2048;
}

static void Keypad_Init(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN;
    GPIOA->MODER = (GPIOA->MODER & ~(3U << 20)) | (1U << 20);
    for (uint32_t pin = 3; pin <= 5; pin++) {
        GPIOB->MODER = (GPIOB->MODER & ~(3U << (pin * 2))) | (1U << (pin * 2));
    }
    GPIOB->MODER &= ~(3U << 20);           /* PB10 input */
    GPIOA->MODER &= ~((3U << 16) | (3U << 18)); /* PA8, PA9 input */
    GPIOC->MODER &= ~(3U << 14);           /* PC7 input */
    GPIOB->PUPDR = (GPIOB->PUPDR & ~(3U << 20)) | (1U << 20);
    GPIOA->PUPDR = (GPIOA->PUPDR & ~((3U << 16) | (3U << 18))) |
                   (1U << 16) | (1U << 18);
    GPIOC->PUPDR = (GPIOC->PUPDR & ~(3U << 14)) | (1U << 14);
    GPIOA->BSRR = (1U << 10);
    GPIOB->BSRR = (1U << 3) | (1U << 4) | (1U << 5);
}

static char Keypad_GetKey(void) {
    static const char keys[4][4] = {
        {'1','2','3','A'}, {'4','5','6','B'},
        {'7','8','9','C'}, {'*','0','#','D'}
    };
    for (int row = 0; row < 4; row++) {
        GPIOA->BSRR = (1U << 10);
        GPIOB->BSRR = (1U << 3) | (1U << 4) | (1U << 5);
        if      (row == 0) GPIOA->BSRR = (1U << (10+16));
        else if (row == 1) GPIOB->BSRR = (1U << (3+16));
        else if (row == 2) GPIOB->BSRR = (1U << (5+16));
        else               GPIOB->BSRR = (1U << (4+16));
        for (volatile int i = 0; i < 100; i++) { }
        if (!(GPIOB->IDR & (1U << 10))) return keys[row][0];
        if (!(GPIOA->IDR & (1U << 8)))  return keys[row][1];
        if (!(GPIOA->IDR & (1U << 9)))  return keys[row][2];
        if (!(GPIOC->IDR & (1U << 7)))  return keys[row][3];
    }
    return '\0';
}

/* Return frequencies to display; no keypad input means disable tone. */
static void DTMF_SetKey(char key, uint16_t *low_hz, uint16_t *high_hz) {
    static const char keys[4][4] = {
        {'1','2','3','A'}, {'4','5','6','B'},
        {'7','8','9','C'}, {'*','0','#','D'}
    };
    static const uint16_t low[4] = {697,770,852,941};
    static const uint16_t high[4] = {1209,1336,1477,1633};
    uint32_t s1 = 0, s2 = 0;
    uint8_t found = 0;
    *low_hz = 0;
    *high_hz = 0;
    for (int r = 0; r < 4 && !found; r++) {
        for (int c = 0; c < 4; c++) {
            if (keys[r][c] == key) {
                *low_hz = low[r];
                *high_hz = high[c];
                s1 = (uint32_t)(((uint64_t)low[r] << 32) / FS);
                s2 = (uint32_t)(((uint64_t)high[c] << 32) / FS);
                found = 1;
                break;
            }
        }
    }
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    tone_enabled = 0;
    phase1 = phase2 = 0;
    step1 = s1;
    step2 = s2;
    tone_enabled = found;
    __set_PRIMASK(primask);
    if (!found) DAC->DHR12R1 = 2048;
}

static void TIM6_Init(void) {
    RCC->APB1ENR |= RCC_APB1ENR_TIM6EN;
    TIM6->CR1 = 0;
    TIM6->PSC = 0;
    TIM6->ARR = 999; /* 16 MHz / 1000 = 16 kHz */
    TIM6->EGR = TIM_EGR_UG;
    TIM6->SR = 0;
    TIM6->DIER = TIM_DIER_UIE;
    NVIC_SetPriority(TIM6_DAC_IRQn, 2);
    NVIC_EnableIRQ(TIM6_DAC_IRQn);
    TIM6->CR1 = TIM_CR1_CEN;
}

void TIM6_DAC_IRQHandler(void) {
    if (TIM6->SR & TIM_SR_UIF) {
        TIM6->SR = 0;
        audio_ticks++;
        if (tone_enabled) {
            phase1 += step1;
            phase2 += step2;
            DAC->DHR12R1 = (uint32_t)(2048 + sine[phase1 >> 27] + sine[phase2 >> 27]);
        } else {
            DAC->DHR12R1 = 2048;
        }
    }
}

static void wait_ms(uint32_t ms) {
    uint32_t start = audio_ticks;
    uint32_t ticks = ms * (FS / 1000U);
    while ((uint32_t)(audio_ticks - start) < ticks) { }
}

/* Blocking I2C1 transmit, with finite timeout so a disconnected OLED does
 * not freeze keypad or sound. In an I2C fault, next OLED update may retry. */
static uint8_t i2c_wait_sr1(uint32_t flag) {
    uint32_t t = I2C_TIMEOUT;
    while (!(I2C1->SR1 & flag)) {
        if (I2C1->SR1 & (I2C_SR1_AF | I2C_SR1_BERR | I2C_SR1_ARLO)) return 0;
        if (--t == 0) return 0;
    }
    return 1;
}
static void i2c_abort(void) {
    I2C1->CR1 |= I2C_CR1_STOP;
    I2C1->SR1 &= ~(I2C_SR1_AF | I2C_SR1_BERR | I2C_SR1_ARLO);
}
static uint8_t i2c_begin(uint8_t control) {
    uint32_t t = I2C_TIMEOUT;
    while ((I2C1->SR2 & I2C_SR2_BUSY) && --t) { }
    if (!t) return 0;
    I2C1->CR1 |= I2C_CR1_START;
    if (!i2c_wait_sr1(I2C_SR1_SB)) { i2c_abort(); return 0; }
    I2C1->DR = OLED_ADDR << 1;
    if (!i2c_wait_sr1(I2C_SR1_ADDR)) { i2c_abort(); return 0; }
    (void)I2C1->SR1;
    (void)I2C1->SR2; /* clear ADDR */
    if (!i2c_wait_sr1(I2C_SR1_TXE)) { i2c_abort(); return 0; }
    I2C1->DR = control; /* 0x00 commands, 0x40 display data */
    return 1;
}
static uint8_t i2c_byte(uint8_t byte) {
    if (!i2c_wait_sr1(I2C_SR1_TXE)) { i2c_abort(); return 0; }
    I2C1->DR = byte;
    return 1;
}
static void i2c_end(void) {
    if (!i2c_wait_sr1(I2C_SR1_BTF)) { i2c_abort(); return; }
    I2C1->CR1 |= I2C_CR1_STOP;
}

static void I2C1_Init(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
    /* PB8/PB9: alternate-function 4, open-drain with pull-ups. */
    GPIOB->MODER = (GPIOB->MODER & ~((3U << 16) | (3U << 18))) |
                   (2U << 16) | (2U << 18);
    GPIOB->OTYPER |= (1U << 8) | (1U << 9);
    GPIOB->OSPEEDR |= (2U << 16) | (2U << 18);
    GPIOB->PUPDR = (GPIOB->PUPDR & ~((3U << 16) | (3U << 18))) |
                   (1U << 16) | (1U << 18);
    GPIOB->AFR[1] = (GPIOB->AFR[1] & ~0xFFU) | 0x44U;
    /* PCLK1 = 16 MHz. Standard mode I2C at ~100 kHz. */
    I2C1->CR1 &= ~I2C_CR1_PE;
    I2C1->CR2 = 16U;
    I2C1->CCR = 80U;
    I2C1->TRISE = 17U;
    I2C1->CR1 |= I2C_CR1_PE;
}

static void oled_commands(const uint8_t *cmd, uint32_t count) {
    if (!i2c_begin(0x00)) return;
    for (uint32_t i = 0; i < count; i++) {
        if (!i2c_byte(cmd[i])) return;
    }
    i2c_end();
}

/* A small uppercase 5x7 font, column-major, one byte per column. */
static const uint8_t font_digits[10][5] = {
    {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4B,0x31},
    {0x18,0x14,0x12,0x7F,0x10}, {0x27,0x45,0x45,0x45,0x39},
    {0x3C,0x4A,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1E}
};
static const uint8_t font_letters[26][5] = {
    {0x7E,0x11,0x11,0x11,0x7E}, {0x7F,0x49,0x49,0x49,0x36},
    {0x3E,0x41,0x41,0x41,0x22}, {0x7F,0x41,0x41,0x22,0x1C},
    {0x7F,0x49,0x49,0x49,0x41}, {0x7F,0x09,0x09,0x09,0x01},
    {0x3E,0x41,0x49,0x49,0x7A}, {0x7F,0x08,0x08,0x08,0x7F},
    {0x00,0x41,0x7F,0x41,0x00}, {0x20,0x40,0x41,0x3F,0x01},
    {0x7F,0x08,0x14,0x22,0x41}, {0x7F,0x40,0x40,0x40,0x40},
    {0x7F,0x02,0x0C,0x02,0x7F}, {0x7F,0x04,0x08,0x10,0x7F},
    {0x3E,0x41,0x41,0x41,0x3E}, {0x7F,0x09,0x09,0x09,0x06},
    {0x3E,0x41,0x51,0x21,0x5E}, {0x7F,0x09,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31}, {0x01,0x01,0x7F,0x01,0x01},
    {0x3F,0x40,0x40,0x40,0x3F}, {0x1F,0x20,0x40,0x20,0x1F},
    {0x3F,0x40,0x38,0x40,0x3F}, {0x63,0x14,0x08,0x14,0x63},
    {0x07,0x08,0x70,0x08,0x07}, {0x61,0x51,0x49,0x45,0x43}
};
static uint8_t oled_fb[OLED_PAGES * OLED_W];

static void oled_glyph(char ch, uint8_t out[5]) {
    if (ch >= '0' && ch <= '9') memcpy(out, font_digits[ch - '0'], 5);
    else if (ch >= 'A' && ch <= 'Z') memcpy(out, font_letters[ch - 'A'], 5);
    else if (ch == ':') { uint8_t v[5] = {0,0x36,0x36,0,0}; memcpy(out,v,5); }
    else if (ch == '-') { uint8_t v[5] = {0x08,0x08,0x08,0x08,0x08}; memcpy(out,v,5); }
    else if (ch == '*') { uint8_t v[5] = {0x14,0x08,0x3E,0x08,0x14}; memcpy(out,v,5); }
    else if (ch == '#') { uint8_t v[5] = {0x14,0x7F,0x14,0x7F,0x14}; memcpy(out,v,5); }
    else memset(out, 0, 5); /* blank for space/unsupported */
}

/* Draw text at x/pixel and page (8-pixel row). 6 pixels/chara*/
static void oled_text(uint8_t x, uint8_t page, const char *s) {
    if (page >= OLED_PAGES) return;
    while (*s && x <= OLED_W - 6U) {
        uint8_t glyph[5];
        oled_glyph(*s++, glyph);
        for (int i = 0; i < 5; i++) oled_fb[(uint32_t)page * OLED_W + x + i] = glyph[i];
        oled_fb[(uint32_t)page * OLED_W + x + 5] = 0;
        x += 6;
    }
}

static void oled_uint(char *buf, uint16_t value) {
    char rev[6]; int n = 0, i = 0;
    do { rev[n++] = (char)('0' + (value % 10)); value /= 10; } while (value);
    while (n) buf[i++] = rev[--n];
    buf[i] = '\0';
}

/* Main screen */
static void oled_show(uint8_t active, char key, uint16_t lo, uint16_t hi) {
    char n[6];
    memset(oled_fb, 0, sizeof(oled_fb));
    oled_text(0, 0, "DTMF GENERATOR");
    oled_text(0, 1, active ? "STATUS: ACTIVE" : "STATUS: IDLE");
    oled_text(0, 2, "KEY:");
    { char k[2] = { key ? key : '-', '\0' }; oled_text(36, 2, k); }
    oled_text(0, 3, "LOW:");
    if (lo) { oled_uint(n, lo); oled_text(36, 3, n); oled_text(72, 3, "HZ"); }
    else oled_text(36, 3, "---");
    oled_text(0, 4, "HIGH:");
    if (hi) { oled_uint(n, hi); oled_text(36, 4, n); oled_text(72, 4, "HZ"); }
    else oled_text(36, 4, "---");
    oled_text(0, 6, "OUTPUT: DAC PA4");

    /* SSD1306 horizontal addressing, all 8 pages (128x64). */
    static const uint8_t area[] = {0x21, 0x00, 0x7F, 0x22, 0x00, 0x07};
    oled_commands(area, sizeof(area));
    if (!i2c_begin(0x40)) return;
    for (uint32_t i = 0; i < sizeof(oled_fb); i++) {
        if (!i2c_byte(oled_fb[i])) return;
    }
    i2c_end();
}

static void OLED_Init(void) {
    wait_ms(100); /* power settle */
    static const uint8_t init[] = {
        0xAE, 0xD5,0x80, 0xA8,0x3F, 0xD3,0x00, 0x40,
        0x8D,0x14, 0x20,0x00, 0xA1, 0xC8, 0xDA,0x12,
        0x81,0x7F, 0xD9,0xF1, 0xDB,0x40, 0xA4,0xA6, 0x2E,0xAF
    };
    oled_commands(init, sizeof(init));
}

int main(void) {
    LED_Init();
    Keypad_Init();
    DAC_Init();
    TIM6_Init();
    I2C1_Init();
    OLED_Init();

    char previous_key = '\0';
    char last_key = '\0';
    uint16_t last_low = 0, last_high = 0;
    oled_show(0, last_key, last_low, last_high);

    while (1) {
        char key = Keypad_GetKey(); // First reading
        if (key != previous_key) {
            wait_ms(15); // Debounce delay
            if (key == Keypad_GetKey()) {  // Confirm key
                previous_key = key;  // Accept new state
                uint16_t lo, hi;
                DTMF_SetKey(key, &lo, &hi);
                if (key) {
                    last_key = key;
                    last_low = lo;
                    last_high = hi;
                    GPIOA->BSRR = (1U << 5);
                } else {
                    GPIOA->BSRR = (1U << 21);
                }
                /* Retain last key/frequencies during IDLE. */
                oled_show(key != '\0', last_key, last_low, last_high);
            }
        }
    }
}
