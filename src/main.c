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

unsigned char temp, i, unseeded,
  temp_x, temp_y, health_changed, score_changed;
unsigned int temp_int;

// game stuff

#define UP 0
#define DOWN 1
#define LEFT 2
#define RIGHT 3

#define EMPTY_TILE 0x00

#define LEFT_BORDER 0x10
#define RIGHT_BORDER 0xef
#define TOP_BORDER 0x20
#define BOTTOM_BORDER 0xbf

enum game_state {
                 Title,
                 Moving,
                 GameOver
} current_game_state;

#pragma bss-name(pop)
// should be in the regular 0x300 ram now

unsigned char irq_array[32];
unsigned char double_buffer[32];

#define EEL_MAX_SIZE 64
unsigned char eel_x[EEL_MAX_SIZE];
unsigned char eel_y[EEL_MAX_SIZE];
unsigned char eel_dir[EEL_MAX_SIZE];

unsigned char eel_length, eel_head, eel_tail,
  eel_frame_counter, eel_speed, eel_growth,
  eel_energy, eel_level_up;

unsigned int eel_health, eel_max_health, eel_score;

unsigned char current_direction;

unsigned char health_buffer[5];
unsigned char score_buffer[5];

#define MAX_PIRANHAS 8
unsigned char piranha_x[MAX_PIRANHAS];
unsigned char piranha_y[MAX_PIRANHAS];
unsigned char piranha_target_x[MAX_PIRANHAS];
unsigned char piranha_target_y[MAX_PIRANHAS];
enum {
      Swimming,
      GettingFood,
      Eating,
      Dead
} piranha_state[MAX_PIRANHAS];
unsigned char piranha_eating_index[MAX_PIRANHAS];
unsigned char piranha_count;
unsigned int piranha_spawn_timer;

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
    case Moving:
      moving();
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

void display_health (void) {
  health_buffer[0] = 0x40;
  temp_int = eel_health;
  while(temp_int >= 10000) {
    temp_int -= 10000;
    ++health_buffer[0];
  }
  health_buffer[1] = 0x40;
  while(temp_int >= 1000) {
    temp_int -= 1000;
    ++health_buffer[1];
  }
  health_buffer[2] = 0x40;
  while(temp_int >= 100) {
    temp_int -= 100;
    ++health_buffer[2];
  }
  health_buffer[3] = 0x40;
  while(temp_int >= 10) {
    temp_int -= 10;
    ++health_buffer[3];
  }
  health_buffer[4] = 0x40 + temp_int;
  multi_vram_buffer_horz(health_buffer, 5, NTADR_A(10, 26));
}

void display_score (void) {
  score_buffer[0] = 0x40;
  temp_int = eel_score;
  while(temp_int >= 10000) {
    temp_int -= 10000;
    ++score_buffer[0];
  }
  score_buffer[1] = 0x40;
  while(temp_int >= 1000) {
    temp_int -= 1000;
    ++score_buffer[1];
  }
  score_buffer[2] = 0x40;
  while(temp_int >= 100) {
    temp_int -= 100;
    ++score_buffer[2];
  }
  score_buffer[3] = 0x40;
  while(temp_int >= 10) {
    temp_int -= 10;
    ++score_buffer[3];
  }
  score_buffer[4] = 0x40 + temp_int;
  multi_vram_buffer_horz(score_buffer, 5, NTADR_A(10, 27));
}

void start_game (void) {
  ppu_off(); // screen off
  pal_bg(palette_bg); //	load the BG palette
  pal_spr(palette_spr); // load the sprite palette

  // draw some things
  vram_adr(NTADR_A(0,0));
  unrle(main_nametable);

  ppu_on_all();

  current_game_state = Moving;

  eel_length = 5;

  memfill(wram_array,0,0x2000);

  for(i = 0; i < 5; ++i) {
    eel_x[i] = i + 7;
    eel_y[i] = 15;
    eel_dir[i] = RIGHT;
    *(wram_array + 15 * 32 + (i + 7)) = 1;
  }

  for(i = 0; i < 32; ++i) {
    *(wram_array + 3 * 32 + i) = 1;
    *(wram_array + 24 * 32 + i) = 1;
  }

  for(i = 3; i < 24; ++i) {
    *(wram_array + i * 32 + 1) = 1;
    *(wram_array + i * 32 + 30) = 1;
  }

  eel_head = 4;
  eel_tail = 0;
  eel_speed = 4;
  eel_growth = 4;
  eel_frame_counter = 0;
  eel_energy = 64;
  eel_health = eel_max_health = 1000;
  display_health();
  eel_level_up = 0;
  current_direction = RIGHT;

  piranha_count = 0;
  piranha_spawn_timer = 120;
}

void erase_tail (unsigned char x, unsigned char y) {
  *(wram_array + y * 32 + x) = 0;
  one_vram_buffer(EMPTY_TILE, NTADR_A(x, y));
  for(i = 0; i < piranha_count; ++i) {
    if (piranha_eating_index[i] == eel_tail) {
      piranha_eating_index[i] = 0xff;
    }
  }
}

void draw_tail (unsigned char x, unsigned char y, unsigned char direction) {
  one_vram_buffer(tail_per_direction[direction], NTADR_A(x, y));
}

void erase_head (unsigned char x, unsigned char y, unsigned char old_direction, unsigned char direction) {
  one_vram_buffer(middle_tile[(old_direction << 2) | direction], NTADR_A(x, y));
}

void draw_head (unsigned char x, unsigned char y, unsigned char direction) {
  *(wram_array + y * 32 + x) = 1;
  one_vram_buffer(head_per_direction[direction], NTADR_A(x, y));
}

void move_eel (void) {
  eel_frame_counter += eel_speed;
  if (eel_frame_counter < 60) return;
  eel_frame_counter -= 60;

  temp_x = eel_x[eel_head];
  temp_y = eel_y[eel_head];

  switch (current_direction) {
  case UP:
    --temp_y;
    break;
  case DOWN:
    ++temp_y;
    break;
  case LEFT:
    --temp_x;
    break;
  case RIGHT:
    ++temp_x;
    break;
  }

  if (*(wram_array + temp_y * 32 + temp_x)) {
    return;
  }

  if (eel_growth > 0) {
    --eel_growth;
    ++eel_length;
  } else {
    erase_tail(eel_x[eel_tail], eel_y[eel_tail]);
    eel_tail = (eel_tail + 1) % EEL_MAX_SIZE;
    draw_tail(eel_x[eel_tail], eel_y[eel_tail], eel_dir[(eel_tail + 1) % EEL_MAX_SIZE]);
  }

  erase_head(eel_x[eel_head], eel_y[eel_head], eel_dir[eel_head], current_direction);
  eel_head = (eel_head + 1) % EEL_MAX_SIZE;
  eel_x[eel_head] = temp_x;
  eel_y[eel_head] = temp_y;
  eel_dir[eel_head] = current_direction;
  draw_head(temp_x, temp_y, current_direction);
}

void shock_attack (void) {
  if (eel_energy < 16) return;

  eel_energy -= 16;

  thunder();

  for(i = 0; i < piranha_count; ++i) {
    if (piranha_state[i] == Eating) {
      piranha_state[i] = Dead;
      piranha_y[i] += 8;
    }
  }
}

void handle_moving_input (void) {
  pad_poll(0);
  temp = get_pad_new(0);
  // TODO: buffer
  if (temp & PAD_UP) {
    current_direction = UP;
  } else if (temp & PAD_DOWN) {
    current_direction = DOWN;
  } else if (temp & PAD_LEFT) {
    current_direction = LEFT;
  } else if (temp & PAD_RIGHT) {
    current_direction = RIGHT;
  } else if (temp & PAD_A) {
    shock_attack();
  }
}

void piranha_retarget_random (unsigned char index) {
  do {
    temp_int = rand16();
    temp_x = temp_int;
    temp_y = temp_int >> 8;
  } while (temp_x < LEFT_BORDER || temp_x > RIGHT_BORDER ||
           temp_y < TOP_BORDER || temp_y > BOTTOM_BORDER);
  piranha_target_x[index] = temp_x;
  piranha_target_y[index] = temp_y;
}

void piranha_retarget_eel (unsigned char index) {
  temp = rand8();
  piranha_target_x[index] = eel_x[eel_head] << 3 | (temp & 0x7);
  piranha_target_y[index] = eel_y[eel_head] << 3 | ((temp >> 3) & 0x7);
}

void maybe_add_piranhas (void) {
  if (piranha_count == MAX_PIRANHAS) return;

  --piranha_spawn_timer;
  if (piranha_spawn_timer > 0) return;

  temp_int = rand16();

  piranha_spawn_timer = (temp_int & 0xff) + (temp_int >> 8) + 60;

  piranha_state[piranha_count] = Swimming;

  do {
    temp_int = rand16();
    temp_x = temp_int;
    temp_y = temp_int >> 8;
  } while (temp_x < LEFT_BORDER || temp_x > RIGHT_BORDER ||
           temp_y < TOP_BORDER || temp_y > BOTTOM_BORDER);
  piranha_x[piranha_count] = temp_x;
  piranha_y[piranha_count] = temp_y;

  piranha_retarget_random(piranha_count);

  ++piranha_count;
}

void move_piranha (unsigned char index) {
  if (piranha_x[index] < piranha_target_x[index]) {
    ++piranha_x[index];
  } else if (piranha_x[index] > piranha_target_x[index]) {
    --piranha_x[index];
  }
  if (piranha_y[index] < piranha_target_y[index]) {
    ++piranha_y[index];
  } else if (piranha_y[index] > piranha_target_y[index]) {
    --piranha_y[index];
  }
}

unsigned char piranha_on_eel (unsigned char index) {
  if (piranha_eating_index[index] != 0xff) return 1;

  temp = eel_tail;
  temp_x = piranha_x[index] >> 3;
  temp_y = piranha_y[index] >> 3;
  do {
    if (temp_x == eel_x[temp] &&
        temp_y == eel_y[temp]) {
      piranha_eating_index[index] = temp;
      return 1;
    }
    if (temp == eel_head) break;
    temp = (temp + 1) % EEL_MAX_SIZE;
  } while(1);
  return 0;
}

unsigned char piranha_on_eel_head (unsigned char index) {
  temp = eel_head;
  temp_x = eel_x[temp] << 3;
  temp_y = eel_y[temp] << 3;
  if (piranha_x[index] < temp_x - 4||
      piranha_x[index] > temp_x + 12 ||
      piranha_y[index] < temp_y - 4 ||
      piranha_y[index] > temp_y + 12) {
    return 0;
  }
  return 1;
}

void delete_piranha (unsigned char index) {
  --piranha_count;
  piranha_x[index] = piranha_x[piranha_count];
  piranha_y[index] = piranha_y[piranha_count];
  piranha_target_x[index] = piranha_target_x[piranha_count];
  piranha_target_y[index] = piranha_target_y[piranha_count];
  piranha_eating_index[index] = piranha_eating_index[piranha_count];
  piranha_state[index] = piranha_state[piranha_count];
}

void move_piranhas (void) {

  for(i = 0; i < piranha_count; ++i) {
    switch(piranha_state[i]) {
    case Swimming:
      move_piranha(i);
      if (piranha_x[i] == piranha_target_x[i] && piranha_y[i] == piranha_target_y[i]) {
        if (rand8() & 0x7) {
          piranha_retarget_random(i);
        } else {
          piranha_retarget_eel(i);
          piranha_state[i] = GettingFood;
        }
      }
      break;
    case GettingFood:
      if (piranha_x[i] == piranha_target_x[i] && piranha_y[i] == piranha_target_y[i]) {
        piranha_state[i] = Eating;
        piranha_eating_index[i] = 0xff;
      } else {
        move_piranha(i);
      }
      break;
    case Eating:
      if (piranha_on_eel(i)) {
        // TODO maybe eating noise
        if (eel_health > 0) {
          --eel_health;
          health_changed = 1;
        }
      } else {
        piranha_retarget_eel(i);
        piranha_state[i] = GettingFood;
      }
      break;
    case Dead:
      if (piranha_on_eel_head(i)) {
        delete_piranha(i);
        eel_score += 50;
        score_changed = 1;
        ++eel_level_up;
        if (eel_level_up == 5) {
          eel_level_up = 0;
          eel_max_health += 200;
          if (eel_speed < 24) ++eel_speed;
        }
        health_changed = 1;
        eel_health += 80;
        if (eel_health > eel_max_health) eel_health = eel_max_health;

        eel_energy += 16;
        if (eel_energy > 64) eel_energy = 64;

        if (eel_length + 2 < EEL_MAX_SIZE) eel_growth = 2;

        break;
      }
      temp = rand8();
      if (temp < 0x20) {
        ++piranha_y[i];
        if (piranha_y[i] > BOTTOM_BORDER) {
          delete_piranha(i);
          --i;
        }
      }
      break;
    }
  }
}

void check_eel_status (void) {
  if (eel_energy < 64 && rand8() < 32) ++eel_energy;
  temp_x = 4;
  for(i = 0; i < 4; ++i) {
    if (i * 16 > eel_energy) {
      temp = 0;
    } else {
      temp = eel_energy - i * 16;
      if (temp > 16) temp = 16;
      temp = temp >> 1;
    }
    one_vram_buffer(0x12 + temp, NTADR_A(temp_x, 26));
    one_vram_buffer(0x12 + temp, NTADR_A(temp_x, 27));
    ++temp_x;
  }

  if (health_changed) {
    display_health();
    health_changed = 0;
  }
  if (score_changed) {
    display_score();
    score_changed = 0;
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

void moving (void) {
  clear_vram_buffer();

  health_changed = 0;
  score_changed = 0;

  handle_moving_input();

  if (current_game_state != Moving) return;

  move_eel();

  maybe_add_piranhas();

  move_piranhas();

  check_eel_status();

  if (eel_health == 0) game_over();
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

  if (current_game_state != Moving) return;

  for(i = 0; i < piranha_count; ++i) {
    if (piranha_state[i] == Dead) {
      oam_meta_spr(piranha_x[i], piranha_y[i], DeadPiranhaSprite);
    } else {
      if (piranha_x[i] < piranha_target_x[i]) {
        oam_meta_spr(piranha_x[i], piranha_y[i], PiranhaSpriteR);
      } else {
        oam_meta_spr(piranha_x[i], piranha_y[i], PiranhaSpriteL);
      }
    }
  }
}
