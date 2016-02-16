#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "SMSlib/src/SMSlib.h"
#include "gfx.h"

#define PF_OFFSET_X 0
#define PF_OFFSET_Y 0

#define PF_WIDTH 32
#define PF_HEIGHT 24

#define SEC_COUNT_X 3
#define SEC_COUNT_Y 3
#define SEC_COUNT (SEC_COUNT_X * SEC_COUNT_Y)
#define SEC_WIDTH (PF_WIDTH / SEC_COUNT_X)
#define SEC_HEIGHT (PF_HEIGHT / SEC_COUNT_Y)
#define SEC_MIN_W 4
#define SEC_MIN_H 4

#define DIR_UP 0
#define DIR_RIGHT 1
#define DIR_DOWN 2
#define DIR_LEFT 3
#define DIR_COUNT 4
#define DIR_MASK (DIR_COUNT - 1)

#define ACTOR_COUNT 64

typedef struct _box {
  unsigned char x1, y1, x2, y2;
} box;

typedef struct _section {
  box c;
} section;

typedef struct _actor {
  char ch, ground_ch;
  unsigned char x, y;
  unsigned int hp;
  bool dirty;

  void (*handler)(struct _actor *p);
} actor;

section sections[SEC_COUNT_Y][SEC_COUNT_Y];
char map[PF_HEIGHT][PF_WIDTH];

actor actors[ACTOR_COUNT];
unsigned char actor_count = 0;

actor *player;
unsigned short kp;

void putchar (char c) {
	SMS_setTile(c - 32);
}

void clear_map() {
  memset(*map, ' ', PF_WIDTH * PF_HEIGHT);
}

void draw_map() {
  char *o;
  unsigned int buffer[PF_WIDTH], *d;
  unsigned int i, j;

  o = map[0];
  for (i = 0; i != PF_HEIGHT; i++) {
    d = buffer;
    for (j = 0; j != PF_WIDTH; j++) {
      *d = *o - 32;
      o++; d++;
    }
    SMS_loadTileMap (0, i, buffer, PF_WIDTH << 1);
  }
}

void draw_room_line(unsigned char x, unsigned char y, unsigned char w, char border, char fill) {
  char *d = map[y] + x;

  *d = border; d++;
  for (w -= 2; w != 0; w--) {
    *d = fill; d++;
  }
  *d = border; d++;
}

void draw_room(unsigned char x, unsigned char y, unsigned char w, unsigned char h) {
  draw_room_line(x, y, w, '+', '-');
  y++;

  for (h -= 2; h != 0; h--) {
    draw_room_line(x, y, w, '|', '.');
    y++;
  }

  draw_room_line(x, y, w, '+', '-');
}

void draw_corridor_x(unsigned char x1, unsigned char y1, unsigned char x2, unsigned char y2) {
  char dy = y1 < y2 ? 1 : -1;
  char dx = x1 < x2 ? 1 : -1;
  unsigned char len = x1 < x2 ? x2 - x1 : x1 - x2;
  unsigned char xh = 1 + (len > 1 ? rand() % (len - 1) : 0);
  unsigned char x, y;

  xh = x1 < x2 ? x1 + xh : x1 - xh;

  for (x = x1; x != xh; x += dx) {
      map[y1][x] = '#';
  }

  for (y = y1; y != y2; y += dy) {
      map[y][xh] = '#';
  }

  for (x = xh; x != x2; x += dx) {
      map[y2][x] = '#';
  }

  map[y1][x1] = '*';
  map[y2][x2] = '*';
}

void draw_corridor_y(unsigned char x1, unsigned char y1, unsigned char x2, unsigned char y2) {
  char dy = y1 < y2 ? 1 : -1;
  char dx = x1 < x2 ? 1 : -1;
  unsigned char len = y1 < y2 ? y2 - y1 : y1 - y2;
  unsigned char yh = 1 + (len > 1 ? rand() % (len - 1) : 0);
  unsigned char x, y;

  yh = y1 < y2 ? y1 + yh : y1 - yh;

  for (y = y1; y != yh; y += dy) {
      map[y][x1] = '#';
  }

  for (x = x1; x != x2; x += dx) {
      map[yh][x] = '#';
  }

  for (y = yh; y != y2; y += dy) {
      map[y][x2] = '#';
  }

  map[y1][x1] = '*';
  map[y2][x2] = '*';
}

void create_section(unsigned char x, unsigned char y, section *sec) {
  unsigned char max_x = x + SEC_WIDTH - 1;
  unsigned char max_y = y + SEC_HEIGHT - 1;

  sec->c.x1 = x + rand() % (SEC_WIDTH - SEC_MIN_W);
  sec->c.y1 = y + rand() % (SEC_HEIGHT - SEC_MIN_H);
  sec->c.x2 = sec->c.x1 + SEC_MIN_W;
  sec->c.y2 = sec->c.y1 + SEC_MIN_H;

  if (sec->c.x2 < max_x) {
    sec->c.x2 += rand() % (max_x - sec->c.x2);
  }
  if (sec->c.y2 < max_y) {
    sec->c.y2 += rand() % (max_y - sec->c.y2);
  }
}

void draw_section(section *sec) {
  draw_room(sec->c.x1, sec->c.y1,
      sec->c.x2 - sec->c.x1 + 1, sec->c.y2 - sec->c.y1 + 1);
}

void create_sections() {
    // Sector number / sector coordinate
    unsigned char secn_x, secn_y, secc_x, secc_y;
    section *sec = sections[0];

    for (secn_y = 0, secc_y = 0; secn_y != SEC_COUNT_Y; secn_y++, secc_y += SEC_HEIGHT) {
      for (secn_x = 0, secc_x = 0; secn_x != SEC_COUNT_X; secn_x++, secc_x += SEC_WIDTH) {
        create_section(secc_x, secc_y, sec);
        draw_section(sec);
        sec++;
      }
    }
}

bool can_go[SEC_COUNT_Y][SEC_COUNT_X][DIR_COUNT];

void create_random_corridor_x(
    unsigned char xa, unsigned char ya1, unsigned char ya2,
    unsigned char xb, unsigned char yb1, unsigned char yb2) {
  unsigned char y1 = ya1 + 1 + rand() % (ya2 - ya1 - 1);
  unsigned char y2 = yb1 + 1 + rand() % (yb2 - yb1 - 1);
  draw_corridor_x(xa, y1, xb, y2);
}

void create_random_corridor_y(
    unsigned char xa1, unsigned char xa2, unsigned char ya,
    unsigned char xb1, unsigned char xb2, unsigned char yb) {
  unsigned char x1 = xa1 + 1 + rand() % (xa2 - xa1 - 1);
  unsigned char x2 = xb1 + 1 + rand() % (xb2 - xb1 - 1);
  draw_corridor_y(x1, ya, x2, yb);
}

void create_corridor(char x1, char y1, char x2, char y2, unsigned char dir) {
  section *sec1, *sec2;

  if (can_go[y1][x1][dir]) {
    x2 = x1; y2 = y1;
    switch (dir) {
      case DIR_UP:
        y2--;
        break;
      case DIR_DOWN:
        y2++;
        break;
      case DIR_LEFT:
        x2--;
        break;
      case DIR_RIGHT:
        x2++;
        break;
    }

    sec1 = &sections[y1][x1];
    can_go[y1][x1][dir] = false;
    sec2 = &sections[y2][x2];
    can_go[y2][x2][(dir + 2) & DIR_MASK] = false;

    switch (dir) {
      case DIR_UP:
        create_random_corridor_y(
            sec1->c.x1, sec1->c.x2, sec1->c.y1,
            sec2->c.x1, sec2->c.x2, sec2->c.y2);
        break;
      case DIR_DOWN:
        create_random_corridor_y(
            sec1->c.x1, sec1->c.x2, sec1->c.y2,
            sec2->c.x1, sec2->c.x2, sec2->c.y1);
        break;
      case DIR_LEFT:
        create_random_corridor_x(
            sec1->c.x1, sec1->c.y1, sec1->c.y2,
            sec2->c.x2, sec2->c.y1, sec2->c.y2);
        break;
      case DIR_RIGHT:
        create_random_corridor_x(
            sec1->c.x2, sec1->c.y1, sec1->c.y2,
            sec2->c.x1, sec2->c.y1, sec2->c.y2);
        break;
    }
  }
}

void random_walk() {
  unsigned char dir, remaining, extra_steps;
  char x1, y1, x2, y2;
  bool visited[SEC_COUNT_Y][SEC_COUNT_X];

  remaining = SEC_COUNT;
  for (y1 = 0; y1 != SEC_COUNT_Y; y1++) {
    for (x1 = 0; x1 != SEC_COUNT_X; x1++) {
      visited[y1][x1] = false;
    }
  }

  x1 = rand() % SEC_COUNT_X;
  y1 = rand() % SEC_COUNT_Y;

  extra_steps = 8;
  while (remaining || extra_steps) {
    // Check if a new room has been visited
    if (!visited[y1][x1]) {
      visited[y1][x1] = true;
      remaining--;
    }

    // After visiting everything, walk some extra steps
    if (!remaining && extra_steps) {
      extra_steps--;
    }

    // Choose a direction to move
    dir = rand() & DIR_MASK;

    // Try to move in the given direction
    x2 = x1; y2 = y1;
    switch (dir) {
      case DIR_UP:
        y2--;
        break;
      case DIR_DOWN:
        y2++;
        break;
      case DIR_LEFT:
        x2--;
        break;
      case DIR_RIGHT:
        x2++;
        break;
    }

    // If the direction is valid, move
    if (x2 >= 0 && x2 < SEC_COUNT_X && y2 >= 0 && y2 < SEC_COUNT_Y) {
      create_corridor(x1, y1, x2, y2, dir);
      x1 = x2; y1 = y2;
    }
  }

}

void create_corridors() {
  unsigned char x, y, i;

  for (y = 0; y != SEC_COUNT_Y; y++) {
    for (x = 0; x != SEC_COUNT_X; x++) {
      for (i = 0; i != DIR_COUNT; i++) {
        can_go[y][x][i] = true;
      }
    }
  }

  for (x = 0; x != SEC_COUNT_X; x++) {
    can_go[0][x][DIR_UP] = false;
    can_go[SEC_COUNT_Y - 1][x][DIR_DOWN] = false;
  }

  for (y = 0; y != SEC_COUNT_Y; y++) {
    can_go[y][0][DIR_LEFT] = false;
    can_go[y][SEC_COUNT_X - 1][DIR_RIGHT] = false;
  }

  random_walk();
}

void draw_char(unsigned char x, unsigned char y, char c) {
  SMS_setTileatXY(x + PF_OFFSET_X, y + PF_OFFSET_Y, c - 32);
}

bool is_wall(char ch) {
  switch (ch) {
    case '+':
    case '-':
    case '|':
    case ' ':
      return true;
  }
  return false;
}

bool is_ground(char ch) {
  return (ch == '.') || (ch == '#') || (ch == '*');
}

bool is_actor(char ch) {
  return !is_wall(ch) && !is_ground(ch);
}

void init_actors() {
  unsigned char i;
  actor *p = actors;

  for (i = ACTOR_COUNT; i; i--) {
    p->hp = 0;
    p++;
  }
}

actor *create_actor(unsigned char x, unsigned char y, char ch) {
  actor *p = actors;
  unsigned int i;

  for (i = 0; i != actor_count && p->hp; i++) {
    p++;
  }

  if (i == actor_count) {
    actor_count++;
  }

  p->x = x;
  p->y = y;

  p->ch = ch;
  p->ground_ch = map[y][x];
  map[y][x] = p->ch;

  p->hp = 1;
  p->dirty = true;
  p->handler = NULL;

  return p;
}

actor *create_actor_somewhere(char ch) {
  unsigned char x, y;
  section *sec;

  do {
    x = rand() % SEC_COUNT_X;
    y = rand() % SEC_COUNT_Y;
    sec = &sections[y][x];

    x = sec->c.x1 + 1 + (rand() % (sec->c.x2 - sec->c.x1));
    y = sec->c.y1 + 1 + (rand() % (sec->c.y2 - sec->c.y1));
  } while (!is_ground(map[y][x]));

  return create_actor(x, y, ch);
}

actor *actor_at(unsigned char x, unsigned char y) {
  actor *p = actors;
  unsigned int i;

  if (!is_actor(map[y][x])) {
    return NULL;
  }

  for (i = 0; i != actor_count; i++) {
    if (p->hp && p->x == x && p->y == y) {
      return p;
    }
    p++;
  }

  return NULL;
}

bool can_move_actor(actor *p, char dx, char dy) {
  unsigned char x = p->x + dx;
  unsigned char y = p->y + dy;
  return is_ground(map[y][x]);
}

void attack_actor(actor *atk, actor *def) {
  def->hp--;
  if (!def->hp) {
    map[def->y][def->x] = def->ground_ch;
  }
}

void move_actor(actor *p, char dx, char dy) {
  unsigned char x = p->x + dx;
  unsigned char y = p->y + dy;
  actor *other = actor_at(x, y);

  if (other) {
    attack_actor(p, other);
    return;
  }

  if (can_move_actor(p, dx, dy)) {
      map[p->y][p->x] = p->ground_ch;
      p->x = x; p->y = y;
      map[p->y][p->x] = p->ch;
  }
}

void move_actors() {
  actor *p;
  unsigned char i;

  for (i = 0, p = actors; i != actor_count; i++, p++) {
    if (p->hp && p->handler) {
      p->handler(p);
    }
  }
}

void draw_actors() {
  actor *p;
  unsigned char i;

  for (i = 0, p = actors; i != actor_count; i++, p++) {
    if (p->hp) {
      SMS_addSprite (p->x << 3, p->y << 3, p->ch - 32);
    }
  }
}

void act_move_keys(actor *p) {
  if (kp & PORT_A_KEY_UP) { move_actor(p, 0, -1); }
  if (kp & PORT_A_KEY_DOWN) { move_actor(p, 0, 1); }
  if (kp & PORT_A_KEY_LEFT) { move_actor(p, -1, 0); }
  if (kp & PORT_A_KEY_RIGHT) { move_actor(p, 1, 0); }
}

void act_move_random(actor *p) {
  char x = 0, y = 0;
  unsigned char dir = rand() & DIR_MASK;
  unsigned char tries = 4;

  do {
    switch (dir) {
      case DIR_UP: y = -1; break;
      case DIR_DOWN: y = 1; break;
      case DIR_LEFT: x = -1; break;
      case DIR_RIGHT: x = 1; break;
    }
  } while(tries-- && !can_move_actor(p, x, y));

  if (x || y) {
    move_actor(p, x, y);
  }
}

void create_enemy() {
  actor *enm = create_actor_somewhere('e');
  enm->handler = act_move_random;
}

void title_screen() {
  unsigned int seed = 1;

  SMS_setNextTileatXY(6, 5);
  puts("**** SMS-Rogue ****");
  SMS_setNextTileatXY(6, 7);
  puts("** Press any key **");
  SMS_displayOn();

  while (SMS_getKeysStatus()) {
    SMS_waitForVBlank();
    seed++;
  }
  while (!SMS_getKeysStatus()) {
    SMS_waitForVBlank();
    seed++;
  }
  while (SMS_getKeysStatus()) {
    SMS_waitForVBlank();
    seed++;
  }

  SMS_displayOff();

  srand(seed ? seed : 1);
}

void simple_rl(void)
{
  clear_map();
  init_actors();

  create_sections();
  create_corridors();
  draw_map();

  player = create_actor_somewhere('@');
  player->handler = act_move_keys;
  player->hp = 5;

  create_actor_somewhere('>');
  create_enemy();
  create_enemy();
  create_enemy();
  create_enemy();
  create_enemy();
  create_enemy();

  SMS_displayOn();

  while (true) {
    kp = SMS_getKeysPressed();

    SMS_waitForVBlank();

    if (kp) {
      move_actors();
    }

    SMS_initSprites();

    draw_actors();

    SMS_finalizeSprites();
    SMS_copySpritestoSAT();
  }
}

void load_font (void) {
  unsigned char i, j;
	unsigned char buffer[32], *o, *d;

	o = font_fnt;
	for (i = 0; i != 96; i++) {
		d = buffer;
		for (j = 0; j != 8; j++) {
			*d = *o; d++;
			*d = ~(*o);	d++;
			*d = 0;	d++;
			*d = 0;	d++;
      o++;
		}
		SMS_loadTiles(buffer, i, 32);
    SMS_loadTiles(buffer, i + 256, 32);
	}
}

void main(void) {
  unsigned char i;

  load_font();

  for (i=0;i<16;i++) {
    SMS_setBGPaletteColor(i,0x00);    // black
    SMS_setSpritePaletteColor(i,0x00);    // black
  }
  SMS_setBGPaletteColor(01,0x3f);     // white
  SMS_setSpritePaletteColor(01,0x3f);     // white

  title_screen();
  simple_rl();
}

SMS_EMBED_SEGA_ROM_HEADER(9999,0); // code 9999 hopefully free, here this means 'homebrew'
SMS_EMBED_SDSC_HEADER(0,6, 2015,9,10, "Haroldo-OK\\2015", "SMS-Rogue",
  "A roguelike for the Sega Master System - https://github.com/haroldo-ok/sms-rogue.\n"
  "Built using devkitSMS & SMSlib - https://github.com/sverx/devkitSMS");
