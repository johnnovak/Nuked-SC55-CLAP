/*
 * Copyright (C) 2021, 2024 nukeykt
 *
 *  Redistribution and use of this code or any derivative works are permitted
 *  provided that the following conditions are met:
 *
 *   - Redistributions may not be sold, nor may they be used in a commercial
 *     product or activity.
 *
 *   - Redistributions that are modified from the original source must include the
 *     complete source code, including the source code for all components used by a
 *     binary built from the modified sources. However, as a special exception, the
 *     source code distributed need not include anything that is normally distributed
 *     (in either source or binary form) with the major components (compiler, kernel,
 *     and so on) of the operating system on which the executable runs, unless that
 *     component itself accompanies the executable.
 *
 *   - Redistributions must reproduce the above copyright notice, this list of
 *     conditions and the following disclaimer in the documentation and/or other
 *     materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "lcd.h"
#include "mcu.h"
#include "submcu.h"
#include "emu.h"
#include <fstream>

void LCD_Enable(lcd_t& lcd, uint32_t enable)
{
    lcd.enable = enable;
}

void LCD_Write(lcd_t& lcd, uint32_t address, uint8_t data)
{
    if (address == 0)
    {
        if ((data & 0xe0) == 0x20)
        {
            lcd.LCD_DL = (data & 0x10) != 0;
            lcd.LCD_N = (data & 0x8) != 0;
            lcd.LCD_F = (data & 0x4) != 0;
        }
        else if ((data & 0xf8) == 0x8)
        {
            lcd.LCD_D = (data & 0x4) != 0;
            lcd.LCD_C = (data & 0x2) != 0;
            lcd.LCD_B = (data & 0x1) != 0;
        }
        else if ((data & 0xff) == 0x01)
        {
            lcd.LCD_DD_RAM = 0;
            lcd.LCD_ID = 1;
            memset(lcd.LCD_Data, 0x20, sizeof(lcd.LCD_Data));
        }
        else if ((data & 0xff) == 0x02)
        {
            lcd.LCD_DD_RAM = 0;
        }
        else if ((data & 0xfc) == 0x04)
        {
            lcd.LCD_ID = (data & 0x2) != 0;
            lcd.LCD_S = (data & 0x1) != 0;
        }
        else if ((data & 0xc0) == 0x40)
        {
            lcd.LCD_CG_RAM = (data & 0x3f);
            lcd.LCD_RAM_MODE = 0;
        }
        else if ((data & 0x80) == 0x80)
        {
            lcd.LCD_DD_RAM = (data & 0x7f);
            lcd.LCD_RAM_MODE = 1;
        }
        else
        {
            address += 0;
        }
    }
    else
    {
        if (!lcd.LCD_RAM_MODE)
        {
            lcd.LCD_CG[lcd.LCD_CG_RAM] = data & 0x1f;
            if (lcd.LCD_ID)
            {
                lcd.LCD_CG_RAM++;
            }
            else
            {
                lcd.LCD_CG_RAM--;
            }
            lcd.LCD_CG_RAM &= 0x3f;
        }
        else
        {
            if (lcd.LCD_N)
            {
                if (lcd.LCD_DD_RAM & 0x40)
                {
                    if ((lcd.LCD_DD_RAM & 0x3f) < 40)
                        lcd.LCD_Data[(lcd.LCD_DD_RAM & 0x3f) + 40] = data;
                }
                else
                {
                    if ((lcd.LCD_DD_RAM & 0x3f) < 40)
                        lcd.LCD_Data[lcd.LCD_DD_RAM & 0x3f] = data;
                }
            }
            else
            {
                if (lcd.LCD_DD_RAM < 80)
                    lcd.LCD_Data[lcd.LCD_DD_RAM] = data;
            }
            if (lcd.LCD_ID)
            {
                lcd.LCD_DD_RAM++;
            }
            else
            {
                lcd.LCD_DD_RAM--;
            }
            lcd.LCD_DD_RAM &= 0x7f;
        }
    }
    //fprintf(stderr, "%i %.2x ", address, data);
    // if (data >= 0x20 && data <= 'z')
    //     fprintf(stderr, "%c\n", data);
    //else
    //    fprintf(stderr, "\n");
}

void LCD_Init(lcd_t& lcd, mcu_t& mcu)
{
    lcd.mcu = &mcu;
}
