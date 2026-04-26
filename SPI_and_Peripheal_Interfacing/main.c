#define F_CPU 16000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <util/atomic.h>
#include <stdint.h>

/*
 *   Wiring on ATmega328P / Arduino Uno:
 *   LCD RS  -> D12 = PB4
 *   LCD E   -> D11 = PB3
 *   LCD D4  -> D5  = PD5
 *   LCD D5  -> D4  = PD4
 *   LCD D6  -> D3  = PD3
 *   LCD D7  -> D2  = PD2
 *   LCD RW  -> GND
 */

#define LCD_RS_PORT PORTB
#define LCD_RS_DDR  DDRB
#define LCD_RS_PIN  PB4

#define LCD_E_PORT  PORTB
#define LCD_E_DDR   DDRB
#define LCD_E_PIN   PB3

#define LCD_DATA_PORT PORTD
#define LCD_DATA_DDR  DDRD
#define LCD_D4_PIN    PD5
#define LCD_D5_PIN    PD4
#define LCD_D6_PIN    PD3
#define LCD_D7_PIN    PD2

volatile uint32_t seconds = 0;

ISR(TIMER1_COMPA_vect)
{
    static uint16_t subsec = 0;
    subsec++;
    if (subsec >= 1000) {
        subsec = 0;
        seconds++;
    }
}

static void timer1_init(void)
{
    TCCR1A = 0;
    TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10); // CTC, prescaler 64
    OCR1A  = 249;  // 1 ms tick at 16 MHz
    TIMSK1 = (1 << OCIE1A);
}

static void lcd_pulse_enable(void)
{
    LCD_E_PORT |= (1 << LCD_E_PIN);
    _delay_us(1);
    LCD_E_PORT &= ~(1 << LCD_E_PIN);
    _delay_us(100);
}

static void lcd_write4(uint8_t nibble)
{
    // Clear D4-D7
    LCD_DATA_PORT &= ~((1 << LCD_D4_PIN) | (1 << LCD_D5_PIN) | (1 << LCD_D6_PIN) | (1 << LCD_D7_PIN));

    // Put nibble on D4-D7
    if (nibble & 0x01) LCD_DATA_PORT |= (1 << LCD_D4_PIN);
    if (nibble & 0x02) LCD_DATA_PORT |= (1 << LCD_D5_PIN);
    if (nibble & 0x04) LCD_DATA_PORT |= (1 << LCD_D6_PIN);
    if (nibble & 0x08) LCD_DATA_PORT |= (1 << LCD_D7_PIN);

    lcd_pulse_enable();
}

static void lcd_write8(uint8_t value, uint8_t rs)
{
    if (rs) LCD_RS_PORT |= (1 << LCD_RS_PIN);
    else    LCD_RS_PORT &= ~(1 << LCD_RS_PIN);

    lcd_write4(value >> 4);
    lcd_write4(value & 0x0F);
}

static void lcd_command(uint8_t cmd)
{
    lcd_write8(cmd, 0);
    if (cmd == 0x01 || cmd == 0x02) {
        _delay_ms(2);
    }
}

static void lcd_data(uint8_t data)
{
    lcd_write8(data, 1);
}

static void lcd_clear(void)
{
    lcd_command(0x01);
}

static void lcd_goto(uint8_t row, uint8_t col)
{
    static const uint8_t row_addr[] = {0x00, 0x40};
    lcd_command(0x80 | (row_addr[row] + col));
}

static void lcd_print(const char *s)
{
    while (*s) {
        lcd_data((uint8_t)*s++);
    }
}

static void lcd_init(void)
{
    // Set control pins as output
    LCD_RS_DDR |= (1 << LCD_RS_PIN);
    LCD_E_DDR  |= (1 << LCD_E_PIN);

    // Set data pins as output
    LCD_DATA_DDR |= (1 << LCD_D4_PIN) | (1 << LCD_D5_PIN) | (1 << LCD_D6_PIN) | (1 << LCD_D7_PIN);

    // Initial states
    LCD_RS_PORT &= ~(1 << LCD_RS_PIN);
    LCD_E_PORT  &= ~(1 << LCD_E_PIN);
    LCD_DATA_PORT &= ~((1 << LCD_D4_PIN) | (1 << LCD_D5_PIN) | (1 << LCD_D6_PIN) | (1 << LCD_D7_PIN));

    _delay_ms(40); // Power-on delay

    // HD44780 init sequence for 4-bit mode
    LCD_RS_PORT &= ~(1 << LCD_RS_PIN);

    lcd_write4(0x03);
    _delay_ms(5);

    lcd_write4(0x03);
    _delay_us(150);

    lcd_write4(0x03);
    _delay_us(150);

    lcd_write4(0x02); // 4-bit mode

    lcd_command(0x28); // 4-bit, 2-line, 5x8 font
    lcd_command(0x0C); // Display ON, cursor OFF, blink OFF
    lcd_command(0x06); // Entry mode, increment cursor
    lcd_clear();
}

static void format_time(uint32_t sec, char *buf)
{
    uint8_t h = (sec / 3600) % 24;
    uint8_t m = (sec / 60) % 60;
    uint8_t s = sec % 60;

    buf[0] = '0' + h / 10;
    buf[1] = '0' + h % 10;
    buf[2] = ':';
    buf[3] = '0' + m / 10;
    buf[4] = '0' + m % 10;
    buf[5] = ':';
    buf[6] = '0' + s / 10;
    buf[7] = '0' + s % 10;
    buf[8] = '\0';
}

int main(void)
{
    lcd_init();
    timer1_init();
    sei();

    uint32_t last_sec = 0xFFFFFFFF;

    while (1) {
        uint32_t now;
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
            now = seconds;
        }

        if (now != last_sec) {
            last_sec = now;

            char time_str[9];
            format_time(now, time_str);

            lcd_clear();
            lcd_goto(0, 4);     // roughly centered on a 16x2 display
            lcd_print(time_str);
        }
    }
}
