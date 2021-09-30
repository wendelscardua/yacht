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
unsigned char straight_count, straight_missing;
unsigned char dice_sfx_counter;

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

unsigned char cursor, reroll_count, turns;

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

const unsigned char palette_bg[16]={ 0x1a,0x1a,0x29,0x3a,0x1a,0x30,0x00,0x0f,0x1a,0x07,0x28,0x0f,0x1a,0x38,0x28,0x29 };
const unsigned char palette_spr[16]={ 0x1a,0x1a,0x29,0x3a,0x1a,0x30,0x00,0x0f,0x1a,0x07,0x28,0x0f,0x1a,0x38,0x28,0x29 };

const unsigned char text_box_empty[] = {0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03};
const unsigned char text_box_press_a_to_roll[] = {0x21,0x1a,0x03,0x32,0x4f,0x4c,0x4c,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03};
const unsigned char text_box_press_a_to_reroll[] = {0x21,0x1a,0x03,0x32,0x45,0x52,0x4f,0x4c,0x4c,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03};
const unsigned char text_box_press_b_to_keep[] = {0x22,0x1a,0x03,0x2b,0x45,0x45,0x50,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03};
const unsigned char text_box_press_a_to_select[] = {0x21,0x1a,0x03,0x33,0x45,0x4c,0x45,0x43,0x54,0x03,0x43,0x41,0x54,0x45,0x47,0x4f,0x52,0x59,0x03,0x03,0x03,0x03};
const unsigned char text_box_dpad_cursor[]={0x24,0x50,0x41,0x44,0x1a,0x03,0x2d,0x4f,0x56,0x45,0x03,0x43,0x55,0x52,0x53,0x4f,0x52,0x03,0x03,0x03,0x03,0x03};
const unsigned char text_box_cpu_rolling[]={0x23,0x30,0x35,0x03,0x52,0x4f,0x4c,0x4c,0x49,0x4e,0x47,0x0e,0x0e,0x0e,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03};
const unsigned char text_box_cpu_thinking[]={0x23,0x30,0x35,0x03,0x54,0x48,0x49,0x4e,0x4b,0x49,0x4e,0x47,0x0e,0x0e,0x0e,0x03,0x03,0x03,0x03,0x03,0x03,0x03};
const unsigned char text_box_cpu_choosing[]={0x23,0x30,0x35,0x03,0x43,0x48,0x4f,0x4f,0x53,0x49,0x4e,0x47,0x0e,0x0e,0x0e,0x03,0x03,0x03,0x03,0x03,0x03,0x03};
const unsigned char text_box_player_wins[]={0x30,0x4c,0x41,0x59,0x45,0x52,0x03,0x57,0x49,0x4e,0x53,0x1b,0x1b,0x1b,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03};
const unsigned char text_box_cpu_wins[]={0x23,0x30,0x35,0x03,0x57,0x49,0x4e,0x53,0x1b,0x1b,0x1b,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03};
const unsigned char text_box_draw[]={0x29,0x54,0x3e,0x53,0x03,0x41,0x03,0x44,0x52,0x41,0x57,0x1b,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03};
const unsigned char text_box_press_start[]={0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x30,0x52,0x45,0x53,0x53,0x03,0x33,0x54,0x41,0x52,0x54};

void draw_sprites (void);

void roll_die (unsigned char index) {
  dice_roll_speed[index] = (rand8() % 16) + 45;
  dice_roll_counter[index] = 0;
}

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
    roll_die(i);
  }
  stop_dice = 1;
  reroll_count = 0;
}

void go_to_player_rolling_again (void) {
  current_game_state = PlayerRolling;
  multi_vram_buffer_horz(text_box_empty, 22, NTADR_A(4, 24));
  multi_vram_buffer_horz(text_box_empty, 22, NTADR_A(4, 25));
  stop_dice = 1;
  cursor = 0;
  ++reroll_count;
}

void go_to_cpu_rolling_again (void) {
  ppu_wait_nmi();
  clear_vram_buffer();
  current_game_state = CPURolling;
  multi_vram_buffer_horz(text_box_cpu_rolling, 22, NTADR_A(4, 24));
  multi_vram_buffer_horz(text_box_empty, 22, NTADR_A(4, 25));
  stop_dice = 1;
  cursor = 0;
  ++reroll_count;
}

void go_to_player_select_scoring (void) {
  current_game_state = PlayerSelectScoring;
  multi_vram_buffer_horz(text_box_press_a_to_select, 22, NTADR_A(4, 24));
  multi_vram_buffer_horz(text_box_dpad_cursor, 22, NTADR_A(4, 25));
  cursor = 0;
  while(player_score_locked[cursor]) ++cursor;
}

void go_to_cpu_rolling (void) {
  current_game_state = CPURolling;
  multi_vram_buffer_horz(text_box_cpu_rolling, 22, NTADR_A(4, 24));
  multi_vram_buffer_horz(text_box_empty, 22, NTADR_A(4, 25));
  for(i = 0; i < 5; ++i) {
    dice_roll_speed[i] = (rand8() % 16) + 45;
    dice_roll_counter[i] = 0;
  }
  stop_dice = 1;
  reroll_count = 0;
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

  dice_sfx_counter = 0;

  for(i = 0; i < 12; ++i) {
    player_score[i] = player_score_locked[i] =
      cpu_score[i] = cpu_score_locked[i] = 0;
  }

  player_points = cpu_points = turns = 0;

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
  unsigned char changed = 0;
  for(i = 0; i < 5; ++i) {
    if (dice_roll_speed[i]) {
      dice_roll_counter[i] += (dice_roll_speed[i] + 1) / 2;
      if (dice_roll_counter[i] >= 60) {
        dice_roll_counter[i] -= 60;

        do {
          dice[i] = rand8() & 7;
        } while (!dice[i] || dice[i] == 7);

        render_die(i);
        changed = 1;

        if (stop_dice) {
          --dice_roll_speed[i];
        }
      }
    }
  }
  if (dice_sfx_counter > 0) --dice_sfx_counter;
  if (changed) {
    if (dice_sfx_counter == 0) {
      sample_play(1);
      dice_sfx_counter = 15;
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
  unsigned char buffer[3];
  ppu_wait_nmi();
  clear_vram_buffer();
  temp_int = score;
  buffer[2] = 0x10;
  if (score >= 100) {
    buffer[0] = 0x10;
  } else {
    buffer[0] = 0x03;
  }
  if (score >= 10) {
    buffer[1] = 0x10;
  } else {
    buffer[1] = 0x03;
  }
  while(temp_int >= 100) {
    ++buffer[0];
    temp_int -= 100;
  }
  while(temp_int >= 10) {
    ++buffer[1];
    temp_int -= 10;
  }
  buffer[2] += temp_int;
  multi_vram_buffer_horz(buffer, 3, address);
}

void compute_available_scores (unsigned char *score, unsigned char *score_locked) {
  for(i = 1; i <= 6; ++i) {
    histo_dice[i] = 0;
  }

  for(i = 0; i < 5; ++i) {
    ++histo_dice[dice[i]];
  }

  if (score == player_score) {
    temp_x = 20;
  } else {
    temp_x = 25;
  }

  if (!score_locked[ONES]) {
    score[ONES] = histo_dice[1];
    display_score(score[ONES], NTADR_A(temp_x, ONES + 3));
  }
  if (!score_locked[TWOS]) {
    score[TWOS] = 2 * histo_dice[2];
    display_score(score[TWOS], NTADR_A(temp_x, TWOS + 3));
  }
  if (!score_locked[THREES]) {
    score[THREES] = 3 * histo_dice[3];
    display_score(score[THREES], NTADR_A(temp_x, THREES + 3));
  }
  if (!score_locked[FOURS]) {
    score[FOURS] = 4 * histo_dice[4];
    display_score(score[FOURS], NTADR_A(temp_x, FOURS + 3));
  }
  if (!score_locked[FIVES]) {
    score[FIVES] = 5 * histo_dice[5];
    display_score(score[FIVES], NTADR_A(temp_x, FIVES + 3));
  }
  if (!score_locked[SIXES]) {
    score[SIXES] = 6 * histo_dice[6];
    display_score(score[SIXES], NTADR_A(temp_x, SIXES + 3));
  }

  if (!score_locked[FULL_HOUSE]) {
    score[FULL_HOUSE] = 0;
    if ((histo_dice[1] == 3 || histo_dice[2] == 3 || histo_dice[3] == 3 || histo_dice[4] == 3 || histo_dice[5] == 3 || histo_dice[6] == 3) &&
        (histo_dice[1] == 2 || histo_dice[2] == 2 || histo_dice[3] == 2 || histo_dice[4] == 2 || histo_dice[5] == 2 || histo_dice[6] == 2)) {
      score[FULL_HOUSE] = dice[0] + dice[1] + dice[2] + dice[3] + dice[4];
    }
    display_score(score[FULL_HOUSE], NTADR_A(temp_x, FULL_HOUSE + 3));
  }

  if (!score_locked[FOUR_OF_A_KIND]) {
    score[FOUR_OF_A_KIND] = 0;
    for(i = 1; i <= 6; ++i) {
      if (histo_dice[i] >= 4) {
        score[FOUR_OF_A_KIND] = i * 4;
        break;
      }
    }
    display_score(score[FOUR_OF_A_KIND], NTADR_A(temp_x, FOUR_OF_A_KIND + 3));
  }

  straight_count = straight_missing = 0;

  for(i = 1; i <= 6; ++i) {
    if (histo_dice[i] == 0) {
      ++straight_count;
      if (straight_count == 2) break;
      straight_missing = i;
    }
  }

  if (!score_locked[LITTLE_STRAIGHT]) {
    if (straight_count == 1 && straight_missing == 6) {
      score[LITTLE_STRAIGHT] = 30;
    } else {
      score[LITTLE_STRAIGHT] = 0;
    }
    display_score(score[LITTLE_STRAIGHT], NTADR_A(temp_x, LITTLE_STRAIGHT + 3));
  }

  if (!score_locked[BIG_STRAIGHT]) {
    if (straight_count == 1 && straight_missing == 1) {
      score[BIG_STRAIGHT] = 30;
    } else {
      score[BIG_STRAIGHT] = 0;
    }
    display_score(score[BIG_STRAIGHT], NTADR_A(temp_x, BIG_STRAIGHT + 3));
  }

  if (!score_locked[CHOICE]) {
    score[CHOICE] = 0;
    for(i = 0; i < 5; ++i) {
      score[CHOICE] += dice[i];
    }
    display_score(score[CHOICE], NTADR_A(temp_x, CHOICE + 3));
  }

  if (!score_locked[YACHT]) {
    score[YACHT] = 0;
    for(i = 1; i <= 6; ++i) {
      if (histo_dice[i] == 5) {
        score[YACHT] = 50;
        break;
      }
    }
    display_score(score[YACHT], NTADR_A(temp_x, YACHT + 3));
  }
}

void go_to_player_may_reroll (void) {
  current_game_state = PlayerMayReroll;
  stop_dice = 0;
  cursor = 0;
  multi_vram_buffer_horz(text_box_press_a_to_reroll, 22, NTADR_A(4, 24));
  multi_vram_buffer_horz(text_box_press_b_to_keep, 22, NTADR_A(4, 25));
}

void go_to_cpu_select_scoring (void) {
  ppu_wait_nmi();
  clear_vram_buffer();
  current_game_state = CPUSelectScoring;
  cursor = 0;
  while(cpu_score_locked[cursor]) ++cursor;
  multi_vram_buffer_horz(text_box_cpu_choosing, 22, NTADR_A(4, 24));
  multi_vram_buffer_horz(text_box_empty, 22, NTADR_A(4, 25));
  temp_y = 0;
}

void go_to_cpu_thinking (void) {
  current_game_state = CPUThinking;
  multi_vram_buffer_horz(text_box_cpu_thinking, 22, NTADR_A(4, 24));
  multi_vram_buffer_horz(text_box_empty, 22, NTADR_A(4, 25));
  temp_int = 120 + (rand8() & 127) - (reroll_count * 32);
}

void go_to_game_end (void) {
  current_game_state = GameEnd;
  if (player_points > cpu_points) {
    multi_vram_buffer_horz(text_box_player_wins, 22, NTADR_A(4, 24));
  } else if (player_points < cpu_points) {
    multi_vram_buffer_horz(text_box_cpu_wins, 22, NTADR_A(4, 24));
  } else {
    multi_vram_buffer_horz(text_box_draw, 22, NTADR_A(4, 24));
  }
  multi_vram_buffer_horz(text_box_press_start, 22, NTADR_A(4, 25));
}

void cpu_ai (void) {
  temp_y = 0;
  temp_x = CHOICE;
  for(i = 0; i < 12; i++) {
    if (cpu_score_locked[i] || i == CHOICE) continue;
    if (cpu_score[i] >= temp_y) {
      temp_y = cpu_score[i];
      temp_x = i;
    }
  }
  if (reroll_count == 2) {
    go_to_cpu_select_scoring(); // temp_x = target category for cpu cursor
  } else {
    // simple AI
    switch(temp_x) {
    case ONES:
    case TWOS:
    case THREES:
    case FOURS:
    case FIVES:
    case SIXES:
      if (histo_dice[temp_x + 1] == 5) {
        go_to_cpu_select_scoring();
      } else {
        for(i = 0; i < 5; i++) {
          if (dice[i] != temp_x + 1) roll_die(i);
        }
        go_to_cpu_rolling_again();
      }
      break;
    case FULL_HOUSE:
      go_to_cpu_select_scoring();
      break;
    case FOUR_OF_A_KIND:
      for(i = 1; i <= 6; i++) {
        if (histo_dice[i] >= 4) break;
      }
      if (i == 7) {
        for(i = 0; i < 5; ++i) roll_die(i);
        go_to_cpu_rolling_again();
      } else if (histo_dice[i] == 5) {
        go_to_cpu_select_scoring();
      } else {
        temp = i;
        for(i = 0; i < 5; ++i) {
          if (dice[i] != temp) roll_die(i);
        }
        go_to_cpu_rolling_again();
      }
      break;
    case LITTLE_STRAIGHT:
    case BIG_STRAIGHT:
      if (cpu_score[temp_x]) {
        go_to_cpu_select_scoring();
      } else {
        for(i = 0; i < 5; ++i) roll_die(i);
        go_to_cpu_rolling_again();
      }
      break;
    case CHOICE:
      temp = 0;
      for(i = 0; i < 5; ++i) {
        if (dice[i] <= 3) {
          roll_die(i);
          ++temp;
        }
      }
      if (temp) {
        go_to_cpu_rolling_again();
      } else {
        go_to_cpu_select_scoring();
      }
      break;
    case YACHT:
      go_to_cpu_select_scoring();
      break;
    }
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
        if (reroll_count == 2) {
          go_to_player_select_scoring();
        } else {
          go_to_player_may_reroll();
        }
      }
      break;
    case PlayerMayReroll:
      rolling_dice();
      if (get_pad_new(0) & PAD_A) {
        roll_die(cursor);
        ++cursor;
      } else if (get_pad_new(0) & PAD_B) {
        ++cursor;
      }
      if (cursor == 5) {
        for(i = 0; i < 5; ++i) {
          if (dice_roll_speed[i]) break;
        }
        if (i == 5) {
          go_to_player_select_scoring();
        } else {
          go_to_player_rolling_again();
        }
      }

      break;
    case PlayerSelectScoring:
      if (get_pad_new(0) & PAD_UP) {
        do {
          cursor = (cursor + 11) % 12;
        } while(player_score_locked[cursor]);
      } else if (get_pad_new(0) & PAD_DOWN) {
        do {
          cursor = (cursor + 1) % 12;
        } while(player_score_locked[cursor]);
      } else if (get_pad_new(0) & PAD_A) {
        player_score_locked[cursor] = 1;
        player_points += player_score[cursor];
        display_score(player_points, NTADR_A(20, 16));
        one_vram_buffer(0x91, NTADR_A(23, 3 + cursor));
        for(i = 0; i < 12; i++) {
          if (!player_score_locked[i]) {
            multi_vram_buffer_horz(text_box_empty, 3, NTADR_A(20, 3 + i));
          }
        }
        ppu_wait_nmi();
        clear_vram_buffer();
        go_to_cpu_rolling();
      }
      break;
    case CPURolling:
      rolling_dice();
      for(i = 0; i < 5; ++i) {
        if (dice_roll_speed[i]) break;
      }
      if (i == 5) {
        compute_available_scores(cpu_score, cpu_score_locked);
        go_to_cpu_thinking();
      }
      break;
    case CPUThinking:
      --temp_int;
      if (temp_int == 0) {
        cpu_ai();
      }
      break;
    case CPUSelectScoring:
      ++temp_y;
      if (temp_y > 30 || (temp_y > 15 && temp_x - cursor > 4)) {
        temp_y = 0;
        if (cursor == temp_x) {
          cpu_score_locked[cursor] = 1;
          cpu_points += cpu_score[cursor];
          display_score(cpu_points, NTADR_A(25, 16));
          one_vram_buffer(0x91, NTADR_A(28, 3 + cursor));
          for(i = 0; i < 12; i++) {
            if (!cpu_score_locked[i]) {
              multi_vram_buffer_horz(text_box_empty, 3, NTADR_A(25, 3 + i));
            }
          }
          ppu_wait_nmi();
          clear_vram_buffer();
          turns++;
          if (turns == 12) {
            go_to_game_end();
          } else {
            go_to_player_wait_roll();
          }
        } else {
          do {
            cursor = (cursor + 1) % 12;
          } while(cpu_score_locked[cursor]);
        }
      }
      break;
    case GameEnd:
      if (get_pad_new(0) & PAD_START) {
        go_to_title();
      }
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

  if (current_game_state == PlayerMayReroll) {
    oam_spr(0x24 + 0x20 * cursor, 0xb6, 0x90, 0x1);
  } else if (current_game_state == PlayerSelectScoring) {
    oam_spr(0xb8, 0x17 + 0x8 * cursor, 0x92, 0x3);
  } else if (current_game_state == CPUSelectScoring) {
    oam_spr(0xe0, 0x17 + 0x8 * cursor, 0x92, 0x3);
  }
}
