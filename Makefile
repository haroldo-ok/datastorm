SHELL=C:/Windows/System32/cmd.exe
CC	= sdcc -mz80

BINS	= datastorm.sms


all:	$(BINS)

gfx.c: gfx/font.fnt gfx/ship.pal gfx/ship.til gfx/intermission_bkg.til gfx/player_shot.psg gfx/enemy_death.psg gfx/player_death.psg
	folder2c gfx gfx

datastorm.rel: gfx.c

%.rel:	%.c
	$(CC) -c --std-sdcc99 $< -o $@

%.sms:	%.ihx
	ihx2sms $< $@

datastorm.ihx:	datastorm.rel SMSlib/SMSlib.rel PSGlib/PSGlib.rel gfx.rel
	$(CC) --data-loc 0xC000 $^

clean:
	rm -f *.ihx *.lk *.lst *.map *.noi *.rel *.sym *.asm *.sms gfx.*
