#include "bootpack.h"

unsigned int memtest(unsigned int start, unsigned int end){
	char flg486 = 0;
	unsigned int eflg, cr0, i;

	/* 386か486以降なのか確認 */
	eflg = io_load_eflags();
	eflg |= EFLAGS_AC_BIT;  /* AC-bit = 1 */
	io_store_eflags(eflg);
	eflg = io_load_eflags();
	if((eflg & EFLAGS_AC_BIT) != 0 ){ /* 386ではAC=1にしても自動で0に戻ってしまう */
		flg486 = 1;
	}
	eflg &= ~EFLAGS_AC_BIT; /* AC-bit =0 */
	io_store_eflags(eflg);

	if(flg486 != 0){
		cr0 = load_cr0();
		cr0 |= CR0_CACHE_DISABLE; /* キャッシュ禁止 */
		store_cr0(cr0);
	}

	i = memtest_sub(start, end);

	if( flg486 !=0 ){
		cr0 = load_cr0();
		cr0 &= ~CR0_CACHE_DISABLE; /* キャッシュ許可 */
		store_cr0(cr0); 
	}

	return i;
}

void memman_init(MEMMAN *man){
	man->frees = 0;		/* 空き情報の個数 */
	man->maxfrees = 0;	/* 状況観察用:frees の最大値 */
	man->lostsize = 0; 	/* 解放に失敗した合計サイズ */
	man->losts = 0;		/* 解放に失敗した回数 */
	return;
}

unsigned int memman_total(MEMMAN *man){
	/* 空きサイズの合計を報告 */
	unsigned int i, t =0;
	for(i = 0;i<man->frees; i++){
		t += man->free[i].size; 
	}
	return t;
}

unsigned int  memman_alloc(MEMMAN *man, unsigned int size){
	/* 確保 */
	unsigned int i, a;
	for(i = 0;i <man->frees; i++){
		if(man->free[i].size >=size){
			/* 十分な量の空きを発見 */
			a = man->free[i].addr;
			man->free[i].addr += size;
			man->free[i].size -= size;
			if(man->free[i].size == 0){
				/* free[i]がなくなったので前に詰める */
				man->frees -=1;
				for(;i< man->frees; i++){
					man->free[i] = man->free[i+1]; /* 構造体の代入 */
				}
			}
			return a;
		}
	}
	return 0; /* 空きがない */
}

int memman_free(MEMMAN *man, unsigned int addr, unsigned int size){
	/* 解放 */
	int i, j;
	/* まとめやすさを考えるとfree[]がaddr側にあったほうが良い */
	for(i=0;i< man->frees; i++){
		if(man->free[i].addr > addr){
			break;
		}
	}
	/* free[i-1].addr < addr < free[i+1].addr */
	if(i >0){
		/* 前がある */
		if(man->free[i-1].addr + man->free[i-1].size == addr){
			/* 一つ前にまとめる */
			man->free[i-1].size += size; 
			if(i < man->frees){	
				/* 後ろもある */
				if(addr + size == man->free[i].addr){
					/* 後ろもまとめられる */		
					man->free[i-1].size += man->free[i].size;
					man->frees -=1;
					for(;i< man->frees; i++){
						man->free[i] = man->free[i+1]; /* 構造体の代入 */
					}
				}
			}
		return 0;	/* 成功終了 */
		}
	}
	/* 前とはまとめられない */
	if( i < man->frees){
		/* 後ろあり */
		if(addr + size == man->free[i].addr){
			/* 後ろはまとめられた */
			man->free[i].addr =addr;
			man->free[i].size += size;
			return 0; /* 成功終了 */
		}
	}
	/* 前とも後ろとも無理 */
	if(man->frees < MEMMAN_FREES){
		/* free[i]より後ろを後ろへずらし、すきまを作る */
		for(j = man->frees;j>i;j--){
			man->free[j] = man->free[j-i];
		}
		man->frees +=1;
		if(man->maxfrees <man->frees){
			man->maxfrees = man->frees; /* 最大値を更新 */
		}
		man->free[i].addr = addr;
		man->free[i].size = size;
		return 0; /* 成功終了 */
	}
	/* 後ろにずらせない */
	man->losts +=1;
	man->lostsize +=size;
	return -1; /* 解放終了 */
}

unsigned int memman_alloc_4k(MEMMAN *man, unsigned int size){
	unsigned int a;
	size = (size + 0xfff) & 0xfffff000;
	a = memman_alloc(man, size);
	return a;
}

int memman_free_4k(MEMMAN *man, unsigned int addr, unsigned int size){
	int i;
	size = (size +0xfff) & 0xfffff000;
	i = memman_free(man, addr, size);
	return i;
}