#include "bootpack.h"

SHTCTL *shtctl_init(MEMMAN *memman, unsigned char *vram, int xsize, int ysize){
    SHTCTL *ctl;
    int i;
    ctl = (SHTCTL *) memman_alloc_4k(memman, sizeof(SHTCTL));
    if(ctl ==0){
        goto err;
    }
    ctl->map = (unsigned char *) memman_alloc_4k(memman, xsize * ysize);
    if(ctl->map == 0){
        memman_free_4k(memman, (int)ctl, sizeof(SHTCTL));
        goto err;
    }

    ctl->vram = vram;
    ctl->xsize = xsize;
    ctl->ysize = ysize;
    ctl->top = -1; /* sheetは1枚もない */
    for(i =0; i<MAX_SHEETS; i++){
        ctl->sheets0[i].flags = 0; /* 未使用マーク */
        ctl->sheets0[i].ctl = (struct SHTCTL *)ctl;
    }
err:
    return ctl;
}

SHEET *sheet_alloc(SHTCTL *ctl){
    SHEET *sht;
    int i;
    for(i=0;i<MAX_SHEETS;i++){
        if(ctl->sheets0[i].flags == 0){
            sht = &ctl->sheets0[i];
            sht->flags = SHEET_USE; /* 使用中 */
            sht->height = -1; /* 非表示中 */ 
            return sht;
        }
    }
    return 0; /* 全て使用中 */
} 

void sheet_setbuf(SHEET *sht, unsigned char *buf, int xsize, int ysize, int col_inv){
    sht->buf = buf;
    sht->bxsize = xsize;
    sht->bysize = ysize;
    sht->col_inv = col_inv;
}

void sheet_updown(SHEET *sht, int height){
    int h, old = sht->height;
    SHTCTL *ctl =(SHTCTL *) sht->ctl;
    /* 低すぎ/高すぎの場合修正*/
    if(height > ctl->top + 1){
        height = ctl->top + 1;
    }
    if(height < -1){
        height = -1;
    }
    sht->height = height; /* 高さを修正 */

    /* sheets[]の並び替え */
    if(old > height){  /* 以前より低くなる */
        if(height >= 0){
            /* 間のものを引き上げる */
            for(h = old; h> height; h--){
                ctl->sheets[h] =ctl->sheets[h-1];
                ctl->sheets[h]->height = h;
            }
            ctl->sheets[height] = sht;
            sheet_refreshmap(ctl, sht->vx0, sht->vy0, sht->vx0 + sht->bxsize, sht->vy0 + sht->bysize, height+1);
            sheet_refreshsub(ctl, sht->vx0, sht->vy0, sht->vx0 + sht->bxsize, sht->vy0 + sht->bysize, height+1, old);
        }else{ 
            /* 非表示 */
            if(ctl->top > old){
                /* 上にあるものを下ろす */
                for(h=old;h< ctl->top;h++){
                    ctl->sheets[h] = ctl->sheets[h+1];
                    ctl->sheets[h]->height = h;
                }
            }
            ctl->top -=1;
        }
        sheet_refreshmap(ctl, sht->vx0, sht->vy0, sht->vx0 + sht->bxsize, sht->vy0 + sht->bysize, 0);
        sheet_refreshsub(ctl, sht->vx0, sht->vy0, sht->vx0 + sht->bxsize, sht->vy0 + sht->bysize, 0, old-1);
    }else if(old < height){ /* 以前より高くなる */
        if(old >= 0){
            /* 間のものを下げる */
            for(h=old; h<height; h++){
                ctl->sheets[h] = ctl->sheets[h+1];
                ctl->sheets[h]->height = h;
            }
            ctl->sheets[height] = sht;
        }else{ /* 非表示から表示へ */
            /* 上にあげる */
            for(h= ctl->top; h >=height;h--){
                ctl->sheets[h+1] = ctl->sheets[h];
                ctl->sheets[h+1]->height = h+1;
            }
            ctl->sheets[height] = sht;
            ctl->top +=1;
        }
        sheet_refreshmap(ctl, sht->vx0, sht->vy0, sht->vx0 + sht->bxsize, sht->vy0 + sht->bysize, height);
        sheet_refreshsub(ctl, sht->vx0, sht->vy0, sht->vx0 + sht->bxsize, sht->vy0 + sht->bysize, height, height);
    }
    return;
}

void sheet_refresh(SHEET *sht, int bx0, int by0, int bx1, int by1){
    
    if(sht->height >= 0){ /* 表示中なら、新しい下敷きの情報に従って書き直す */
        sheet_refreshsub((SHTCTL*)sht->ctl, sht->vx0 + bx0, sht->vy0 + by0, sht->vx0 + bx1, sht->vy0 + by1, sht->height, sht->height);
    }
    return;
}

void sheet_slide(SHEET *sht, int vx0, int vy0){
    int old_vx0 = sht->vx0, old_vy0 = sht->vy0;
    sht->vx0 = vx0;
    sht->vy0 = vy0;
    if(sht->height >= 0){ /* 表示中なら新しい下敷きの情報に沿って画面を書き出す */
        sheet_refreshmap((SHTCTL *)sht->ctl, old_vx0, old_vy0, old_vx0 + sht->bxsize, old_vy0 + sht->bysize, 0);
        sheet_refreshmap((SHTCTL *)sht->ctl, vx0, vy0, vx0 + sht->bxsize, vy0 + sht->bysize, sht->height);
        sheet_refreshsub((SHTCTL *)sht->ctl, old_vx0, old_vy0, old_vx0 + sht->bxsize, old_vy0 + sht->bysize, 0, sht->height -1);
        sheet_refreshsub((SHTCTL *)sht->ctl, vx0, vy0, vx0 + sht->bxsize, vy0 + sht->bysize, sht->height, sht->height);
    } 
    return;
}

void sheet_free(SHEET *sht){
    if(sht->height >=0){
        sheet_updown(sht, -1);
    }
    sht->flags = 0;
    return;
}

void sheet_refreshsub(SHTCTL *ctl, int vx0, int vy0, int vx1, int vy1, int h0, int h1){
    int h, bx, by, vx, vy, bx0, by0, bx1, by1;
    unsigned char *buf, *vram = ctl->vram, *map = ctl->map , sid;
    SHEET *sht;
    /* mouseの画面外処理 */
    if(vx0 < 0){ vx0 = 0;}
    if(vy0 < 0){ vy0 = 0;}
    if(vx1 > ctl->xsize){ vx1 = ctl->xsize;}
    if(vy1 > ctl->ysize){ vy1 = ctl->ysize;}
    for(h=h0;h <= h1;h++){
        sht = ctl->sheets[h];
        buf = sht->buf;
        sid = sht - ctl->sheets0;
        bx0 = vx0 - sht->vx0;
        by0 = vy0 - sht->vy0;
        bx1 = vx1 - sht->vx0;
        by1 = vy1 - sht->vy0;
        if(bx0 < 0) {bx0 = 0;}
        if(by0 < 0) {by0 = 0;}
        if(bx1 > sht->bxsize) bx1 = sht->bxsize;
        if(by1 > sht->bysize) by1 = sht->bysize;
        for(by = by0; by < by1; by++){
            vy = by + sht->vy0;
            for(bx = bx0; bx < bx1;bx++){
                vx = bx + sht->vx0; 
                if(map[vy * ctl->xsize + vx] == sid){
                    vram[vy*ctl->xsize +vx] = buf[by*sht->bxsize + bx];
                }
            }
        }
    }
    return;
}

void sheet_refreshmap(SHTCTL *ctl, int vx0, int vy0, int vx1, int vy1, int h0){
    int h, bx, by, vx, vy, bx0, by0, bx1, by1;
    unsigned char *buf, sid, *map = ctl->map;
    SHEET *sht;
    if(vx0 < 0) vx0 = 0;
    if(vy0 < 0) vy0 = 0; 
    if(vx1 > ctl->xsize) vx1 = ctl->xsize;
    if(vy1 > ctl->ysize) vy1 = ctl->ysize;
    for(h = h0; h<= ctl->top;h++){
        sht = ctl->sheets[h];
        sid = sht - ctl->sheets0; /* 番地の引き算が下敷き番号 */
        buf = sht->buf;
        bx0 = vx0 - sht->vx0;
        by0 = vy0 - sht->vy0;
        bx1 = vx1 - sht->vx0;
        by1 = vy1 - sht->vy0;
        if(bx0 < 0) bx0 = 0;
        if(by0 < 0) by0 = 0;
        if(bx1 > sht->bxsize) bx1 = sht->bxsize;
        if(by1 > sht->bysize) by1 = sht->bysize;
        for(by = by0; by <by1; by++){
            vy = sht->vy0 + by;
            for(bx = bx0; bx<bx1; bx++){
                vx = sht->vx0 + bx;
                if(buf[by*sht->bxsize + bx]!= sht->col_inv){
                    map[vy * ctl->xsize +vx] = sid;
                }
            }
        }
    }
    return;
}
