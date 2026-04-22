#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>

/* OCR1A values for C4 to C6 (25 semitones, prescaler 8) */
static const uint16_t notes[] = {
    3816, 3607, 3400, 3213, 3029, 2862,  /* C4  to F4  */
    2702, 2550, 2406, 2272, 2144, 2023,  /* F#4 to B4  */
    1910, 1803, 1702, 1607, 1516, 1431,  /* C5  to F5  */
    1350, 1275, 1203, 1135, 1072, 1011,  /* F#5 to B5  */
    954                                    /* C6         */
};

#define NUM_NOTES (sizeof(notes) / sizeof(notes[0]))
#define BTN_UP   PD6
#define BTN_DOWN PD7

static uint8_t debounce(uint8_t pin)
{
    if (!(PIND & (1 << pin))) {
        _delay_ms(50);
        if (!(PIND & (1 << pin)))
            return 1;
    }
    return 0;
}

int main(void)
{
    /* OC1A (PB1) as output for tone */
    DDRB |= (1 << PB1);

    /* Buttons as inputs with pull-ups */
    DDRD &= ~((1 << BTN_UP) | (1 << BTN_DOWN));
    PORTD |= (1 << BTN_UP) | (1 << BTN_DOWN);

    /* Timer1: CTC mode, toggle OC1A on compare match, prescaler 8 */
    TCCR1A = (1 << COM1A0);              /* Toggle OC1A on match */
    TCCR1B = (1 << WGM12) | (1 << CS11); /* CTC mode, prescaler 8 */

    uint8_t note_idx = 9;  /* Start at A4 (440 Hz) */
    OCR1A = notes[note_idx];

    while (1) {
        if (debounce(BTN_UP)) {
            if (note_idx < NUM_NOTES - 1) {
                note_idx++;
                OCR1A = notes[note_idx];
            }
            while (!(PIND & (1 << BTN_UP)));  /* Wait for release */
                _delay_ms(50);
        }

        if (debounce(BTN_DOWN)) {
            if (note_idx > 0) {
                note_idx--;
                OCR1A = notes[note_idx];
            }
            while (!(PIND & (1 << BTN_DOWN)));
            _delay_ms(50);
        }
    }
}
