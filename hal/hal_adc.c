#include "hal_adc.h"
#include <stdbool.h>
#include <stdint.h>
#include <LPC17xx.h>

#define MAX_NO_OF_ADC_CHANNELS 8
#define ADC_CLKDIV 0x00

void init_hal_adc(void) 
{
    LPC_SC->PCLKSEL0 |= (3UL << 24);  // set ADC CCLK
    LPC_SC->PCONP |= (1 << 12);     // Power up ADC    
    LPC_ADC->ADINTEN = 0x00; // No interrupts
    LPC_PINCON->PINSEL1 |= (1 << 14);  // P0.23 (AD0.0)
    LPC_PINCON->PINSEL1 |= (1 << 16);  // P0.24 (AD0.1)
    LPC_PINCON->PINSEL1 |= (1 << 18);  // P0.25 (AD0.2)
    LPC_PINCON->PINSEL1 |= (1 << 20);  // P0.26 (AD0.3)
    LPC_PINCON->PINSEL3 |= (3 << 28);  // P1.30 (AD0.4)
    LPC_PINCON->PINSEL3 |= (3 << 30);  // P1.31 (AD0.5)
    LPC_PINCON->PINSEL0 |= (3 << 6);   // P0.3  (AD0.6)
    LPC_PINCON->PINSEL0 |= (3 << 4);   // P0.2  (AD0.7)
    LPC_ADC->ADCR = 0x002100ff | (uint32_t)ADC_CLKDIV << 8; // start burst convert of all channels
}

void task_hal_adc(void)
{

}

uint32_t hal_adc_get_value(uint8_t channelno)
{
    uint32_t regval;
    if(channelno < MAX_NO_OF_ADC_CHANNELS) {
        switch(channelno)
        {
            case 0: regval = LPC_ADC->ADDR0; break;
            case 1: regval = LPC_ADC->ADDR1; break;
            case 2: regval = LPC_ADC->ADDR2; break;
            case 3: regval = LPC_ADC->ADDR3; break;
            case 4: regval = LPC_ADC->ADDR4; break;
            case 5: regval = LPC_ADC->ADDR5; break;
            case 6: regval = LPC_ADC->ADDR6; break;
            case 7: regval = LPC_ADC->ADDR7; break;
        }
        return ((regval >> 4) & 0xfff);
    }
    return 0; //todo: assert
}