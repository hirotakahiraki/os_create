#include <stdio.h>
#include "bootpack.h"

FIFO32 fifo;
extern TIMERCTL timerctl;
BOOTINFO *binfo= (BOOTINFO *) ADR_BOOTINFO;
TSS32 tss_a, tss_b;
void HariMain(void)
{
	int fifobuf[128];	
	char s[40], keybuf[32], mousebuf[128], timerbuf[8];
	int mx, my, i, count10, cursor_x = 8, cursor_c = COL8_FFFFFF, task_b_esp;
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
	buf_cons = (unsigned char *) memman_alloc_4k(memman, 144*52);
	sheet_setbuf(sht_cons, buf_cons, 256, 165, -1); //透明色なし
	make_window8(buf_cons, 256, 165, "console", 0);
	make_textbox8(sht_cons, 8, 28, 240, 128, COL8_000000);
	task_cons = task_alloc();
	task_cons->tss.esp = memman_alloc_4k(memman, 64*1024) + 64*1024 -8;
	task_cons->tss.eip = (int) &console_task;
	task_cons->tss.es = 1*8;
	task_cons->tss.cs = 2*8;
	task_cons->tss.ss = 1*8;
	task_cons->tss.ds = 1*8;
	task_cons->tss.fs = 1*8;
	task_cons->tss.gs = 1*8;
	*((int *)(task_cons->tss.esp + 4)) = (int)sht_cons;
	task_run(task_cons, 2, 2);  // level = 2, priority = 2
 
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

	sprintf(s, "(%3d, %3d)", mx, my);
	putfonts8_asc((char*)buf_back, binfo->scrnx, 0, 0, COL8_FFFFFF, s);
	sprintf(s, "memory %dMB  free : %dKB, %dx%d", memtotal/(1024 *1024), memman_total(memman)/1024,binfo->scrnx, binfo->scrny);
	putfonts8_asc(buf_back, binfo->scrnx, 0, 32, COL8_FFFFFF, s);
	sheet_refresh(sht_back, 0, 0, binfo->scrnx, 48);

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
				if(i< 256 + 0x54){
					if(keytable[i-256]!=0 && cursor_x < 144){
						s[0] = keytable[i-256];
						s[1] = 0;
						putfonts8_asc_sht(sht_win, cursor_x, 28, COL8_000000, COL8_FFFFFF, s, 1);
						cursor_x += 8;
					}
				}
				// delete
				if(i == 256 + 0x0e && cursor_x > 8){
					putfonts8_asc_sht(sht_win, cursor_x, 28, COL8_000000, COL8_FFFFFF, " ", 1);
					cursor_x -= 8;
				}
				// カーソルの再表示
				boxfill8(sht_win->buf, sht_win->bxsize, cursor_c, cursor_x, 28, cursor_x + 7, 43);
				sheet_refresh(sht_win, cursor_x, 28, cursor_x + 8 ,44);

			}else if(512 <= i && i <= 767){
				// マウス
				if( mouse_decode(binfo, &mdec, i-512)!=0 ){	
					sprintf(s, "[lcr %4d %4d]", mdec.x, mdec.y);
					if((mdec.btn & 0x01)!=0){
						s[1] = 'L';
					}
					if((mdec.btn & 0x02)!=0){
						s[3] = 'R';
					}
					if((mdec.btn & 0x04)!=0){
						s[2] = 'C';
					}
					putfonts8_asc_sht(sht_back, 32, 16, COL8_FFFFFF, COL8_008484, s, 15);
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
					sprintf(s, "(%3d, %3d)", mx, my);
					putfonts8_asc_sht(sht_back, 0, 0, COL8_FFFFFF, COL8_008484, s, 10);
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
						cursor_c = COL8_FFFFFF;
						timer_settime(timer, 50);
						boxfill8(sht_win->buf, sht_win->bxsize, cursor_c, cursor_x, 28, cursor_x + 7, 43);					
						sheet_refresh(sht_win, cursor_x, 28, cursor_x + 8, 44);
						break;
					case 1: // カーソルタイマ
						timer_init(timer, &fifo, 0);
						cursor_c = COL8_000000;
						timer_settime(timer, 50);
						boxfill8(sht_win->buf, sht_win->bxsize, cursor_c, cursor_x, 28, cursor_x + 7, 43);					
						sheet_refresh(sht_win, cursor_x, 28, cursor_x + 8, 44);
						break;
				/*	case 3:
						putfonts8_asc_sht(sht_back, 0, 80, COL8_FFFFFF, COL8_008484, "3[sec]", 6);
						break;					
					case 10:
						putfonts8_asc_sht(sht_back, 0, 64, COL8_FFFFFF, COL8_008484, "10[sec]", 7);
						break;*/
				}
		}
	}
}

void make_window8(unsigned char *buf, int xsize, int ysize, char *title, char act){
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
		tbc = COL8_008484;
	} else {
		tc = COL8_C6C6C6;
		tbc = COL8_848484;
	}
	boxfill8(buf, xsize, COL8_C6C6C6, 0, 		 0, 		xsize -1, 	0		 );
	boxfill8(buf, xsize, COL8_FFFFFF, 1, 		 1, 		xsize -2, 	1		 );
	boxfill8(buf, xsize, COL8_C6C6C6, 0, 		 0, 		0, 			ysize - 1);
	boxfill8(buf, xsize, COL8_FFFFFF, 1, 		 1, 		1, 			ysize - 2);
	boxfill8(buf, xsize, COL8_848484, xsize - 2, 1, 		xsize -2, 	ysize - 2);
	boxfill8(buf, xsize, COL8_000000, xsize - 1, 0, 		xsize -1, 	ysize - 1);
	boxfill8(buf, xsize, COL8_C6C6C6, 2, 		 2, 		xsize -3, 	ysize - 3);
	boxfill8(buf, xsize, COL8_000084, 3, 		 3, 		xsize -4, 	20		 );
	boxfill8(buf, xsize, COL8_848484, 1, 		 ysize-2, 	xsize -2, 	ysize-2  );
	boxfill8(buf, xsize, COL8_000000, 0, 		 ysize-1, 	xsize -1, 	ysize-1  );
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

void console_task(SHEET *sheet){
	FIFO32 fifo;
	TIMER * timer;
	TASK *task = task_now();

	int i, fifobuf[128], cursor_x = 8, cursor_c = COL8_000000;
	fifo32_init(&fifo, 128, fifobuf, task);
	timer = timer_alloc();
	timer_init(timer, &fifo, 1);
	timer_settime(timer, 50);

	for(;;){
		io_cli();
		if(fifo32_status(&fifo) == 0){
			task_sleep(task);
			io_sti();
		} else { 
			i = fifo32_get(&fifo);
			io_sti();
			// カーソル用タイマ
			if(i <= 1){
				if(i != 0){
					timer_init(timer, &fifo, 0); //次は0
					cursor_c = COL8_FFFFFF;
				} else {
					timer_init(timer, &fifo, 1); // 次は1
					cursor_c = COL8_000000;
				}
				timer_settime(timer, 50);
				boxfill8(sheet->buf, sheet->bxsize, cursor_c, cursor_x, 28, cursor_x + 7, 43);
				sheet_refresh(sheet, cursor_x, 28, cursor_x + 8, 44);
			}
		}
	}
}