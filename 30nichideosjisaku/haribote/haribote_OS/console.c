#include "bootpack.h"
#include <stdio.h>
#include <string.h>

void console_task(SHEET *sheet, unsigned int memtotal){
	TIMER * timer;
	TASK *task = task_now();

	int i, x, y, fifobuf[128], cursor_x = 16, cursor_y = 28, cursor_c = -1;
	char s[30],cmdline[30], *p;
	MEMMAN *memman = (MEMMAN *) MEMMAN_ADDR;
	FIFOINFO *finfo = (FIFOINFO *) (ADR_DISKIMG + 0x002600);
	int *fat = (int *)memman_alloc_4k(memman, 4*2880); 
	file_readfat(fat, (unsigned char *) (ADR_DISKIMG + 0x000200));
    SEGMENT_DISCRIPTOR *gdt = (SEGMENT_DISCRIPTOR *) ADR_GDT;

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
					} else if(strncmp(cmdline, "cat ", 4)==0){
						for(y =0;y<11;y++){
							s[y] = ' ';
						}
						y = 0;
						// xはコマンドの文字数+1から開始
						for(x = 4;y<11 && cmdline[x] != 0;x++){
							if(cmdline[x] == '.' && y<=8){
								y = 8;
							} else {
								s[y] = cmdline[x];
								if('a' <= s[y] && s[y] <= 'z'){
									s[y] -= 0x20; // 小文字を大文字に
								}
								y++;
							}
						}
						// ファイルを探す
						for(x = 0;x<224;){
							if(finfo[x].name[0]==0x00){break;}
							if((finfo[x].type & 0x18)==0){
								for(y =0 ;y<11;y++){
									if(finfo[x].name[y] != s[y]){
										goto type_next_file;
									}
								}
								break;
							}
type_next_file:
						x++;
						}
						if(x<224 && finfo[x].name[0] != 0x00){ // ファイルが見つかった
							
							p = (char *) memman_alloc_4k(memman, finfo[x].size);
							file_loadfile(finfo[x].clustno, finfo[x].size, p, fat, (char *)(ADR_DISKIMG+ (0x003e00)));
							cursor_x = 8;
							for(y = 0; y<finfo[x].size; y++){
								//一文字づつ出力
								s[0] = p[y];
								s[1] = 0;
								if(s[0] == 0x09){
									for(;;){
										putfonts8_asc_sht(sheet, cursor_x, cursor_y, COL8_FFFFFF, COL8_000000, " ", 1);
										cursor_x += 8;
										if(cursor_x == 8 +240){
											cursor_x = 8;
											cursor_y = cons_newline(cursor_y, sheet);
										}
										if(((cursor_x - 8) & 0x1f) == 0){ break; } // 32で割り切れたらbreak}

									}
									memman_free_4k(memman, (int) p, finfo[x].size);
								} else if(s[0] == 0x0a){ // 改行
									cursor_x = 8;
									cursor_y = cons_newline(cursor_y, sheet);
								} else if(s[0] == 0x0d){ // 復帰
									// とりあえずなにもしない
								} else { // 普通の文字
									putfonts8_asc_sht(sheet, cursor_x, cursor_y, COL8_FFFFFF, COL8_000000, s, 1);
									cursor_x += 8;
									if(cursor_x == 8+240){
										cursor_x = 8;
										cursor_y = cons_newline(cursor_y, sheet);
									}
								}
							}
						} else { // ファイルが見つからなかった
							putfonts8_asc_sht(sheet, 8, cursor_y, COL8_FFFFFF, COL8_000000, "file not found", 15);
							cursor_y = cons_newline(cursor_y, sheet);
						}
						cursor_y = cons_newline(cursor_y, sheet);

					} else if(strcmp(cmdline, "hlt") == 0){
                        for(y =0; y<11; y++){
                            s[y] = ' ';
                        }
                        s[0] = 'H';
                        s[1] = 'L';
                        s[2] = 'T';
                        s[8] = 'H';
                        s[9] = 'R';
                        s[10]= 'B';
                        for(x=0; x<224;){
                            if(finfo[x].name[0] == 0x00){
                                break;
                            }
                            if((finfo[x].type & 0x18)==0){
                                for(y=0; y<11; y++){
                                    if(finfo[x].name[y] != s[y]){
                                        goto hlt_next_file;
                                    }
                                }
                                break; // ファイルが見つかった
                            }
            hlt_next_file:
                            x+=1;
                        }
                        if(x <224 && finfo[x].name[0] != 0x00){  // ファイルが見つかった
                           p = (char *) memman_alloc_4k(memman, finfo[x].size);
                           file_loadfile(finfo[x].clustno, finfo[x].size, p, fat, (char *)(ADR_DISKIMG + 0x003e00));
                           set_segmdesc(gdt + 1003, finfo[x].size - 1, (int)p, AR_CODE32_ER);
                           farjmp(0, 1003*8);
                           memman_free_4k(memman, (int) p, finfo[x].size);
                        } else { // ファイルがなくなった
                            putfonts8_asc_sht(sheet, 8, cursor_y, COL8_FFFFFF, COL8_000000, "File not found", 15);
                            cursor_y = cons_newline(cursor_y, sheet);
                        }
                        cursor_y = cons_newline(cursor_y, sheet);
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