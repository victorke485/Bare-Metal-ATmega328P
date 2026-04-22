#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>

/* Variable-length delay: _delay_ms() requires a compile-time
 *  constant on many avr-gcc versions, so we wrap it in a loop. */
static void delay_ms(uint16_t ms)
{
    while (ms--) _delay_ms(1);
}

#define LED_MASK ((1<<PD2)|(1<<PD3)|(1<<PD4)|(1<<PD5))
#define BTN_PIN  PD6

static uint16_t lfsr = 0xACE1;

static uint8_t roll_dice(void)
{
    uint8_t lsb = lfsr & 1;
    lfsr >>= 1;
    if (lsb) lfsr ^= 0xB400;
    return (lfsr % 6) + 1;
}

static void show_dice(uint8_t value)
{
    /* Clear all LEDs */
    PORTD &= ~LED_MASK;

    uint8_t pattern = 0;
    switch (value) {
        case 1: pattern = (1<<PD3); break;
        case 2: pattern = (1<<PD2)|(1<<PD5); break;
        case 3: pattern = (1<<PD2)|(1<<PD4)|(1<<PD5); break;
        case 4: pattern = (1<<PD2)|(1<<PD3)|(1<<PD4)|(1<<PD5); break;
        case 5: /* All + single blink to distinguish from 4 */
            pattern = (1<<PD2)|(1<<PD3)|(1<<PD4)|(1<<PD5);
            PORTD |= pattern;
            delay_ms(100);
            PORTD &= ~LED_MASK;
            delay_ms(100);
            break;
        case 6: /* All + double blink to distinguish from 4 and 5 */
            pattern = (1<<PD2)|(1<<PD3)|(1<<PD4)|(1<<PD5);
            PORTD |= pattern;
            delay_ms(100);
            PORTD &= ~LED_MASK;
            delay_ms(100);
            PORTD |= pattern;
            delay_ms(100);
            PORTD &= ~LED_MASK;
            delay_ms(100);
            break;
    }
    PORTD |= pattern;
}

static uint8_t button_pressed(void)
{
    if (!(PIND & (1 << BTN_PIN))) {
        _delay_ms(50);  /* Debounce */
        if (!(PIND & (1 << BTN_PIN))) {
            return 1;
        }
    }
    return 0;
}

int main(void)
{
    /* LEDs as outputs */
    DDRD |= LED_MASK;

    /* Button as input with internal pull-up */
    DDRD &= ~(1 << BTN_PIN);
    PORTD |= (1 << BTN_PIN);

    uint8_t last_value = 1;
    show_dice(last_value);

    while (1) {
        /* Keep the LFSR running for randomness */
        (void)roll_dice();

        if (button_pressed()) {
            /* Rolling animation */
            for (uint8_t i = 0; i < 10; i++) {
                show_dice((i % 6) + 1);
                delay_ms(50 + i * 20);
            }

            last_value = roll_dice();
            show_dice(last_value);

            /* Wait for button release */
            while (!(PIND & (1 << BTN_PIN)));
            _delay_ms(50);
        }
    }
}
