#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "SMSlib/src/SMSlib.h"
#include "PSGlib/src/PSGlib.h"
#include "gfx.h"

#define ACTOR_MIN_X 8
#define ACTOR_MAX_X (256 - 8)
#define ACTOR_FRAME_TILE 4
#define ACTOR_DIR_TILE 8
#define ACTOR_TILE_SHIFT 4

#define LANE_COUNT 7
#define LANE_CHAR_HEIGHT 3
#define LANE_TOP 1
#define LANE_BOTTOM (LANE_TOP + LANE_COUNT * LANE_CHAR_HEIGHT)
#define LANE_PIXEL_TOP_LIMIT (LANE_TOP * 8 + 8)
#define LANE_PIXEL_HEIGHT (LANE_CHAR_HEIGHT * 8)
#define LANE_BASE_TILE 0xB0

#define PLAYER_MIN_Y LANE_PIXEL_TOP_LIMIT
#define PLAYER_CENTER_X 128
#define PLAYER_WIDTH 16
#define PLAYER_HEIGHT 16
#define PLAYER_BASE_TILE 16
#define PLAYER_DEATH_BASE_TILE 4
#define PLAYER_DEATH_FRAMES 2
#define PLAYER_VERTICAL_SPEED 12

#define SHOT_FLAG_LEFT 1
#define SHOT_FLAG_RIGHT 2
#define SHOT_BASE_TILE 32

#define ENEMY_BASE_TILE 48
#define ENEMY_TYPE_MASK 7
#define ENEMY_TYPE_SLOW 0
#define ENEMY_TYPE_MEDIUM 1
#define ENEMY_TYPE_FAST 2
#define ENEMY_TYPE_PELLET 3
#define ENEMY_TYPE_BALL 4
#define ENEMY_TYPE_ARROW 5
#define ENEMY_TYPE_TANK 6
#define ENEMY_TYPE_PHANTOM 7

#define SCORE_BASE_TILE 0xB6
#define SCORE_TOP 22
#define SCORE_LEFT 13
#define SCORE_DIGITS 6

#define LIFE_BASE_TILE 0xCA

#define STATE_DEMO 1
#define STATE_GAME_START 2
#define STATE_NEXT_STAGE 3
#define STATE_PLAY 4

typedef struct _shot {
  unsigned int x;
  unsigned char flag;
} shot;

typedef struct _enemy {
  unsigned int x;
  unsigned char type;
  char spd, timer;
} enemy;

const unsigned char lane_coords[] = { 0, LANE_PIXEL_HEIGHT, 2 * LANE_PIXEL_HEIGHT, 3 * LANE_PIXEL_HEIGHT, 4 * LANE_PIXEL_HEIGHT, 5 * LANE_PIXEL_HEIGHT, 6 * LANE_PIXEL_HEIGHT };

int joy;
unsigned int frame_timer;

unsigned char player_x, player_y, player_frame;
unsigned char player_fire_delay;
unsigned char player_current_lane, player_target_lane;
unsigned char player_target_y;
unsigned char player_looking_left;
bool player_dead;
bool player_invincible;

unsigned char score_digits[SCORE_DIGITS];
unsigned int score_tiles[2][SCORE_DIGITS];
bool score_needs_update;
bool score_enabled;

unsigned char lives;
bool lives_need_update;

unsigned char current_game_state;
unsigned char next_game_state;

bool sound_enabled;

unsigned char level_number;
char remaining_enemies;

shot shots[LANE_COUNT];
enemy enemies[LANE_COUNT];

// Former local variables converted to globals to see if it speeds up things
unsigned char g_i, g_x, g_y, g_lane;
unsigned char *sc_p;
unsigned int *st_p, *st_p2;
shot *shot_p;
enemy *enm_p;

void add_double_sprite(unsigned char x, unsigned char y, unsigned char tile) {
  SMS_addSprite(x - 8, y, tile);
  SMS_addSprite(x, y, tile + 2);
}

void draw_ship(unsigned char x, unsigned char y, unsigned char base_tile, unsigned char facing_right) {
  if (frame_timer & 0x10) {
    base_tile += ACTOR_FRAME_TILE;
  }

  if (facing_right) {
    base_tile += ACTOR_DIR_TILE;
  }

  add_double_sprite(x, y, base_tile);
}

void draw_player_ship() {
  draw_ship(player_x, player_y, PLAYER_BASE_TILE, !player_looking_left);
}

void load_ingame_tiles() {
  SMS_loadTiles(ship_til, 0, ship_til_size);
}

void generate_lane(unsigned int *buffer) {
  unsigned char i;
  unsigned int *p, *p2;

  p = buffer;
  p2 = p + 31;

  // Creates the gradient for the lanes (yes, I'm STILL too lazy to make a map.)

  for (i = 0; i != 5; i++) {
    *p = LANE_BASE_TILE;
    *p2 = LANE_BASE_TILE;
    p++; p2--;
  }

  for (i = 4; i != 9; i++) {
    *p = LANE_BASE_TILE + 1;
    *p2 = LANE_BASE_TILE + 1;
    p++; p2--;
  }

  for (i = 9; i != 16; i++) {
    *p = LANE_BASE_TILE + 2;
    *p2 = LANE_BASE_TILE + 2;
    p++; p2--;
  }
}

void draw_lanes() {
  unsigned char i;
  unsigned int y;
  unsigned int buffer[32];

  generate_lane(buffer);

  // Draws the topmost/bottommost dividers

  SMS_loadTileMap(0, LANE_TOP, buffer, 64);
  SMS_loadTileMap(0, LANE_BOTTOM, buffer, 64);

  // Opens a hole in the middle

  buffer[14] = LANE_BASE_TILE + 3;
  buffer[15] = 0;
  buffer[16] = 0;
  buffer[17] = LANE_BASE_TILE + 4;

  // Draws remaining dividers

  y = LANE_TOP + LANE_CHAR_HEIGHT;
  for (i = 2; i != (LANE_COUNT + 1); i++) {
    SMS_loadTileMap(0, y, buffer, 64);
    y += LANE_CHAR_HEIGHT;
  }
}

void change_lane() {
  player_target_y = lane_coords[player_target_lane] + LANE_PIXEL_TOP_LIMIT;
}

void init_shots() {
  unsigned char i;
  shot *p;

  for (i = 0, p = shots; i != LANE_COUNT; i++, p++) {
    p->flag = 0;
  }
}

void draw_shot() {
  if (shot_p->flag) {
    draw_ship(shot_p->x, g_y, SHOT_BASE_TILE, shot_p->flag == SHOT_FLAG_RIGHT);
  }
}

void draw_shots() {
  for (g_i = 0, shot_p = shots, g_y = LANE_PIXEL_TOP_LIMIT; g_i != LANE_COUNT; g_i++, shot_p++, g_y += LANE_PIXEL_HEIGHT) {
    draw_shot();
  }
}

void move_shot() {
  if (shot_p->flag) {
    if (shot_p->flag == SHOT_FLAG_LEFT) {
      shot_p->x -= 6;
    } else {
      shot_p->x += 6;
    }

    if (shot_p->x <= ACTOR_MIN_X || shot_p->x >= ACTOR_MAX_X) {
      shot_p->flag = 0;
    }
  }
}

void move_shots() {
  for (g_i = 0, shot_p = shots; g_i != LANE_COUNT; g_i++, shot_p++) {
    move_shot();
  }
}

void fire() {
  shot *p = shots + player_current_lane;
  if (!p->flag) {
    p->x = player_x;
    p->flag = player_looking_left ? SHOT_FLAG_LEFT : SHOT_FLAG_RIGHT;

     PSGPlayNoRepeat(player_shot_psg);
  }
}

void init_enemies() {
  unsigned int i;
  enemy *p;

  for (i = 0, p = enemies; i != LANE_COUNT; i++, p++) {
    p->spd = 0;
  }
}

void draw_enemy() {
  if (enm_p->spd) {
    draw_ship(enm_p->x, g_y, ENEMY_BASE_TILE + (enm_p->type << ACTOR_TILE_SHIFT), enm_p->spd > 0);
  }
}

void draw_enemies() {
  for (g_i = 0, enm_p = enemies, g_y = LANE_PIXEL_TOP_LIMIT; g_i != LANE_COUNT; g_i++, enm_p++, g_y += LANE_PIXEL_HEIGHT) {
    draw_enemy();
  }
}

void stop_sound() {
  PSGStop();
  PSGSFXStop();
}

void sound_frame() {
  if (!sound_enabled) {
    stop_sound();
    return;
  }

  PSGFrame();
  PSGSFXFrame();
}

void wait_frame() {
  SMS_waitForVBlank();
  sound_frame();
}

void wait_frames(int count) {
    for (; count; count--) {
      wait_frame();
    }
}

void draw_player_death_frame(unsigned char frame) {
  SMS_initSprites();

  draw_ship(player_x, player_y, PLAYER_DEATH_BASE_TILE + (frame << 2), 0);

  SMS_finalizeSprites();
  SMS_waitForVBlank();
  SMS_copySpritestoSAT();

  sound_frame();

  wait_frames(18);
}

void change_life_counter(unsigned char next_value) {
  lives = next_value;
  lives_need_update = true;
}

void kill_player() {
  unsigned char i;

  if (player_invincible) {
    return;
  }

  stop_sound();
  PSGPlayNoRepeat(player_death_psg);

  init_enemies();
  init_shots();
  player_looking_left = true;

  for (i = 0; i != PLAYER_DEATH_FRAMES; i++) {
    draw_player_death_frame(i);
  }

  wait_frames(18);

  for (i = PLAYER_DEATH_FRAMES; i != 0; i--) {
    draw_player_death_frame(i - 1);
  }

  player_x = PLAYER_CENTER_X;
  player_dead = false;

  change_life_counter(lives - 1);
}

void prepare_score() {
  if (!score_needs_update) {
    return;
  }

  st_p = score_tiles[0];
  st_p2 = score_tiles[1];
  for (g_i = 0, sc_p = score_digits; g_i != SCORE_DIGITS; g_i++, sc_p++, st_p++, st_p2++) {
    *st_p = (*sc_p << 1) + SCORE_BASE_TILE;
    *st_p2 = *st_p + 1;
  }
}

void increase_score(unsigned int how_much) {
  sc_p = score_digits + SCORE_DIGITS - 2;

  if (!score_enabled) {
    return;
  }

  while (how_much) {
    *sc_p += how_much;
    how_much = 0;

    while (*sc_p > 9) {
      *sc_p -= 10;
      how_much++;
    }

    sc_p--;
  }

  score_needs_update = true;
}

void init_score() {
  unsigned char i;
  unsigned char *sc_p;

  for (i = 0, sc_p = score_digits; i != SCORE_DIGITS; i++, sc_p++) {
    *sc_p = 0;
  }

  score_needs_update = true;
  prepare_score();
}

void draw_score() {
  if (!score_needs_update) {
    return;
  }

  SMS_loadTileMapArea(SCORE_LEFT, SCORE_TOP, *score_tiles, SCORE_DIGITS, 2);
  score_needs_update = false;
}

void kill_enemy() {
  enm_p->spd = 0;
  increase_score(1);
  PSGSFXPlay(enemy_death_psg, SFX_CHANNEL2 | SFX_CHANNEL3);
  remaining_enemies--;
}

void spawn_enemy(unsigned char type, bool from_left) {
  if (from_left) {
    enm_p->x = ACTOR_MIN_X + 1;
    enm_p->spd = 12;
  } else {
    enm_p->x = ACTOR_MAX_X - 1;
    enm_p->spd = -12;
  }
  enm_p->type = type;
}

void collide_enemy() {
  shot_p = shots + g_lane;

  if (g_lane == player_current_lane && enm_p->x > player_x - 8 && enm_p->x < player_x + 8) {
    if (enm_p->type == ENEMY_TYPE_PELLET) {
      // Pellets act as bonuses
      kill_enemy();
      spawn_enemy(ENEMY_TYPE_PHANTOM, enm_p->x > PLAYER_CENTER_X);
    } else {
      // Other enemies are lethal.
      player_dead = true;
    }
    return;
  }

  if (enm_p->type == ENEMY_TYPE_PHANTOM || enm_p->type == ENEMY_TYPE_PELLET) {
    // Phantom ships and pellets are invulnerable
    return;
  }

  if (shot_p->flag) {
    if (shot_p->x - enm_p->x <= 16 || enm_p->x - shot_p->x <= 16) {
      if (enm_p->type == ENEMY_TYPE_TANK) {
        // Tanks can only be hit from the back
        if (enm_p->spd < 0) {
          if (shot_p->flag == SHOT_FLAG_LEFT) {
            // Both facing left? Blam!
            kill_enemy();
          }
        } else {
          if (shot_p->flag == SHOT_FLAG_RIGHT) {
            // Both facing right? Blam!
            kill_enemy();
          }
        }
      } else {
        // Other enemies will simply be dead.
        kill_enemy();
      }

      // Removes the shot
      shot_p->flag = 0;
    }
  }
}

void move_enemies() {
  for (g_lane = 0, enm_p = enemies; g_lane != LANE_COUNT; g_lane++, enm_p++) {
    if (!enm_p->spd) {
      if ((rand() & 0x1F) == 1) {
        // Spawns a new enemy
        spawn_enemy(rand() & ENEMY_TYPE_MASK, rand() & 1);
      }
    } else {
      collide_enemy();

      if (enm_p->type == ENEMY_TYPE_PELLET) {
        enm_p->timer = 1;
      } else {
        enm_p->timer += enm_p->spd;
        enm_p->x += enm_p->timer >> 4;
        enm_p->timer &= 0x0F;
      }

      // Enemy moved out?
      if (enm_p->x <= ACTOR_MIN_X || enm_p->x >= ACTOR_MAX_X) {
        if (enm_p->type == ENEMY_TYPE_BALL || enm_p->type == ENEMY_TYPE_TANK || enm_p->type == ENEMY_TYPE_ARROW) {
          // Turn around
          enm_p->x = enm_p->spd < 0 ? ACTOR_MIN_X + 1 : ACTOR_MAX_X - 1;
          enm_p->spd = -enm_p->spd;

          if (enm_p->type == ENEMY_TYPE_ARROW) {
            // Become a tank
            enm_p->type = ENEMY_TYPE_TANK;
          }
        } else {
          // Disappear
          enm_p->spd = 0;
        }
      }

      collide_enemy();
    }
  }
}

void draw_lives() {
  unsigned int buffer[32];

  if (!lives_need_update) {
    return;
  }

  // Redraws the topmost lane dividers
  generate_lane(buffer);
  SMS_loadTileMap(0, LANE_TOP, buffer, 64);
  memset(buffer, 0, sizeof buffer);
  SMS_loadTileMap(0, 0, buffer, 64);

  if (!lives) {
    // No lives left.
    return;
  }

  buffer[0] = LIFE_BASE_TILE;
  buffer[1] = LIFE_BASE_TILE + 2;
  buffer[2] = LIFE_BASE_TILE + 1;
  buffer[3] = LIFE_BASE_TILE + 3;

  for (g_i = 0, g_x = 16 - lives; g_i != lives; g_i++, g_x += 2) {
    SMS_loadTileMapArea(g_x, 0, buffer, 2, 2);
  }

  lives_need_update = false;
}

void init_player() {
  player_x = PLAYER_CENTER_X;
  player_y = PLAYER_MIN_Y;
  player_current_lane = 0;
  player_target_lane = 0;
  player_dead = false;
}

void move_player_target_lane() {
  // Move towards the targeted lane
  if (player_y != player_target_y) {
    if (player_y < player_target_y) {
      player_y += PLAYER_VERTICAL_SPEED;
    } else {
      player_y -= PLAYER_VERTICAL_SPEED;
    }
  } else {
    player_current_lane = player_target_lane;
  }
}

void handle_player_movement() {
  joy = SMS_getKeysStatus();

  if (joy & PORT_A_KEY_LEFT) {
    player_looking_left = true;
  } else if (joy & PORT_A_KEY_RIGHT) {
    player_looking_left = false;
  }

  if (player_current_lane == player_target_lane) {
    // Allow to move left or right if there's a pellet on the current lane
    enm_p = enemies + player_current_lane;
    if (enm_p->spd && enm_p->type == ENEMY_TYPE_PELLET || player_x != PLAYER_CENTER_X) {
      if ((joy & PORT_A_KEY_LEFT) && (player_x > ACTOR_MIN_X)) {
        player_x -= 4;
      } else if ((joy & PORT_A_KEY_RIGHT) && (player_x < ACTOR_MAX_X)) {
        player_x += 4;
      }
    }

    if (player_x == PLAYER_CENTER_X) {
      // Move up or down the lanes according to joypad command
      if ((joy & PORT_A_KEY_UP) && (player_current_lane > 0)) {
        player_target_lane--;
        change_lane();
      } else if ((joy & PORT_A_KEY_DOWN) && (player_current_lane < LANE_COUNT - 1)) {
        player_target_lane++;
        change_lane();
      }
    }
  } else {
    move_player_target_lane();
  }

  if (remaining_enemies <= 0) {
    next_game_state = STATE_NEXT_STAGE;
  }
}

void auto_player_movement() {
  if (player_current_lane == player_target_lane) {
    if (player_looking_left) {
      // Move up, if possible
      if (player_current_lane > 0) {
        player_target_lane--;
        change_lane();
      } else {
        player_looking_left = false;
      }
    } else {
      // Move down, if possible
      if (player_current_lane < LANE_COUNT - 1) {
        player_target_lane++;
        change_lane();
      } else {
        player_looking_left = true;
      }
    }
  } else {
    move_player_target_lane();
  }

  joy = SMS_getKeysStatus();
  if (joy & (PORT_A_KEY_1 | PORT_A_KEY_2)) {
    next_game_state = STATE_GAME_START;

    do {
      wait_frame();
    } while (SMS_getKeysStatus());
  }
}

void intermission() {
  unsigned int timeleft;
  unsigned int buffer[4][32], *p;
  unsigned char pal_buffer[16];

  SMS_displayOff();
  stop_sound();

  SMS_loadTiles(ship_til, 0, ship_til_size);
  SMS_loadTiles(intermission_bkg_til, 0, intermission_bkg_til_size);
  SMS_loadBGPalette(ship_pal);
  SMS_loadSpritePalette(ship_pal);

  // Draw the background

  for (g_y = 0; g_y != 4; g_y++) {
    for (g_x = 0; g_x != 32; g_x++) {
      buffer[g_y][g_x] = g_y | (g_x & 0x04 ? 0x400 : 0);
    }
  }

  for (g_y = 0; g_y < 24; g_y += 4) {
    SMS_loadTileMapArea(0, g_y, *buffer, 32, 4);
  }

  // Draw the black rectangle in the center

  p = *buffer;
  for (g_y = 0; g_y != 4; g_y++) {
    for (g_x = 0; g_x != 6; g_x++) {
      *p = 0x8FF;
      p++;
    }
  }
  SMS_loadTileMapArea(13, 10, *buffer, 6, 4);

  // Draw the level number
  SMS_initSprites();
  SMS_addSprite(126, 88, SCORE_BASE_TILE + (level_number << 1));
  SMS_finalizeSprites();
  SMS_copySpritestoSAT();

  SMS_displayOn();

  PSGPlayNoRepeat(intermission_psg);

  for (timeleft = 50; timeleft; timeleft--) {
    for (g_i = 0; g_i != 16; g_i++) {
      pal_buffer[g_i] = ship_pal[(g_i + timeleft) & 0x0F];
    }

    SMS_waitForVBlank();
    SMS_loadBGPalette(pal_buffer);
    sound_frame();

    wait_frames(2);
  }
}

void gameplay_loop(void (*player_handler)()) {
  SMS_displayOff();

  init_player();

  load_ingame_tiles();

  SMS_loadBGPalette(ship_pal);
  SMS_loadSpritePalette(ship_pal);

  draw_lanes();
  init_enemies();
  init_shots();

  score_needs_update = true;
  prepare_score();
  draw_score();
  draw_lives();

  SMS_displayOn();

  remaining_enemies = 20 + (level_number << 2);

  while (!next_game_state) {
    // Player

    (*player_handler)();
    fire();

    // Shots

    move_shots();
    move_enemies();
    prepare_score();

    // Draw

    SMS_initSprites();

    draw_player_ship();
    draw_shots();
    draw_enemies();

    SMS_finalizeSprites();

    SMS_waitForVBlank();
    SMS_copySpritestoSAT();
    draw_score();
    draw_lives();

    sound_frame();

    frame_timer++;

    if (player_dead) {
      kill_player();
    }
  }
}

void main(void) {
  SMS_VDPturnOnFeature(VDPFEATURE_USETALLSPRITES);
  SMS_VDPturnOffFeature(VDPFEATURE_HIDEFIRSTCOL);
  SMS_useFirstHalfTilesforSprites(true);

  next_game_state = STATE_DEMO;
  while (true) {
    if (next_game_state) {
      current_game_state = next_game_state;
      next_game_state = 0;
    }

    switch (current_game_state) {
      case STATE_DEMO:
        change_life_counter(0);
        player_invincible = true;
        sound_enabled = false;
        score_enabled = false;
        gameplay_loop(auto_player_movement);
        break;

      case STATE_GAME_START:
        change_life_counter(6);
        player_invincible = false;
        sound_enabled = true;
        score_enabled = true;
        next_game_state = STATE_NEXT_STAGE;
        level_number = 0;
        init_score();
        break;

      case STATE_NEXT_STAGE:
        level_number++;
        intermission();
        next_game_state = STATE_PLAY;
        break;

      case STATE_PLAY:
        gameplay_loop(handle_player_movement);
        break;
    }
  }

}

SMS_EMBED_SEGA_ROM_HEADER(9999,0); // code 9999 hopefully free, here this means 'homebrew'
SMS_EMBED_SDSC_HEADER(0,3, 2016,2,16, "Haroldo-OK\\2016", "Data Storm",
  "A fast paced shoot-em-up.\n"
  "Built using devkitSMS & SMSlib - https://github.com/sverx/devkitSMS");
