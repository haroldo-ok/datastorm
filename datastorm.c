#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "SMSlib/src/SMSlib.h"
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

shot shots[LANE_COUNT];
enemy enemies[LANE_COUNT];

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
  SMS_loadTiles(ship_til, 256, ship_til_size);
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

void draw_shot(shot *p, unsigned char y) {
  if (p->flag) {
    draw_ship(p->x, y, SHOT_BASE_TILE, p->flag == SHOT_FLAG_RIGHT);
  }
}

void draw_shots() {
  unsigned char i, y;
  shot *p;

  for (i = 0, p = shots, y = LANE_PIXEL_TOP_LIMIT; i != LANE_COUNT; i++, p++, y += LANE_PIXEL_HEIGHT) {
    draw_shot(p, y);
  }
}

void move_shot(shot *p) {
  if (p->flag) {
    if (p->flag == SHOT_FLAG_LEFT) {
      p->x -= 6;
    } else {
      p->x += 6;
    }

    if (p->x <= ACTOR_MIN_X || p->x >= ACTOR_MAX_X) {
      p->flag = 0;
    }
  }
}

void move_shots() {
  unsigned char i;
  shot *p;

  for (i = 0, p = shots; i != LANE_COUNT; i++, p++) {
    move_shot(p);
  }
}

void fire() {
  shot *p = shots + player_current_lane;
  if (!p->flag) {
    p->x = PLAYER_CENTER_X;
    p->flag = player_looking_left ? SHOT_FLAG_LEFT : SHOT_FLAG_RIGHT;
  }
}

void init_enemies() {
  unsigned int i;
  enemy *p;

  for (i = 0, p = enemies; i != LANE_COUNT; i++, p++) {
    p->spd = 0;
  }
}

void draw_enemy(enemy *p, unsigned char y) {
  if (p->spd) {
    draw_ship(p->x, y, ENEMY_BASE_TILE + (p->type << ACTOR_TILE_SHIFT), p->spd > 0);
  }
}

void draw_enemies() {
  unsigned int i, y;
  enemy *p;

  for (i = 0, p = enemies, y = LANE_PIXEL_TOP_LIMIT; i != LANE_COUNT; i++, p++, y += LANE_PIXEL_HEIGHT) {
    draw_enemy(p, y);
  }
}

void collide_enemy(enemy *e, unsigned char lane) {
  shot *s = shots + lane;

  if (e->type == ENEMY_TYPE_PHANTOM) {
    // Phantom ships are invulnerable
    return;
  }

  if (s->flag) {
    if (s->x - e->x <= 16 || e->x - s->x <= 16) {
      if (e->type == ENEMY_TYPE_TANK) {
        // Tanks can only be hit from the back
        if (e->spd < 0) {
          if (s->flag == SHOT_FLAG_LEFT) {
            // Both facing left? Blam!
            e->spd = 0;
          }
        } else {
          if (s->flag == SHOT_FLAG_RIGHT) {
            // Both facing right? Blam!
            e->spd = 0;
          }
        }
      } else {
        // Other enemies will simply be dead.
        e->spd = 0;
      }

      // Removes the shot
      s->flag = 0;
    }
  }
}

void move_enemies() {
  unsigned char i;
  enemy *p;

  for (i = 0, p = enemies; i != LANE_COUNT; i++, p++) {
    if (!p->spd) {
      if ((rand() & 0x1F) == 1) {
        // Spawns a new enemy
        if (rand() & 1) {
          p->x = ACTOR_MIN_X + 1;
          p->spd = 12;
        } else {
          p->x = ACTOR_MAX_X - 1;
          p->spd = -12;
        }
        p->type = rand() & ENEMY_TYPE_MASK;
      }
    } else {
      collide_enemy(p, i);

      if (p->type == ENEMY_TYPE_PELLET) {
        p->timer = 1;
      } else {
        p->timer += p->spd;
        p->x += p->timer >> 4;
        p->timer &= 0x0F;
      }

      // Enemy moved out?
      if (p->x <= ACTOR_MIN_X || p->x >= ACTOR_MAX_X) {
        if (p->type == ENEMY_TYPE_BALL || p->type == ENEMY_TYPE_TANK || p->type == ENEMY_TYPE_ARROW) {
          // Turn around
          p->x = p->spd < 0 ? ACTOR_MIN_X + 1 : ACTOR_MAX_X - 1;
          p->spd = -p->spd;

          if (p->type == ENEMY_TYPE_ARROW) {
            // Become a tank
            p->type = ENEMY_TYPE_TANK;
          }
        } else {
          // Disappear
          p->spd = 0;
        }
      }

      collide_enemy(p, i);
    }
  }
}

void init_player() {
  player_x = PLAYER_CENTER_X;
  player_y = PLAYER_MIN_Y;
  player_current_lane = 0;
  player_target_lane = 0;
}

void main(void) {
  init_player();

  SMS_VDPturnOnFeature(VDPFEATURE_USETALLSPRITES);
  SMS_VDPturnOffFeature(VDPFEATURE_HIDEFIRSTCOL);

  load_ingame_tiles();

  SMS_loadBGPalette(ship_pal);
  SMS_loadSpritePalette(ship_pal);

  draw_lanes();
  init_enemies();
  init_shots();

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

    frame_timer++;
  }
}

SMS_EMBED_SEGA_ROM_HEADER(9999,0); // code 9999 hopefully free, here this means 'homebrew'
SMS_EMBED_SDSC_HEADER(0,3, 2016,2,16, "Haroldo-OK\\2016", "Data Storm",
  "A fast paced shoot-em-up.\n"
  "Built using devkitSMS & SMSlib - https://github.com/sverx/devkitSMS");
