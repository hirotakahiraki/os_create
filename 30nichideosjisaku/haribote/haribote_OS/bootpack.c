#include <stdio.h>
#include <string.h>
#include "bootpack.h"
SHEET *sht_debug, *sht_back;
FIFO32 fifo,keycmd;
BOOTINFO *binfo= (BOOTINFO *) ADR_BOOTINFO;
TSS32 tss_a, tss_b;
void HariMain(void)
{
	int fifobuf[128];
	char s[40];
	int mx, my, i, cursor_x = 8, cursor_c = -1;
	int key_to = 0, key_shift = 0, key_caps = 0;
	int j, x, y, mmx = -1, mmy = -1;
	unsigned int memtotal;
	MOUSE_DEC mdec;
	MEMMAN *memman = (MEMMAN *) MEMMAN_ADDR;
	SHTCTL *shtctl;
	SHEET *sht_mouse, *sht_win, *sht_cons, *sht = 0, *key_win;
	TIMER *timer;
	CONSOLE *cons;
	unsigned char *buf_back, buf_mouse[256], *buf_win, *buf_cons;
	TASK *task_a, *task_cons;
	static char keytable0[0x80] = { 0,   0,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '^', 0, 0, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '@', '[',  0 ,  0 , 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', ':',  0,   0 , ']', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',  0 , '*',  0 , ' ',  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , 0, 0, '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.', 0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , 0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , 0 ,  0 ,  0 ,  0 ,  0 , 0x5c, 0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , 0 ,  0 , 0x5c, 0 ,  0 };
	static char keytable1[0x80] = { 0 ,  0 , '!', 0x22,'#', '$', '%', '&', 0x27,'(', ')',  0 , '=', '~', 0 ,  0 , 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '`', '{',  0 ,  0 , 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', '+', '*',  0 ,  0 ,  '}','Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', '_','*',  0 , ' ',  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , 0, 0, '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.', 0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , 0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , 0 ,  0 ,  0 ,  0 ,  0 , '_', 0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , 0 ,  0 , '|', 0 ,  0 };
	sht_debug = sht_back;
	init_gdtidt();
	init_pic();
	io_sti();

	fifo32_init(&fifo, 128, fifobuf, 0);
	init_pit();
	init_keyboard(&fifo, 256);
	enable_mouse(&fifo, 512, &mdec);
	io_out8(PIC0_IMR, 0xf8); /* PIT, PIC1, キーボードを初期化 */
	io_out8(PIC1_IMR, 0xef);

	memtotal = memtest(0x00400000,0xbfffffff);
	memman_init(memman);
	memman_free(memman, 0x00001000, 0x0009e000); // 0x00001000 - 0x0009efff 
	memman_free(memman, 0x00400000, memtotal - 0x00400000);
	
	init_palette();	
	shtctl = shtctl_init(memman, binfo->vram, binfo->scrnx, binfo->scrny);
	task_a = task_init(memman);
	fifo.task = task_a;
	task_run(task_a, 1, 2);
	*((int *) 0x0fe4) = (int) shtctl;

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
	*((int*) (task_cons->tss.esp + 4)) = (int) sht_cons;
	*((int*) (task_cons->tss.esp + 8)) = memtotal;
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

	// windowの切り替え
	key_win = sht_win;
	sht_cons->task = task_cons;
	sht_cons->flags |= 0x20;

	for (;;) {
		io_cli();
		if(fifo32_status(&fifo) == 0){
			task_sleep(task_a);
			io_sti();
		} else
		{	
			i = fifo32_get(&fifo);
			io_sti();
			if(key_win->flags == 0){// 入力ウィンドウが閉じられた
				key_win = shtctl->sheets[shtctl->top -1];
				cursor_c = keywin_on(key_win, sht_win, cursor_c);
			}
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
					if(key_win == sht_win){ //task Aへ
						if(cursor_x < 128){
						// 一文字表示してカーソルを進める
						s[1] = 0;
						putfonts8_asc_sht(sht_win, cursor_x, 28, COL8_000000, COL8_FFFFFF, s, 1);
						cursor_x += 8;
						} 
					}else { // コンソールへ
						fifo32_put(&key_win->task->fifo, s[0]+256);
					}
				}

				// delete key
				if(i == 256 + 0x0e){
					if(key_win == sht_win){ // task Aへ
						if(cursor_x > 8){
							// カーソルをスペースで消してカーソルを一つ戻す
							putfonts8_asc_sht(sht_win, cursor_x, 28, COL8_000000, COL8_FFFFFF, " ", 1);
							cursor_x -= 8;
						}
					} else {
						fifo32_put(&key_win->task->fifo, 8+256);
					}
				}

				// tabボタン 
				if(i == 256 + 0x0f){
					cursor_c = keywin_off(key_win, sht_win, cursor_c, cursor_x);
					j = key_win->height -1;
					if( j ==0){
						j = shtctl->top -1;
					}
					key_win = shtctl->sheets[j];
					cursor_c = keywin_on(key_win, sht_win, cursor_c);
				}

				if(256 <= i && i<= 511 && key_shift != 0 && task_cons->tss.ss0 != 0){ // shift + F1
					cons = (CONSOLE *) *((int *) 0x0fec);
					cons_putstr0(cons, "\nBreak(key): \n");
					io_cli();
					task_cons->tss.eax = (int) &(task_cons->tss.esp0);
					task_cons->tss.eip = (int) & asm_end_app;
					io_sti();
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
					case 256 + 0xba: // caps lock
						if(key_caps != 0){
							key_caps = 0;
						} else {
							key_caps = 1;
						}
						break;
					case 256 + 0x1c: // enter
						if(key_win != sht_win){ fifo32_put(&task_cons->fifo ,10 +256); }
						break;
					case 256 + 0xb8: // optionキーで画面の切り替え
						if(shtctl->top > 2){
							sheet_updown(shtctl->sheets[1], shtctl->top -1);
						}
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
					if((mdec.btn & 0x01)!=0){
						// 左ボタンを押している
						if(mmx < 0){
							// 通常モードの場合
							// 上の下敷きから順番にマウスが指している下敷きを探す
							for(j = shtctl->top -1 ; j>0; j--){
								sht = shtctl->sheets[j];
								x = mx - sht->vx0;
								y = my - sht->vy0;
								if(0 <= x && x< sht->bxsize && 0<=y && y < sht->bysize){
									if(sht->buf[y*sht->bxsize + x] != sht->col_inv){
										sheet_updown(sht, shtctl->top-1);
										if(sht != key_win){
											cursor_c = keywin_off(key_win, sht_win, cursor_c, cursor_x);
											key_win = sht;
											cursor_c = keywin_on(key_win, sht_win, cursor_c);
										}
										if(3 <= x && x < sht->bxsize-3 && 3 <= y && y < 21){
											mmx = mx; // ウィンドウ移動モード
											mmy = my;
										}
										if(sht->bxsize - 21 <= x && x< sht->bxsize - 5 && 5 <= y && y < 19){
											if((sht->flags & 0x10) != 0){ 	// アプリが作ったウィンドウかどうか
												if(sht->task != 0){
													cons = (CONSOLE *) *((int *) 0x0fec);
													cons_putstr0(cons, "\nBreak(mouse): \n");
													io_cli();
													task_cons->tss.eax = (int) &(task_cons->tss.esp0);
													task_cons->tss.eip = (int) asm_end_app;
													io_sti();
												}
											}
										}
										break;
									}
								} 
							}	
						} else {
							// ウィンドウ移動モードのとき
							x = mx - mmx;
							y = my - mmy;
							sheet_slide(sht, sht->vx0+x, sht->vy0+y);
							mmx = mx;
							mmy = my;
						}						
					} else {
						// 左ボタンを押していない
						mmx = -1;
					}
				}	
			}
	
			switch (i)
				{
					case 0: // カーソルタイマ
						timer_init(timer, &fifo, 1); // 次は1 
						if(cursor_c >=0){
							cursor_c = COL8_FFFFFF;
						}
						timer_settime(timer, 50);
						if(cursor_c >=0){
							boxfill8(sht_win->buf, sht_win->bxsize, cursor_c, cursor_x, 28, cursor_x + 7, 43);					
							sheet_refresh(sht_win, cursor_x, 28, cursor_x + 8, 44);
						}
						break;
					case 1: // カーソルタイマ
						timer_init(timer, &fifo, 0);
						if(cursor_c >=0){
							cursor_c = COL8_000000;
						}
						timer_settime(timer, 50);
						if(cursor_c >=0){
							boxfill8(sht_win->buf, sht_win->bxsize, cursor_c, cursor_x, 28, cursor_x + 7, 43);					
							sheet_refresh(sht_win, cursor_x, 28, cursor_x + 8, 44);
						}
						break;
				}
		}
	}
}

int keywin_on(SHEET *key_win, SHEET *sht_win, int cur_c){
	change_wtitle8(key_win, 1);
	if(key_win == sht_win){
		cur_c = COL8_000000;
	} else {
		if((key_win->flags & 0x20) != 0){
			fifo32_put(&key_win->task->fifo, 2); // コンソールのカーソルON
		}
	}
	return cur_c;
}
int keywin_off(SHEET *key_win, SHEET *sht_win, int cur_c, int cur_x){
	change_wtitle8(key_win, 0);
	if(key_win == sht_win){
		cur_c = -1;
		boxfill8(sht_win->buf, sht_win->bxsize, COL8_FFFFFF, cur_x, 28, cur_x+7, 43);
	} else {
		if((key_win->flags &0x20) != 0){
			fifo32_put(&key_win->task->fifo, 3); // コンソールのカーソルOFF
		}
	}
	return cur_c;
}
void change_wtitle8(SHEET * sht, char act){
	int x, y, xsize = sht->bxsize;
	char c, tc_new, tbc_new, tc_old, tbc_old, *buf = sht->buf;
	if(act != 0){
		tc_new = COL8_FFFFFF;
		tbc_new = COL8_000084;
		tc_old = COL8_C6C6C6;
		tbc_old = COL8_848484;
	} else {
		tc_new = COL8_C6C6C6;
		tbc_new = COL8_848484;
		tc_old = COL8_FFFFFF;
		tbc_old = COL8_000084;
	}
	for(y = 3; y<= 20; y++){
		for(x =3; x <= xsize -4; x++){
			c = buf[y*xsize + x];
			if(c == tc_old && x <= xsize -22){
				c = tc_new;
			} else if(c == tbc_old){
				c = tbc_new;
			}
			buf[y*xsize + x] = c;
		}	
	}
	sheet_refresh(sht, 3, 3, xsize, 21);
	return;
}