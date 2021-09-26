/* Based on ...
 *  example of MMC3 for cc65
 *	Doug Fraker 2019
 */

#include "lib/neslib.h"
#include "lib/nesdoug.h"
#include "lib/unrle.h"
#include "mmc3/mmc3_code.h"
#include "mmc3/mmc3_code.c"
#include "sprites.h"
#include "../assets/nametables.h"

#pragma bss-name(push, "ZEROPAGE")

// GLOBAL VARIABLES
unsigned char arg1;
unsigned char arg2;
unsigned char pad1;
unsigned char pad1_new;
unsigned char double_buffer_index;

unsigned char temp, i, unseeded;
unsigned int temp_int;

enum game_state {
                 Title,
                 PlayerWaitRoll,
                 PlayerRolling,
                 PlayerMayReroll,
                 PlayerSelectScoring,
                 CPURolling,
                 CPUThinking,
                 CPUSelectScoring,
                 GameEnd
} current_game_state;

#pragma bss-name(pop)
// should be in the regular 0x300 ram now

unsigned char irq_array[32];
unsigned char double_buffer[32];

#pragma bss-name(push, "XRAM")
// extra RAM at $6000-$7fff
unsigned char wram_array[0x2000];

#pragma bss-name(pop)

// the fixed bank

#pragma rodata-name ("RODATA")
#pragma code-name ("CODE")

const unsigned char palette_bg[] = {
                                    0x02,0x1a,0x2a,0x3a,0x02,0x07,0x17,0x3c,0x02,0x07,0x28,0x0f,0x02,0x09,0x19,0x29
};

const unsigned char palette_spr[]={
                                   0x02,0x1a,0x2a,0x3a,0x02,0x07,0x17,0x26,0x02,0x06,0x16,0x26,0x02,0x09,0x19,0x29
};

const unsigned char palette_thunder[] = {
                                         0x02,0x28,0x38,0x30,0x02,0x07,0x17,0x3c,0x02,0x07,0x28,0x0f,0x02,0x09,0x19,0x29
};

const unsigned char head_per_direction[] = { 0x03, 0x04, 0x05, 0x06 };
const unsigned char tail_per_direction[] = { 0x0d, 0x0e, 0x0f, 0x10 };
const unsigned char middle_tile[] = {
                                     0x07, 0x07, 0x0a, 0x09,
                                     0x07, 0x07, 0x0b, 0x0c,
                                     0x0c, 0x09, 0x08, 0x08,
                                     0x0b, 0x0a, 0x08, 0x08
};

const unsigned char over_string[] = {0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77};
const unsigned char start_string[] = {0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67};

void draw_sprites (void);
void go_to_title (void);
void start_game (void);
void moving (void);
void game_over (void);

void thunder (void) {
  sfx_play(0, 0);

  for(i = 0; i < 12; i++) {
    ppu_wait_nmi();
    pal_bg(palette_thunder);
    ppu_wait_nmi();
    ppu_wait_nmi();
    ppu_wait_nmi();
    ppu_wait_nmi();
    pal_bg(palette_bg);
  }
}

void main (void) {
  set_mirroring(MIRROR_HORIZONTAL);
  bank_spr(1);
  irq_array[0] = 0xff; // end of data
  set_irq_ptr(irq_array); // point to this array

  // clear the WRAM, not done by the init code
  // memfill(void *dst,unsigned char value,unsigned int len);
  memfill(wram_array,0,0x2000);

  ppu_off(); // screen off
  pal_bg(palette_bg); //	load the BG palette
  pal_spr(palette_spr); // load the sprite palette
  // load red alpha and drawing as bg chars
  // and unused as sprites
  set_chr_mode_2(0);
  set_chr_mode_3(1);
  set_chr_mode_4(2);
  set_chr_mode_5(3);
  set_chr_mode_0(4);
  set_chr_mode_1(6);

  go_to_title();

  unseeded = 1;


  set_vram_buffer();
  clear_vram_buffer();

  while (1){ // infinite loop
    ppu_wait_nmi();
    rand16();

    switch (current_game_state) {
    case Title:
      do {
        ppu_wait_nmi();
        pad_poll(0);
      } while((get_pad_new(0) & PAD_START) == 0);
      thunder();
      if (unseeded) {
        seed_rng();
        unseeded = 0;
      }
      start_game();
      break;
    }

    // load the irq array with values it parse
    // ! CHANGED it, double buffered so we aren't editing the same
    // array that the irq system is reading from

    double_buffer_index = 0;

    // populate double buffer here

    double_buffer[double_buffer_index++] = 0xff; // end of data

    draw_sprites();

    // wait till the irq system is done before changing it
    // this could waste a lot of CPU time, so we do it last
    while(!is_irq_done() ){ // have we reached the 0xff, end of data
      // is_irq_done() returns zero if not done
      // do nothing while we wait
    }

    // copy from double_buffer to the irq_array
    // memcpy(void *dst,void *src,unsigned int len);
    memcpy(irq_array, double_buffer, sizeof(irq_array));
  }
}

void start_game (void) {
  ppu_off(); // screen off
  pal_bg(palette_bg); //	load the BG palette
  pal_spr(palette_spr); // load the sprite palette

  // draw some things
  vram_adr(NTADR_A(0,0));
  unrle(main_nametable);

  ppu_on_all();

  memfill(wram_array,0,0x2000);

  current_game_state = PlayerWaitRoll;
}

void go_to_title (void) {
  ppu_off(); // screen off
  // draw some things
  vram_adr(NTADR_A(0,0));
  unrle(title_nametable);
  music_play(0);
  draw_sprites();
  ppu_on_all(); //	turn on screen
  current_game_state = Title;
}

void game_over (void) {
  multi_vram_buffer_horz(over_string, 8, NTADR_A(12, 12));
  multi_vram_buffer_horz(start_string, 8, NTADR_A(12, 13));

  do {
    ppu_wait_nmi();
    pad_poll(0);
  } while((get_pad_new(0) & PAD_START) == 0);

  clear_vram_buffer();
  go_to_title();
}

void draw_sprites (void) {
  oam_clear();
}
