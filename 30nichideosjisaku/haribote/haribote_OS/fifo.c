#include "bootpack.h"

void fifo8_init(FIFO8 *fifo, int size, unsigned char *buf){
    fifo->size = size;
    fifo->buf = buf;
    fifo->free = size;
    fifo->flags = 0;
    fifo->p = 0; /* next_w */
    fifo->q = 0; /* next_r */
    return;
}

int fifo8_put(FIFO8 *fifo, unsigned char data){
    if(fifo->free ==0 ){
        fifo->flags |= FLAGS_OVERRUN;
        return -1;
    }
    fifo->buf[fifo->p] = data;
    fifo->p += 1;
    if(fifo->p == fifo->size){
        fifo->p = 0;
    }
    fifo->free -= 1;
    return 0;
}

int fifo8_get(FIFO8 *fifo){
    int data;
    if(fifo->free == fifo->size){
        return -1;
    }
    data = fifo->buf[fifo->q];
    fifo->q += 1;
    if(fifo->q == fifo->size){
        fifo->q = 0;
    }
    fifo->free += 1;
    return data;
}

int fifo8_status(FIFO8 *fifo){
    return fifo->size - fifo->free;
}