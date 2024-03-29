/*
 * This file is part of the simplux project.
 *
 * Copyright (C) 2011 Piotr Esden-Tempski <piotr@esden.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

u8 anim[10][8] = {
  {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80},
  {0x00, 0x00, 0x03, 0x0C, 0x30, 0xC0, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x0F, 0xF0, 0x00, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0xF0, 0x0F, 0x00, 0x00, 0x00},
  {0x00, 0x00, 0xC0, 0x30, 0x0C, 0x03, 0x00, 0x00},
  {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01},
  {0x20, 0x20, 0x10, 0x10, 0x08, 0x08, 0x04, 0x04},
  {0x10, 0x10, 0x10, 0x10, 0x08, 0x08, 0x08, 0x08},
  {0x08, 0x08, 0x08, 0x08, 0x10, 0x10, 0x10, 0x10},
  {0x04, 0x04, 0x08, 0x08, 0x10, 0x10, 0x20, 0x20},
};
int anim_frames = 10;

int frame_count = 0;
int frame_delay = 0;

/* Set STM32 to 72 MHz. */
void clock_setup(void)
{
	rcc_clock_setup_in_hse_8mhz_out_72mhz();

	/* Enable GPIOA and GPIOB clock. */
	rcc_peripheral_enable_clock(&RCC_APB2ENR, RCC_APB2ENR_IOPAEN);
	rcc_peripheral_enable_clock(&RCC_APB2ENR, RCC_APB2ENR_IOPBEN);

}

void gpio_setup(void)
{
	/* Row 0..5 */
	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ,
		      GPIO_CNF_OUTPUT_OPENDRAIN,
		      GPIO0 |
		      GPIO1 |
		      GPIO2 |
		      GPIO3 |
		      GPIO6 |
		      GPIO7);

	/* Row 6 and 7 */
	gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ,
		      GPIO_CNF_OUTPUT_OPENDRAIN,
		      GPIO0 |
		      GPIO1);


	/* Shift register gpio
	 * PB6: Col Clk
	 * PB7: Col Data
	 * PB8: Col Output Enable
	 * PB9: Col Latch
	 */
	gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ,
		      GPIO_CNF_OUTPUT_PUSHPULL,
		      GPIO6 |
		      GPIO7 |
		      GPIO8 |
		      GPIO9);

}

void clear_rows()
{
	/* set all rows to off */
	GPIO_BSRR(GPIOA) = 0xCF;
	GPIO_BSRR(GPIOB) = 0x03;
}

void clear_cols()
{
  int i;

  /* Initialize the state of the shift register control pins */
  gpio_clear(GPIOB, GPIO7);
  gpio_clear(GPIOB, GPIO8);
  gpio_set(GPIOB, GPIO9);

  /* Erase the content of the shift register */
  for (i = 0; i < 8; i++) {
    gpio_clear(GPIOB, GPIO6);
    gpio_set(GPIOB, GPIO6);
  }
}

void render_frame(u8 *frame_data)
{
  int i, j;

  /* We are at the first column so we are shifting one bit into the shift register. */
  gpio_set(GPIOB, GPIO7); /* data pin = 1 */
  for (i = 0; i < 8; i++) {
    /* advancing the shift register clock by one. */
    gpio_clear(GPIOB, GPIO6);
    gpio_set(GPIOB, GPIO6);

    /* All subsequent 7 bits will be zero from now on. */
    gpio_clear(GPIOB, GPIO7); /* data pin = 0 */

    /* Set the correct rows to on according to animation data */
    GPIO_BRR(GPIOA) = frame_data[i] & 0x0F;
    GPIO_BRR(GPIOA) = (frame_data[i] & 0x30) << 2;
    GPIO_BRR(GPIOB) = (frame_data[i] & 0xC0) >> 6;

    for (j = 0; j < 5000; j++)	/* Wait a bit. */
      __asm__("nop");

    /* set all rows to off before we shift the shift register */
    clear_rows();

  }
}

void animate_startup()
{
  u8 frame[8];
  int startup_animation_run = 1;
  int i;

  /* initialize first frame */
  for(i=0; i<8; i++) {
    frame[i] = 1;
  }

  /* reset animation counter variables */
  frame_delay = 0;
  frame_count = 0;

  while (startup_animation_run) {
    render_frame(frame);

    frame_delay++;
    if (frame_delay >= 50) {
      frame_delay = 0;
      frame_count++;
      if(frame_count < 8) {
	for(i=0; i<8; i++) {
	  frame[i] <<= 1;
	}
      } else if (frame_count < 16) {
	for(i=0; i<8; i++) {
	  frame[i] = 0;
	}
	frame[frame_count - 8] = 0xFF;
      } else {
	startup_animation_run = 0;
      }
    }
  }
}

int main(void)
{
  //int i, j;

  clock_setup();
  gpio_setup();

  /* clear rows and columns */
  clear_rows();
  clear_cols();

  animate_startup();

  frame_count = 0;
  frame_delay = 0;

  while (1) {
    /* Render one single frame */
    render_frame(anim[frame_count]);

    /* Calculate next animation frame */
    frame_delay++;
    if(frame_delay >= 50) {
      frame_delay=0;
      frame_count++;
      if(frame_count >= anim_frames) {
	frame_count=0;
      }
    }
  }

  return 0;
}
