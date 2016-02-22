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

shot shots[LANE_COUNT];
enemy enemies[LANE_COUNT];

// Former local variables converted to globals to see if it speeds up things
unsigned char g_i, g_x, g_y, g_lane;
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

void draw_lanes() {
  unsigned char i;
  unsigned int *p, *p2;
  unsigned int y;
  unsigned int buffer[32];

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
    p->x = PLAYER_CENTER_X;
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

void wait_frames(int count) {
    for (; count; count--) {
      SMS_waitForVBlank();
      PSGFrame();
      PSGSFXFrame();
    }
}

void draw_player_death_frame(unsigned char frame) {
  SMS_initSprites();

  draw_ship(player_x, player_y, PLAYER_DEATH_BASE_TILE + (frame << 2), 0);

  SMS_finalizeSprites();
  SMS_waitForVBlank();
  SMS_copySpritestoSAT();

  PSGFrame();
  PSGSFXFrame();

  wait_frames(18);
}

void kill_player() {
  unsigned char i;

  PSGStop();
  PSGSFXStop();
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

  player_dead = false;
}

void kill_enemy() {
  enm_p->spd = 0;
  PSGSFXPlay(enemy_death_psg, SFX_CHANNEL2 | SFX_CHANNEL3);
}

void collide_enemy() {
  shot_p = shots + g_lane;

  if (g_lane == player_current_lane && enm_p->x > PLAYER_CENTER_X - 8 && enm_p->x < PLAYER_CENTER_X + 8) {
    player_dead = true;
    return;
  }

  if (enm_p->type == ENEMY_TYPE_PHANTOM) {
    // Phantom ships are invulnerable
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
        if (rand() & 1) {
          enm_p->x = ACTOR_MIN_X + 1;
          enm_p->spd = 12;
        } else {
          enm_p->x = ACTOR_MAX_X - 1;
          enm_p->spd = -12;
        }
        enm_p->type = rand() & ENEMY_TYPE_MASK;
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

void init_player() {
  player_x = PLAYER_CENTER_X;
  player_y = PLAYER_MIN_Y;
  player_current_lane = 0;
  player_target_lane = 0;
  player_dead = false;
}

void draw_score() {
  unsigned char i, j, y, t;

  for (j = 0, y = SCORE_TOP, t = SCORE_BASE_TILE; j != 2; j++, y++, t++) {
    SMS_setNextTileatXY(SCORE_LEFT, y);
    for (i = 0; i != 6; i++) {
      SMS_setTile((i << 1) + t);
    }
  }
}

void main(void) {
  init_player();

  SMS_VDPturnOnFeature(VDPFEATURE_USETALLSPRITES);
  SMS_VDPturnOffFeature(VDPFEATURE_HIDEFIRSTCOL);
  SMS_useFirstHalfTilesforSprites(true);

  load_ingame_tiles();

  SMS_loadBGPalette(ship_pal);
  SMS_loadSpritePalette(ship_pal);

  draw_lanes();
  init_enemies();
  init_shots();

  draw_score();

  SMS_displayOn();

  while (true) {
    // Player

    joy = SMS_getKeysStatus();

    if (joy & PORT_A_KEY_LEFT) {
      player_looking_left = true;
    } else if (joy & PORT_A_KEY_RIGHT) {
      player_looking_left = false;
    }

    if (player_current_lane == player_target_lane) {
      if ((joy & PORT_A_KEY_UP) && (player_current_lane > 0)) {
        player_target_lane--;
        change_lane();
      } else if ((joy & PORT_A_KEY_DOWN) && (player_current_lane < LANE_COUNT - 1)) {
        player_target_lane++;
        change_lane();
      }
    } else {
      if (player_y != player_target_y) {
        if (player_y < player_target_y) {
          player_y += 12;
        } else {
          player_y -= 12;
        }
      } else {
        player_current_lane = player_target_lane;
      }
    }

    fire();

    // Shots

    move_shots();
    move_enemies();

    // Draw

    SMS_initSprites();

    draw_player_ship();
    draw_shots();
    draw_enemies();

    SMS_finalizeSprites();

    SMS_waitForVBlank();
    SMS_copySpritestoSAT();

    PSGFrame();
    PSGSFXFrame();

    frame_timer++;

    if (player_dead) {
      kill_player();
    }
  }
}

SMS_EMBED_SEGA_ROM_HEADER(9999,0); // code 9999 hopefully free, here this means 'homebrew'
SMS_EMBED_SDSC_HEADER(0,3, 2016,2,16, "Haroldo-OK\\2016", "Data Storm",
  "A fast paced shoot-em-up.\n"
  "Built using devkitSMS & SMSlib - https://github.com/sverx/devkitSMS");
