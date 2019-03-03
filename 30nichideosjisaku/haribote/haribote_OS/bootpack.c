#include <stdio.h>
#include <string.h>
#include "bootpack.h"

FIFO32 fifo;
extern TIMERCTL timerctl;
BOOTINFO *binfo= (BOOTINFO *) ADR_BOOTINFO;
TSS32 tss_a, tss_b;
void HariMain(void)
{
	int fifobuf[128];	
	char s[40], keybuf[32], mousebuf[128], timerbuf[8];
	int mx, my, i, count10, cursor_x = 8, cursor_c = -1, task_b_esp;
	int key_to = 0, key_shift = 0, key_leds = (binfo->leds >> 4) & 7, key_caps = 0;
	unsigned int memtotal, count =0;
	MOUSE_DEC mdec;
	MEMMAN *memman = (MEMMAN *) MEMMAN_ADDR;
	SHTCTL *shtctl;
	SHEET *sht_back, *sht_mouse, *sht_win, *sht_cons;
	TIMER *timer;
	unsigned char *buf_back, buf_mouse[256], *buf_win, *buf_cons;
	TASK *task_a, *task_cons;

	init_gdtidt();
	init_pic();
	io_sti();

	fifo32_init(&fifo, 128, fifobuf, 0);
	init_pit();
	io_out8(PIC0_IMR, 0xf8); /* PIT, PIC1, キーボードを初期化 */
	io_out8(PIC1_IMR, 0xef);

	init_keyboard(&fifo, 256);
	enable_mouse(&fifo, 512, &mdec);
	memtotal = memtest(0x00400000,0xbfffffff);
	memman_init(memman);
	memman_free(memman, 0x00001000, 0x0009e000); // 0x00001000 - 0x0009efff 
	memman_free(memman, 0x00400000, memtotal - 0x00400000);
	
	init_palette();	
	shtctl = shtctl_init(memman, binfo->vram, binfo->scrnx, binfo->scrny);
	task_a = task_init(memman);
	fifo.task = task_a;
	task_run(task_a, 1, 2);

	// sht_back
	sht_back = sheet_alloc(shtctl);
	buf_back = (unsigned char *) memman_alloc_4k(memman, binfo->scrnx * binfo->scrny);
	sheet_setbuf(sht_back, buf_back, binfo->scrnx, binfo->scrny, -1);
	init_screen(buf_back, binfo->scrnx, binfo->scrny);

	// sht_cons

	sht_cons = sheet_alloc(shtctl);
	buf_cons = (unsigned char *) memman_alloc_4k(memman, 256*165);
	sheet_setbuf(sht_cons, buf_cons, 256, 165, -1); //透明色なし
	make_window8(buf_cons, 256, 165, "console", 0);
	make_textbox8(sht_cons, 8, 28, 240, 128, COL8_000000);
	task_cons = task_alloc();
	task_cons->tss.esp = memman_alloc_4k(memman, 64*1024) + 64*1024 -12;
	task_cons->tss.eip = (int) &console_task;
	task_cons->tss.es = 1*8;
	task_cons->tss.cs = 2*8;
	task_cons->tss.ss = 1*8;
	task_cons->tss.ds = 1*8;
	task_cons->tss.fs = 1*8;
	task_cons->tss.gs = 1*8;
	*((int *)(task_cons->tss.esp + 4)) = (int)sht_cons;
	task_run(task_cons, 2, 2);  // level = 2, priority = 2
	*((int*) (task_cons->tss.esp + 4)) = (int) sht_cons;
	*((int*) (task_cons->tss.esp + 8)) = memtotal;
   
	// sht_win
	sht_win = sheet_alloc(shtctl);
	buf_win = (unsigned char *) memman_alloc_4k(memman, 160 * 52);
	sheet_setbuf(sht_win, buf_win, 144, 52, -1);
	make_window8(buf_win, 144, 52, "task_a", 1);
	make_textbox8(sht_win, 8, 28, 128, 16, COL8_FFFFFF);
	cursor_x = 8;
	cursor_c = COL8_FFFFFF;
	timer = timer_alloc();
	timer_init(timer, &fifo, 1);
	timer_settime(timer, 50);

	// sht_mouse
	sht_mouse = sheet_alloc(shtctl);	
	sheet_setbuf(sht_mouse, buf_mouse, 16, 16, 99);
	init_mouse_cursor8(buf_mouse, 99); //COL8_008484
	mx = (binfo->scrnx -16)/2; // 画面中央になるように計算
	my = (binfo->scrny -28-16)/2;

	// make_window8(buf_win, 160, 68,"console");
	sheet_slide(sht_back, 0, 0);
	sheet_slide(sht_cons, 32, 4);
	sheet_slide(sht_win,  64, 56);
	sheet_slide(sht_mouse, mx, my);
	sheet_updown(sht_back, 0);
	sheet_updown(sht_cons, 1);
	sheet_updown(sht_win, 2);
	sheet_updown(sht_mouse, 3);

	for (;;) {
		io_cli();
		if(fifo32_status(&fifo) == 0){
			task_sleep(task_a);
			io_sti();
		} else
		{	
			i = fifo32_get(&fifo);
			io_sti();
			if(256 <= i && i <=511){
				// キーボード
				sprintf(s, "%02X", i-256);
				putfonts8_asc_sht(sht_back, 0, 16, COL8_FFFFFF, COL8_008484, s, 2);
				// 文字コード変換
				if(i< 256 + 0x80){
					if(key_shift == 0){
						s[0] = keytable0[i - 256];
					} else {
						s[0] = keytable1[i - 256];
					}
				} else {
					s[0] = 0;
				} 
				if('a' <= s[0] && s[0] <= 'z'){ 
					if(key_caps!=0 || key_shift !=0){
							s[0] -= 0x20;
					}
				}
				if(s[0] != 0){
					if(key_to == 0){ //task Aへ
						if(cursor_x < 128){
						// 一文字表示してカーソルを進める
						s[1] = 0;
						putfonts8_asc_sht(sht_win, cursor_x, 28, COL8_000000, COL8_FFFFFF, s, 1);
						cursor_x += 8;
						} 
					}else { // コンソールへ
						fifo32_put((FIFO32 *)task_cons->fifo, s[0]+256);
					}
				}

				// delete key
				if(i == 256 + 0x0e){
					if(key_to == 0){ // task Aへ
						if(cursor_x > 8){
							// カーソルをスペースで消してカーソルを一つ戻す
							putfonts8_asc_sht(sht_win, cursor_x, 28, COL8_000000, COL8_FFFFFF, "", 1);
							cursor_x -= 8;
						}
					} else {
						fifo32_put((FIFO32 *)task_cons->fifo, 8+256);
					}
				}

				// tabボタン 
				if(i == 256 + 0x0f){
					if(key_to == 0){
						key_to = 1;
						make_wtitle8(buf_win, sht_win->bxsize, "task_a", 0);
						make_wtitle8(buf_cons, sht_cons->bxsize, "console", 1);
						cursor_c = -1; // カーソルを消す
						boxfill8(sht_win->buf, sht_win->bxsize, COL8_FFFFFF, cursor_x, 28, cursor_x + 7, 43);
						fifo32_put((FIFO32 *)task_cons->fifo, 2); // コンソールのカーソルON
					} else {
						key_to = 0;
						make_wtitle8(buf_win, sht_win->bxsize, "task_a", 1);
						make_wtitle8(buf_cons, sht_cons->bxsize, "console", 0);	
						cursor_c = COL8_000000; // カーソルを出す
						fifo32_put((FIFO32 *)task_cons->fifo, 3); // コンソールのカーソルOFF					
					}
					sheet_refresh(sht_win, 0, 0, sht_win->bxsize, 21);
					sheet_refresh(sht_cons, 0, 0, sht_cons->bxsize, 21);
				}

				
				switch (i)
				{
					// shift key
					case 256 + 0x2a:
						key_shift |= 1; // 左シフトON
						break;
					case 256 + 0x36:
						key_shift |= 2; // 右シフトON
						break;
					case 256 + 0xaa:
						key_shift &= ~1; // 左シフトOFF 
						break;
					case 256 + 0xb6:
						key_shift &= ~2; // 右シフトOFF
						break;
					case 256 + 0xba:
						if(key_caps != 0){
							key_caps = 0;
						} else {
							key_caps = 1;
						}
						break;
					case 256 + 0x1c:
						if(key_to != 0){ fifo32_put((FIFO32 *)task_cons->fifo ,10 +256); }
						break;
				}

				// カーソルの再表示
				if(cursor_c >= 0){
					boxfill8(sht_win->buf, sht_win->bxsize, cursor_c, cursor_x, 28, cursor_x + 7, 43);
				}
				sheet_refresh(sht_win, cursor_x, 28, cursor_x + 8 ,44);

			}else if(512 <= i && i <= 767){
				// マウス
				if( mouse_decode(binfo, &mdec, i-512)!=0 ){	
					sprintf(s, "[lcr %4d %4d]", mdec.x, mdec.y);
					/* マウスカーソルの移動 */
					mx += mdec.x;
					my += mdec.y;
					if(mx < 0){
						mx = 0;
					}
					if(my < 0){
						my = 0;
					}
					if(mx > binfo->scrnx - 1){
						mx = binfo->scrnx -1;
					}
					if(my > binfo->scrny -1){
						my = binfo->scrny - 1;
					}
					sheet_slide(sht_mouse, mx, my);
					if((mdec.btn & 0x01) !=0){
						// 左ボタンを推したら sht_winを動かす
						sheet_slide(sht_win, mx - 80, my - 8);
					}
				}	
			}
	
			switch (i)
				{
					case 0: // カーソルタイマ
						timer_init(timer, &fifo, 1); // 次は1 
						timer_settime(timer, 50);
						if(cursor_c >=0){
							cursor_c = COL8_FFFFFF;
							boxfill8(sht_win->buf, sht_win->bxsize, cursor_c, cursor_x, 28, cursor_x + 7, 43);					
							sheet_refresh(sht_win, cursor_x, 28, cursor_x + 8, 44);
						}
						break;
					case 1: // カーソルタイマ
						timer_init(timer, &fifo, 0);
						timer_settime(timer, 50);
						if(cursor_c >=0){
							cursor_c = COL8_000000;
							boxfill8(sht_win->buf, sht_win->bxsize, cursor_c, cursor_x, 28, cursor_x + 7, 43);					
							sheet_refresh(sht_win, cursor_x, 28, cursor_x + 8, 44);
						}
						break;
				}
		}
	}
}