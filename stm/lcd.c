#include <string.h>
#include <stm32f4xx_gpio.h>

#include "misc.h"
#include "systick.h"
#include "lcd.h"
#include "font_petme128_8x8.h"

#define PYB_LCD_PORT        (GPIOA)
#define PYB_LCD_CS1_PIN     (GPIO_Pin_0)
#define PYB_LCD_RST_PIN     (GPIO_Pin_1)
#define PYB_LCD_A0_PIN      (GPIO_Pin_2)
#define PYB_LCD_SCL_PIN     (GPIO_Pin_3)
#define PYB_LCD_SI_PIN      (GPIO_Pin_4)

#define LCD_INSTR (0)
#define LCD_DATA (1)

static void lcd_out(int instr_data, uint8_t i) {
    sys_tick_delay_ms(0);
    PYB_LCD_PORT->BSRRH = PYB_LCD_CS1_PIN; // CS=0; enable
    if (instr_data == LCD_INSTR) {
        PYB_LCD_PORT->BSRRH = PYB_LCD_A0_PIN; // A0=0; select instr reg
    } else {
        PYB_LCD_PORT->BSRRL = PYB_LCD_A0_PIN; // A0=1; select data reg
    }
    // send byte bigendian, latches on rising clock
    for (uint32_t n = 0; n < 8; n++) {
        sys_tick_delay_ms(0);
        PYB_LCD_PORT->BSRRH = PYB_LCD_SCL_PIN; // SCL=0
        if ((i & 0x80) == 0) {
            PYB_LCD_PORT->BSRRH = PYB_LCD_SI_PIN; // SI=0
        } else {
            PYB_LCD_PORT->BSRRL = PYB_LCD_SI_PIN; // SI=1
        }
        i <<= 1;
        sys_tick_delay_ms(0);
        PYB_LCD_PORT->BSRRL = PYB_LCD_SCL_PIN; // SCL=1
    }
    PYB_LCD_PORT->BSRRL = PYB_LCD_CS1_PIN; // CS=1; disable

    /*
    in Python, native types:
    CS1_PIN(const) = 0
    n = int(0)
    delay_ms(0)
    PORT[word:BSRRH] = 1 << CS1_PIN
    for n in range(0, 8):
        delay_ms(0)
        PORT[word:BSRRH] = 1 << SCL_PIN
        if i & 0x80 == 0:
            PORT[word:BSRRH] = 1 << SI_PIN
        else:
            PORT[word:BSRRL] = 1 << SI_PIN
        i <<= 1
        delay_ms(0)
        PORT[word:BSRRL] = 1 << SCL_PIN
    */
}

/*
static void lcd_data_out(uint8_t i) {
    delay_ms(0);
    PYB_LCD_PORT->BSRRH = PYB_LCD_CS1_PIN; // CS=0; enable
    PYB_LCD_PORT->BSRRL = PYB_LCD_A0_PIN; // A0=1; select data reg
    // send byte bigendian, latches on rising clock
    for (uint32_t n = 0; n < 8; n++) {
        delay_ms(0);
        PYB_LCD_PORT->BSRRH = PYB_LCD_SCL_PIN; // SCL=0
        if ((i & 0x80) == 0) {
            PYB_LCD_PORT->BSRRH = PYB_LCD_SI_PIN; // SI=0
        } else {
            PYB_LCD_PORT->BSRRL = PYB_LCD_SI_PIN; // SI=1
        }
        i <<= 1;
        delay_ms(0);
        PYB_LCD_PORT->BSRRL = PYB_LCD_SCL_PIN; // SCL=1
    }
    PYB_LCD_PORT->BSRRL = PYB_LCD_CS1_PIN; // CS=1; disable
}
*/

#define LCD_BUF_W (16)
#define LCD_BUF_H (4)

char lcd_buffer[LCD_BUF_W * LCD_BUF_H];
int lcd_line;
int lcd_column;
int lcd_next_line;

void lcd_init(void) {
    // set the outputs high
    PYB_LCD_PORT->BSRRL = PYB_LCD_CS1_PIN;
    PYB_LCD_PORT->BSRRL = PYB_LCD_RST_PIN;
    PYB_LCD_PORT->BSRRL = PYB_LCD_A0_PIN;
    PYB_LCD_PORT->BSRRL = PYB_LCD_SCL_PIN;
    PYB_LCD_PORT->BSRRL = PYB_LCD_SI_PIN;

    // make them push/pull outputs
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = PYB_LCD_CS1_PIN | PYB_LCD_RST_PIN | PYB_LCD_A0_PIN | PYB_LCD_SCL_PIN | PYB_LCD_SI_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(PYB_LCD_PORT, &GPIO_InitStructure);

    // init the LCD
    sys_tick_delay_ms(1); // wait a bit
    PYB_LCD_PORT->BSRRH = PYB_LCD_RST_PIN; // RST=0; reset
    sys_tick_delay_ms(1); // wait for reset; 2us min
    PYB_LCD_PORT->BSRRL = PYB_LCD_RST_PIN; // RST=1; enable
    sys_tick_delay_ms(1); // wait for reset; 2us min
    lcd_out(LCD_INSTR, 0xa0); // ADC select, normal
    lcd_out(LCD_INSTR, 0xc8); // common output mode select, reverse
    lcd_out(LCD_INSTR, 0xa2); // LCD bias set, 1/9 bias
    lcd_out(LCD_INSTR, 0x2f); // power control set, 0b111=(booster on, vreg on, vfollow on)
    lcd_out(LCD_INSTR, 0x21); // v0 voltage regulator internal resistor ratio set, 0b001=small
    lcd_out(LCD_INSTR, 0x81); // electronic volume mode set
    lcd_out(LCD_INSTR, 0x34); // electronic volume register set, 0b110100
    lcd_out(LCD_INSTR, 0x40); // display start line set, 0
    lcd_out(LCD_INSTR, 0xaf); // LCD display, on

    // clear display
    for (int page = 0; page < 4; page++) {
        lcd_out(LCD_INSTR, 0xb0 | page); // page address set
        lcd_out(LCD_INSTR, 0x10); // column address set upper
        lcd_out(LCD_INSTR, 0x00); // column address set lower
        for (int i = 0; i < 128; i++) {
            lcd_out(LCD_DATA, 0x00);
        }
    }

    for (int i = 0; i < LCD_BUF_H * LCD_BUF_W; i++) {
        lcd_buffer[i] = ' ';
    }
    lcd_line = 0;
    lcd_column = 0;
    lcd_next_line = 0;
}

void lcd_print_str(const char *str) {
    lcd_print_strn(str, strlen(str));
}

void lcd_print_strn(const char *str, unsigned int len) {
    int redraw_min = lcd_line * LCD_BUF_W + lcd_column;
    int redraw_max = redraw_min;
    int did_new_line = 0;
    for (; len > 0; len--, str++) {
        // move to next line if needed
        if (lcd_next_line) {
            if (lcd_line + 1 < LCD_BUF_H) {
                lcd_line += 1;
            } else {
                lcd_line = LCD_BUF_H - 1;
                for (int i = 0; i < LCD_BUF_W * (LCD_BUF_H - 1); i++) {
                    lcd_buffer[i] = lcd_buffer[i + LCD_BUF_W];
                }
                for (int i = 0; i < LCD_BUF_W; i++) {
                    lcd_buffer[LCD_BUF_W * (LCD_BUF_H - 1) + i] = ' ';
                }
                redraw_min = 0;
                redraw_max = LCD_BUF_W * LCD_BUF_H;
            }
            lcd_next_line = 0;
            lcd_column = 0;
            did_new_line = 1;
        }
        if (*str == '\n') {
            lcd_next_line = 1;
        } else if (lcd_column >= LCD_BUF_W) {
            lcd_next_line = 1;
            str -= 1;
            len += 1;
        } else {
            lcd_buffer[lcd_line * LCD_BUF_W + lcd_column] = *str;
            lcd_column += 1;
            int max = lcd_line * LCD_BUF_W + lcd_column;
            if (max > redraw_max) {
                redraw_max = max;
            }
        }
    }

    int last_page = -1;
    for (int i = redraw_min; i < redraw_max; i++) {
        int page = i / LCD_BUF_W;
        if (page != last_page) {
            int offset = 8 * (i - (page * LCD_BUF_W));
            lcd_out(LCD_INSTR, 0xb0 | page); // page address set
            lcd_out(LCD_INSTR, 0x10 | ((offset >> 4) & 0x0f)); // column address set upper
            lcd_out(LCD_INSTR, 0x00 | (offset & 0x0f)); // column address set lower
            last_page = page;
        }
        int chr = lcd_buffer[i];
        if (chr < 32 || chr > 126) {
            chr = 127;
        }
        const uint8_t *chr_data = &font_petme128_8x8[(chr - 32) * 8];
        for (int j = 0; j < 8; j++) {
            lcd_out(LCD_DATA, chr_data[j]);
        }
    }

    if (did_new_line) {
        sys_tick_delay_ms(200);
    }
}

// writes 8 vertical pixels
// pos 0 is upper left, pos 1 is 8 pixels to right of that, pos 128 is 8 pixels below that
void lcd_draw_pixel_8(int pos, int val) {
    int page = pos / 128;
    int offset = pos - (page * 128);
    lcd_out(LCD_INSTR, 0xb0 | page); // page address set
    lcd_out(LCD_INSTR, 0x10 | ((offset >> 4) & 0x0f)); // column address set upper
    lcd_out(LCD_INSTR, 0x00 | (offset & 0x0f)); // column address set lower
    lcd_out(LCD_DATA, val); // write data
}