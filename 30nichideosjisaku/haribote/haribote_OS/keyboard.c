#include "bootpack.h"
FIFO32 *keyfifo;
int keydata0;
/* ps/2キーボードからの割り込み */
void inthandler21(int *esp){

    int data;
    io_out8(PIC0_OCW2, 0x61);
    data = io_in8(PORT_KEYDAT);
    fifo32_put(keyfifo, data + keydata0);
    return;
}

void wait_KBC_sendReady(void){
	for(;;){
		if((io_in8(PORT_KEYSTA) & KEYSTA_SEND_NOTREADY) ==0 ){
			break;
		}
	}
	return;
}

void init_keyboard(FIFO32 *fifo, int data0){
	keyfifo = fifo;
	keydata0 = data0;
	wait_KBC_sendReady();
	io_out8(PORT_KEYCMD, KEYCMD_WRITE_MODE);
	wait_KBC_sendReady();
	io_out8(PORT_KEYDAT, KBC_MODE);
	return;
}

