#define F_CPU 16000000UL
#define BAUD 9600
#define UBRR_VAL ((F_CPU / (16UL * BAUD)) - 1)

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <util/atomic.h>
#include <stdlib.h>

/* --- UART --- */
static void uart_init(void)
{
    UBRR0H = (uint8_t)(UBRR_VAL >> 8);
    UBRR0L = (uint8_t)(UBRR_VAL);
    UCSR0B = (1 << TXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);  /* 8N1 */
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

/* --- Timing --- */
volatile uint16_t ms_counter = 0;
volatile uint8_t  button_flag = 0;

ISR(TIMER1_COMPA_vect)
{
    ms_counter++;
}

ISR(INT0_vect)
{
    button_flag = 1;
}

static void timer1_init(void)
{
    TCCR1A = 0;
    TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10);
    OCR1A = 249;
    TIMSK1 = (1 << OCIE1A);
}

static uint16_t get_ms(void)
{
    uint16_t val;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        val = ms_counter;
    }
    return val;
}

/* --- LFSR for random delays --- */
static uint16_t lfsr = 0xBEEF;

static uint16_t random_delay(void)
{
    uint8_t lsb = lfsr & 1;
    lfsr >>= 1;
    if (lsb) lfsr ^= 0xB400;
    return 1000 + (lfsr % 4001);  /* 1000 to 5000 ms */
}

/* --- Main --- */
int main(void)
{
    /* LED on PB5 */
    DDRB |= (1 << PB5);
    PORTB &= ~(1 << PB5);

    /* Button on PD2 (INT0), input with pull-up */
    DDRD &= ~(1 << PD2);
    PORTD |= (1 << PD2);

    /* INT0: falling edge */
    EICRA = (1 << ISC01);
    EIMSK = (1 << INT0);

    uart_init();
    timer1_init();
    sei();

    uart_puts("Reaction Time Tester\r\n");
    uart_puts("Wait for the LED, then press the button!\r\n\r\n");

    while (1) {
        /* Reset state */
        PORTB &= ~(1 << PB1);
        button_flag = 0;
        PORTB &= ~(1 << PB5);

        uart_puts("Get ready...\r\n");

        /* Random wait */
        uint16_t wait = random_delay();
        uint16_t start = get_ms();

        /* Check for false start during wait */
        uint8_t false_start = 0;
        while ((get_ms() - start) < wait) {
            if (button_flag) {
                false_start = 1;
                break;
            }
        }

        if (false_start) {
            uart_puts("False start! Wait for the LED.\r\n\r\n");
            button_flag = 0;
            _delay_ms(2000);
            continue;
        }

        /* Light the LED and start timing */
        PORTB |= (1 << PB5);
        PORTB |= (1 << PB1);
        button_flag = 0;
        uint16_t led_on_time = get_ms();

        /* Wait for button press (up to 3 seconds) */
        while (!button_flag) {
            if ((get_ms() - led_on_time) > 3000) break;
        }

        PORTB &= ~(1 << PB5);

        if (button_flag) {
            uint16_t reaction = get_ms() - led_on_time;
            uart_puts("Reaction time: ");
            uart_putu16(reaction);
            uart_puts(" ms\r\n\r\n");
        } else {
            uart_puts("Too slow! (over 3 seconds)\r\n\r\n");
        }

        _delay_ms(2000);  /* Pause before next round */
    }
}
