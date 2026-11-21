
#include "stm32f446xx.h"
#include <stdint.h>

#define SAMPLE_RATE 16000

volatile uint16_t adc_value = 2048;

void ADC_Init(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;

    GPIOA->MODER |= (3U << 0);  // PA0 analog input
    GPIOA->PUPDR &= ~(3U << 0);

    ADC1->CR1 = 0;
    ADC1->CR2 = 0;
    ADC->CCR &= ~ADC_CCR_ADCPRE;
    ADC1->SMPR2 = (ADC1->SMPR2 & ~7U) | 4U;
    ADC1->SQR1 = 0;
    ADC1->SQR3 = 0;
    ADC1->SR = 0;
}

void DMA_Init(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;

    DMA2_Stream0->CR &= ~DMA_SxCR_EN;
    while (DMA2_Stream0->CR & DMA_SxCR_EN);
    DMA2->LIFCR = 0x3D;

    DMA2_Stream0->PAR = (uint32_t)&ADC1->DR;
    DMA2_Stream0->M0AR = (uint32_t)&adc_value;
    DMA2_Stream0->NDTR = 1;
    DMA2_Stream0->FCR = 0;
    DMA2_Stream0->CR = DMA_SxCR_CIRC | DMA_SxCR_PSIZE_0 | DMA_SxCR_MSIZE_0;
    DMA2_Stream0->CR |= DMA_SxCR_EN;
}

void DAC_Init(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_DACEN;

    GPIOA->MODER |= (3U << 8);  // PA4 analog output
    GPIOA->PUPDR &= ~(3U << 8);

    DAC->CR = DAC_CR_EN1;
    DAC->DHR12R1 = 2048;
}

void TIM6_Init(void) {
    RCC->APB1ENR |= RCC_APB1ENR_TIM6EN;

    TIM6->CR1 = 0;
    TIM6->PSC = 0;
    TIM6->ARR = (16000000U / SAMPLE_RATE) - 1;
    TIM6->EGR = TIM_EGR_UG;
    TIM6->SR = 0;
    TIM6->DIER = TIM_DIER_UIE;

    NVIC_SetPriority(TIM6_DAC_IRQn, 2);
    NVIC_EnableIRQ(TIM6_DAC_IRQn);
    TIM6->CR1 |= TIM_CR1_CEN;
}

void TIM6_DAC_IRQHandler(void) {
    if (TIM6->SR & TIM_SR_UIF) {
        TIM6->SR = 0;
        DAC->DHR12R1 = adc_value & 0x0FFF;
        ADC1->CR2 |= ADC_CR2_SWSTART;
    }
}

int main(void) {
    ADC_Init();
    DMA_Init();
    DAC_Init();

    ADC1->CR2 |= ADC_CR2_DMA | ADC_CR2_DDS | ADC_CR2_ADON;
    for (volatile int i = 0; i < 200; i++);

    TIM6_Init();

    while (1) {
        __WFI();
    }
}
