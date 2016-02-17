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

const unsigned char lane_coords[] = { 0, LANE_PIXEL_HEIGHT, 2 * LANE_PIXEL_HEIGHT, 3 * LANE_PIXEL_HEIGHT, 4 * LANE_PIXEL_HEIGHT, 5 * LANE_PIXEL_HEIGHT, 6 * LANE_PIXEL_HEIGHT };

int joy;
unsigned int frame_timer;

unsigned char player_x, player_y, player_frame;
unsigned char player_fire_delay;
unsigned char player_current_lane, player_target_lane;
unsigned char player_target_y;
unsigned char player_looking_left;

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

    // Draw

    SMS_initSprites();

    draw_player_ship();

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
