#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <string.h>
#include <ctype.h>

/* Morse timing (ms) */
#define DOT_MS    200
#define DASH_MS   600
#define SYMBOL_GAP  200   /* gap between dots/dashes in a letter */
#define LETTER_GAP  600   /* gap between letters */
#define WORD_GAP   1400   /* gap between words */

/* Morse lookup: A-Z encoded as strings of '.' and '-' */
static const char *morse[] = {
    ".-",   "-...", "-.-.", "-..",  ".",    "..-.", /* A-F */
    "--.",   "....", "..",   ".---", "-.-",  ".-..", /* G-L */
    "--",   "-.",   "---",  ".--.", "--.-", ".-.",  /* M-R */
    "...",   "-",   "..-",  "...-", ".--",  "-..-", /* S-X */
    "-.--", "--.."                                   /* Y-Z */
};

static void led_on(void)  { PORTB |=  (1 << PB5); }
static void led_off(void) { PORTB &= ~(1 << PB5); }

static void delay_ms(uint16_t ms)
{
    while (ms--) _delay_ms(1);
}

static void blink_dot(void)
{
    led_on();
    delay_ms(DOT_MS);
    led_off();
    delay_ms(SYMBOL_GAP);
}

static void blink_dash(void)
{
    led_on();
    delay_ms(DASH_MS);
    led_off();
    delay_ms(SYMBOL_GAP);
}

static void send_char(char c)
{
    if (c == ' ') {
        delay_ms(WORD_GAP);
        return;
    }
    c = toupper((unsigned char)c);
    if (c < 'A' || c > 'Z') return;

    const char *code = morse[c - 'A'];
    for (uint8_t i = 0; code[i]; i++) {
        if (code[i] == '.') blink_dot();
        else                blink_dash();
    }
    delay_ms(LETTER_GAP);
}

int main(void)
{
    /* Set PB5 as output */
    DDRB |= (1 << PB5);

    const char *message = "VICTOR";  /* Change to your name */

    while (1) {
        for (uint8_t i = 0; i < strlen(message); i++) {
            send_char(message[i]);
        }
        delay_ms(WORD_GAP * 2);  /* Long pause before repeating */
    }
}
