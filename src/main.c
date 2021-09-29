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

unsigned char temp, i, unseeded, temp_x, temp_y;
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

unsigned char dice[5];
unsigned char dice_roll_speed[5];
unsigned char dice_roll_counter[5];
unsigned char stop_dice;
unsigned char histo_dice[7];

#define ONES 0
#define TWOS 1
#define THREES 2
#define FOURS 3
#define FIVES 4
#define SIXES 5
#define FULL_HOUSE 6
#define FOUR_OF_A_KIND 7
#define LITTLE_STRAIGHT 8
#define BIG_STRAIGHT 9
#define CHOICE 10
#define YACHT 11

unsigned char player_score[12];
unsigned char player_score_locked[12];
unsigned char cpu_score[12];
unsigned char cpu_score_locked[12];
unsigned int player_points;
unsigned int cpu_points;

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

const unsigned char palette_bg[16]={ 0x1a,0x1a,0x29,0x3a,0x1a,0x30,0x00,0x0f,0x1a,0x07,0x28,0x0f,0x1a,0x09,0x19,0x29 };
const unsigned char palette_spr[16]={ 0x1a,0x1a,0x29,0x3a,0x1a,0x30,0x00,0x0f,0x1a,0x07,0x28,0x0f,0x1a,0x09,0x19,0x29 };

void draw_sprites (void);

const unsigned char text_box_empty[] = {0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03};
const unsigned char text_box_press_a_to_roll[] = {0x21,0x1a,0x03,0x32,0x4f,0x4c,0x4c,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03};

void go_to_player_wait_roll (void) {
  current_game_state = PlayerWaitRoll;
  multi_vram_buffer_horz(text_box_press_a_to_roll, 22, NTADR_A(4, 24));
  multi_vram_buffer_horz(text_box_empty, 22, NTADR_A(4, 25));
}

void go_to_player_rolling (void) {
  current_game_state = PlayerRolling;
  multi_vram_buffer_horz(text_box_empty, 22, NTADR_A(4, 24));
  multi_vram_buffer_horz(text_box_empty, 22, NTADR_A(4, 25));
  for(i = 0; i < 5; ++i) {
    dice_roll_speed[i] = (rand8() % 16) + 45;
    dice_roll_counter[i] = 0;
  }
  stop_dice = 1;
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

  for(i = 0; i < 5; ++i) {
    dice[i] = i + 1;
    dice_roll_speed[i] = 0;
  }

  for(i = 0; i < 12; ++i) {
    player_score[i] = player_score_locked[i] =
      cpu_score[i] = cpu_score_locked[i] = 0;
  }

  player_points = cpu_points = 0;

  go_to_player_wait_roll();
}

void render_die (unsigned char index) {
  temp_y = 20;
  temp_x = 4 + 4 * index;
  temp = dice[index] - 1;
  one_vram_buffer(0x60 + 2 * temp, NTADR_A(temp_x, temp_y));
  one_vram_buffer(0x61 + 2 * temp, NTADR_A(temp_x+1, temp_y));
  one_vram_buffer(0x70 + 2 * temp, NTADR_A(temp_x, temp_y+1));
  one_vram_buffer(0x71 + 2 * temp, NTADR_A(temp_x+1, temp_y+1));
  one_vram_buffer(0x80 + 2 * temp, NTADR_A(temp_x, temp_y+2));
  one_vram_buffer(0x81 + 2 * temp, NTADR_A(temp_x+1, temp_y+2));
}

void rolling_dice (void) {
  for(i = 0; i < 5; ++i) {
    if (dice_roll_speed[i]) {
      dice_roll_counter[i] += (dice_roll_speed[i] + 1) / 2;
      if (dice_roll_counter[i] >= 60) {
        dice_roll_counter[i] -= 60;

        do {
          dice[i] = rand8() & 7;
        } while (!dice[i] || dice[i] == 7);

        render_die(i);

        if (stop_dice) {
          --dice_roll_speed[i];
        }
      }
    }
  }
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

void display_score (unsigned int score, unsigned int address) {
  // TODO
}

void compute_available_scores (unsigned char *score, unsigned char *score_locked) {
  for(i = 1; i <= 6; ++i) {
    histo_dice[i] = 0;
  }

  for(i = 0; i < 5; ++i) {
    ++histo_dice[dice[i]];
  }

  if (!score_locked[ONES]) {
    score[ONES] = histo_dice[1];
    display_score(score[ONES], NTADR_A(20, 3));
  }
  if (!score_locked[TWOS]) {
    score[TWOS] = 2 * histo_dice[2];
    display_score(score[TWOS], NTADR_A(20, 4));
  }
  if (!score_locked[THREES]) {
    score[THREES] = 3 * histo_dice[3];
    display_score(score[THREES], NTADR_A(20, 5));
  }
  if (!score_locked[FOURS]) {
    score[FOURS] = 4 * histo_dice[4];
    display_score(score[FOURS], NTADR_A(20, 6));
  }
  if (!score_locked[FIVES]) {
    score[FIVES] = 5 * histo_dice[5];
    display_score(score[FIVES], NTADR_A(20, 7));
  }
  if (!score_locked[SIXES]) {
    score[SIXES] = 6 * histo_dice[6];
    display_score(score[SIXES], NTADR_A(20, 8));
  }

  // TODO continue
}

void go_to_player_may_reroll (void) {
  // TODO
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
    clear_vram_buffer();
    pad_poll(0);
    rand16();

    switch (current_game_state) {
    case Title:
      if (get_pad_new(0) & PAD_START) {
        if (unseeded) {
          seed_rng();
          unseeded = 0;
        }
        start_game();
      }
      break;
    case PlayerWaitRoll:
      if (get_pad_new(0) & PAD_A) {
        go_to_player_rolling();
      }
      break;
    case PlayerRolling:
      rolling_dice();
      for(i = 0; i < 5; ++i) {
        if (dice_roll_speed[i]) break;
      }
      if (i == 5) {
        compute_available_scores(player_score, player_score_locked);
        go_to_player_may_reroll();
      }
      break;
    case PlayerMayReroll:
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

void draw_sprites (void) {
  oam_clear();
}
