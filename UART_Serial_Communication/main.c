#define F_CPU 16000000UL
#define BAUD 9600
#define UBRR_VAL ((F_CPU / (16UL * BAUD)) - 1)

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <util/atomic.h>
#include <stdlib.h>
#include <math.h>

/* --- UART (blocking for simplicity) --- */
static void uart_init(void)
{
    UBRR0H = (uint8_t)(UBRR_VAL >> 8);
    UBRR0L = (uint8_t)(UBRR_VAL);
    UCSR0B = (1 << TXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

static void uart_putc(char c)
{
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = c;
}

static void uart_puts(const char *s)
{
    while (*s) uart_putc(*s++);
}

static void uart_putu16(uint16_t val)
{
    char buf[6];
    utoa(val, buf, 10);
    uart_puts(buf);
}

static void uart_putu32(uint32_t val)
{
    char buf[11];
    ultoa(val, buf, 10);
    uart_puts(buf);
}

/* --- ADC --- */
static void adc_init(void)
{
    ADMUX  = (1 << REFS0);  /* AVcc reference, channel 0 */
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
    /* ADC enabled, prescaler 128 (125 kHz ADC clock) */
}

static uint16_t adc_read(void)
{
    ADCSRA |= (1 << ADSC);          /* Start conversion */
    while (ADCSRA & (1 << ADSC));   /* Wait for completion */
        return ADC;
}

/* --- Timer for timestamps --- */
volatile uint32_t ms_ticks = 0;

ISR(TIMER1_COMPA_vect)
{
    ms_ticks++;
}

static void timer1_init(void)
{
    TCCR1A = 0;
    TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10);
    OCR1A  = 249;
    TIMSK1 = (1 << OCIE1A);
}

static uint32_t get_ms(void)
{
    uint32_t val;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        val = ms_ticks;
    }
    return val;
}

/* --- Temperature conversion --- */
#define B_COEFF  3950.0
#define T0_K     298.15
#define R0       10000.0
#define R_FIXED  10000.0

static int16_t adc_to_temp_x10(uint16_t adc_val)
{
    if (adc_val == 0) adc_val = 1;  /* Avoid division by zero */

        double r_therm = R_FIXED * (1023.0 - adc_val) / adc_val;
    double t_kelvin = 1.0 / ((1.0 / T0_K) + (log(r_therm / R0) / B_COEFF));
    double t_celsius = t_kelvin - 273.15;

    return (int16_t)(t_celsius * 10);  /* Fixed-point: 25.3C = 253 */
}

/* --- Main --- */
int main(void)
{
    uart_init();
    adc_init();
    timer1_init();
    sei();

    /* CSV header */
    uart_puts("timestamp_ms,adc_raw,temp_c\r\n");

    uint32_t next_sample = 0;

    while (1) {
        uint32_t now = get_ms();
        if (now >= next_sample) {
            next_sample = now + 1000;  /* 1 Hz sample rate */

            uint16_t adc_val = adc_read();
            int16_t temp_x10 = adc_to_temp_x10(adc_val);

            /* timestamp */
            uart_putu32(now);
            uart_putc(',');

            /* raw ADC */
            uart_putu16(adc_val);
            uart_putc(',');

            /* temperature with one decimal place */
            int16_t whole = temp_x10 / 10;
            int16_t frac  = temp_x10 % 10;
            if (frac < 0) frac = -frac;
            if (temp_x10 < 0 && whole == 0) uart_putc('-');
            char buf[8];
            itoa(whole, buf, 10);
            uart_puts(buf);
            uart_putc('.');
            itoa(frac, buf, 10);
            uart_puts(buf);

            uart_puts("\r\n");
        }
    }
}
