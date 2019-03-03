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

void make_window8(unsigned char *buf, int xsize, int ysize, char *title, char act){
	boxfill8(buf, xsize, COL8_C6C6C6, 0, 		 0, 		xsize -1, 	0		 );
	boxfill8(buf, xsize, COL8_FFFFFF, 1, 		 1, 		xsize -2, 	1		 );
	boxfill8(buf, xsize, COL8_C6C6C6, 0, 		 0, 		0, 			ysize - 1);
	boxfill8(buf, xsize, COL8_FFFFFF, 1, 		 1, 		1, 			ysize - 2);
	boxfill8(buf, xsize, COL8_848484, xsize - 2, 1, 		xsize -2, 	ysize - 2);
	boxfill8(buf, xsize, COL8_000000, xsize - 1, 0, 		xsize -1, 	ysize - 1);
	boxfill8(buf, xsize, COL8_C6C6C6, 2, 		 2, 		xsize -3, 	ysize - 3);
	boxfill8(buf, xsize, COL8_848484, 1, 		 ysize-2, 	xsize -2, 	ysize - 2);
	boxfill8(buf, xsize, COL8_000000, 0, 		 ysize-1, 	xsize -1, 	ysize - 1);
	make_wtitle8(buf, xsize, title, act);
	return;		
}

void make_textbox8(SHEET *sht, int x0, int y0, int sx, int sy, int c){
	int x1 = x0 + sx, y1 = y0 + sy;
	boxfill8(sht->buf, sht->bxsize, COL8_848484, x0 - 2, y0 - 3, x1 + 1, y0 - 3);
	boxfill8(sht->buf, sht->bxsize, COL8_848484, x0 - 3, y0 - 3, x0 - 3, y1 + 1);
	boxfill8(sht->buf, sht->bxsize, COL8_FFFFFF, x0 - 3, y1 + 2, x1 + 1, y1 + 2);
	boxfill8(sht->buf, sht->bxsize, COL8_FFFFFF, x1 + 2, y0 - 3, x1 + 2, y1 + 2);
	boxfill8(sht->buf, sht->bxsize, COL8_000000, x0 - 1, y0 - 2, x1 + 0, y0 - 2);
	boxfill8(sht->buf, sht->bxsize, COL8_000000, x0 - 2, y0 - 2, x0 - 2, y1 + 0);
	boxfill8(sht->buf, sht->bxsize, COL8_C6C6C6, x0 - 2, y1 + 1, x1 + 0, y1 + 1);
	boxfill8(sht->buf, sht->bxsize, COL8_C6C6C6, x1 + 1, y0 - 2, x1 + 1, y1 + 1);
	boxfill8(sht->buf, sht->bxsize, c		   , x0 - 1, y0 - 1, x1 + 0, y1 + 0);
	
	
}

void putfonts8_asc_sht(SHEET *sht, int x, int y, int c, int b, char *s, int l){
	boxfill8(sht->buf, sht->bxsize, b, x, y, x+l*8-1, y+15);
	putfonts8_asc((char *)sht->buf, sht->bxsize, x, y, c, s);
	sheet_refresh(sht, x, y, x + l*8, y+16);
}

void set490(FIFO32 *fifo, int mode){
	int i;
	TIMER *timer;
	if(mode != 0){
		for(i=0;i<490;i++){
			timer =timer_alloc();
			timer_init(timer, fifo, 1024 +i);
			timer_settime(timer, 100*60*60*24*50 + i*100);
		}
	}
	return;
}

void task_b_main(SHEET *sht_win_b){
	FIFO32 fifo;
	TIMER *timer_1s;
	int i, fifobuf[128], count = 0, count0 = 0;
	char s[12];

	fifo32_init(&fifo, 128, fifobuf, 0);
	timer_1s = timer_alloc();
	timer_init(timer_1s, &fifo, 100);
	timer_settime(timer_1s, 100);

	for(;;){
		count +=1;
		io_cli();
		if(fifo32_status(&fifo) == 0){
			io_stihlt(); //hltした方が良い
		} else {
			i = fifo32_get(&fifo);
			io_sti();
			switch (i)
			{
				case 100:
					sprintf(s, "%11d", count - count0);
					putfonts8_asc_sht(sht_win_b, 24, 28, COL8_000000, COL8_C6C6C6, s, 11);
					count0 = count;
					timer_settime(timer_1s, 100);
					break;
			}			
		}
	}
}

void console_task(SHEET *sheet, unsigned int memtotal){
	TIMER * timer;
	TASK *task = task_now();

	int i, x, y, fifobuf[128], cursor_x = 16, cursor_y = 28, cursor_c = -1;
	char s[30],cmdline[30];
	MEMMAN *memman = (MEMMAN *) MEMMAN_ADDR;
	FIFOINFO *finfo = (FIFOINFO *) (ADR_DISKIMG + 0x002600);

	fifo32_init((FIFO32 *)task->fifo, 128, fifobuf, task);
	timer = timer_alloc();
	timer_init(timer, (FIFO32 *)task->fifo, 1);
	timer_settime(timer, 50);

	// プロンプト表示
	putfonts8_asc_sht(sheet, 8, 28, COL8_FFFFFF, COL8_000000, ">", 1);
	for(;;){
		io_cli();
		if(fifo32_status((FIFO32 *)task->fifo) == 0){
			task_sleep(task);
			io_sti();
		} else { 
			i = fifo32_get((FIFO32 *)task->fifo);
			io_sti();
			// カーソル用タイマ
			if(i <= 1){
				if(i != 0){
					timer_init(timer, (FIFO32 *)task->fifo, 0); //次は0
					if(cursor_c >= 0){
						cursor_c = COL8_FFFFFF;
					}
				} else {
					timer_init(timer, (FIFO32 *)task->fifo, 1); // 次は1
					if(cursor_c >= 0){
						cursor_c = COL8_000000;
					}
				}
				timer_settime(timer, 50);
			}
			switch (i)
			{
				case 2:
					cursor_c = COL8_FFFFFF;
					break;
			
				case 3:
					boxfill8(sheet->buf, sheet->bxsize, COL8_000000, cursor_x, 28, cursor_x + 7 ,43);
					cursor_c = -1;
					break;
			}
			if(256 <= i && i<=511){ // キーボードデータ (タスクA経由)

				if(i == 8 +256){ // バックスペース
					if(cursor_x > 16){ 
						// カーソルをスペースで消してからカーソルを一つ戻す
						putfonts8_asc_sht(sheet, cursor_x, cursor_y, COL8_FFFFFF, COL8_000000, "", 1);
						cmdline[cursor_x/8] = 0;
						cursor_x -= 8;
					} else if(cursor_x == 16){
						// ">"だけにする
						putfonts8_asc_sht(sheet, cursor_x, cursor_y, COL8_FFFFFF, COL8_000000, " ", 1);
					}
				} else if(i == 10 + 256){ //enter
				
					// カーソルをスペースで消す
					putfonts8_asc_sht(sheet, cursor_x, cursor_y, COL8_FFFFFF, COL8_000000, " ", 1);
					cmdline[cursor_x/8 -2] = 0;
					cursor_y = cons_newline(cursor_y, sheet);
					if(strcmp(cmdline, "mem")==0 ){
						sprintf(s, "%dMB" ,memtotal/(1024*1024));
						putfonts8_asc_sht(sheet, 8, cursor_y, COL8_FFFFFF, COL8_000000, s, 30);
						cursor_y = cons_newline(cursor_y, sheet);
						sprintf(s, "free %dKB", memman_total(memman) / 1024);
						putfonts8_asc_sht(sheet, 8, cursor_y, COL8_FFFFFF, COL8_000000, s, 30);
						cursor_y = cons_newline(cursor_y, sheet);
						cursor_y = cons_newline(cursor_y, sheet);
					} else if(strcmp(cmdline, "cls")==0){
							for(y = 28; y<28 +128;y++){
								for(x = 8; x < 8+240; x++){
									sheet->buf[x + y*sheet->bxsize] = COL8_000000;
								}
							}
							sheet_refresh(sheet, 8, 28, 8+240, 28 +128);
							cursor_y = 28;
					} else if(strcmp(cmdline, "ls") ==0){
						for(x = 0; x<224; x++){
							if(finfo[x].name[0] == 0x00){
								break;
							}
							if(finfo[x].name[0] != 0xe5){
								if((finfo[x].type & 0x18) == 0){
									sprintf(s, "filename.ext	%7d", finfo[x].size);
									for(y = 0 ;y < 8; y++){
										s[y] = finfo[x].name[y];
									}
									s[ 9] = finfo[x].ext[0];
									s[10] = finfo[x].ext[1];
									s[11] = finfo[x].ext[2];
									putfonts8_asc_sht(sheet, 8, cursor_y, COL8_FFFFFF, COL8_000000, s, 30);
									cursor_y = cons_newline(cursor_y, sheet);
								}
							}
						}
					} else if(cmdline[0] != 0) {
						putfonts8_asc_sht(	sheet, 8, cursor_y, COL8_FFFFFF, COL8_000000, "Bad Command", 12);
						cursor_y = cons_newline(cursor_y, sheet);
						cursor_y = cons_newline(cursor_y, sheet);
					}
					// プロンプト表示
					putfonts8_asc_sht(sheet, 8, cursor_y, COL8_FFFFFF, COL8_000000, ">", 1);
					cursor_x = 16;
				
				} else {
					// 一般文字
					if(cursor_x < 240){
						// 一文字表示してからカーソルを一つ進める
						s[0] = i - 256;
						s[1] = 0;
						cmdline[cursor_x/8 -2] = i-256;
						putfonts8_asc_sht(sheet, cursor_x, cursor_y, COL8_FFFFFF, COL8_000000, s, 1);
						cursor_x += 8;
						}
				}
			}
			// カーソル再表示
			if(cursor_c >= 0){
					boxfill8(sheet->buf, sheet->bxsize, cursor_c, cursor_x, cursor_y, cursor_x + 7, cursor_y + 15);
			}
			sheet_refresh(sheet, cursor_x, 28, cursor_x + 8, 44);
		}
	}
}

void make_wtitle8(unsigned char *buf, int xsize, char *title, char act){
		static char closebtn[14][16] ={
		"OOOOOOOOOOOOOOO@",
		"OQQQQQQQQQQQQQ$@",
		"OQQQQQQQQQQQQQ$@",
		"OQQQ@@QQQQ@@QQ$@",
		"OQQQQ@@QQ@@QQQ$@",
		"OQQQQQ@@@@QQQQ$@",
		"OQQQQQQ@@QQQQQ$@",
		"OQQQQQ@@@@QQQQ$@",
		"OQQQQ@@QQ@@QQQ$@",
		"OQQQ@@QQQQ@@QQ$@",
		"OQQQQQQQQQQQQQ$@",
		"OQQQQQQQQQQQQQ$@",
		"O$$$$$$$$$$$$$$@",
		"@@@@@@@@@@@@@@@@",

	};
	int x, y;
	char c, tc, tbc;
	if(act != 0){
		tc = COL8_FFFFFF;
		tbc = COL8_000084;
	} else {
		tc = COL8_C6C6C6;
		tbc = COL8_848484;
	}
	boxfill8(buf, xsize, COL8_000084, 3, 3, xsize -4, 20);
	putfonts8_asc(buf, xsize, 24, 4, tc, title);
	for(y=0; y <14; y++){
		for(x =0 ; x<16; x++){
			c = closebtn[y][x];
			if(c == '@'){
				c = COL8_000000;
			} else if(c == '$'){
				c = COL8_848484;
			} else if(c == 'Q'){
				c = COL8_C6C6C6;
			}else{
				c = COL8_FFFFFF;
			}
			buf[(5+y)*xsize + (xsize -21 + x)] = c;
		}
	}
	return;
}
int cons_newline(int cursor_y, SHEET *sheet){
	int x, y;
	if(cursor_y < 28 + 112){
		cursor_y += 16; // 次の行
	} else {
		for(y = 28; y < 28 + 112; y++){
			for(x = 8; x < 8 + 240;x++){
				sheet->buf[x + y*sheet->bxsize] = sheet->buf[x + (y+16)*sheet->bxsize];
			}
		}
		for(y = 28 + 112; y<28; y++){
			for(x = 8; x< 8 +240;x++){
				sheet->buf[x + y*sheet->bxsize] = COL8_000000;
			}
		}
		sheet_refresh(sheet, 8, 28, 8+240, 28+128);
	}
	return cursor_y;
}