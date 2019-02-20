#include  "bootpack.h"
TIMERCTL timerctl;

void init_pit(void){
    int i;
    io_out8(PIT_CTRL, 0x34);
    io_out8(PIT_CNT0, 0x9c);
    io_out8(PIT_CNT0, 0x2e);
    timerctl.count = 0;
    timerctl.next = 0xffffffff;
    timerctl.using = 0;
    for(i=0 ; i<MAX_TIMER ;i++){
        timerctl.timers0[i].flags =0; /* 未使用 */
    }
    return;
}

TIMER *timer_alloc(void){
    int i;
    for(i=0;i<MAX_TIMER; i++){
        if(timerctl.timers0[i].flags == 0){
            timerctl.timers0[i].flags = TIMER_FLAGS_ALLOC;
            return &timerctl.timers0[i];
        }
    }
    return 0;
}

void timer_free(TIMER *timer){
    timer->flags = 0; // 未使用 
    return;
}

void timer_init(TIMER *timer, FIFO32 *fifo, int data){
    timer->fifo = fifo;
    timer->data = data;
    return;
}

void timer_settime(TIMER *timer, unsigned int timeout){
    int e;
    TIMER *t, *s;
    timer->timeout = timeout + timerctl.count;
    timer->flags = TIMER_FLAGS_USING;
    e = io_load_eflags();
    io_cli();
    timerctl.using +=1;
    if(timerctl.using ==1){
        // 動作中のタイマはこれ1つになる場合
        timerctl.t0 = timer;
        timer->next = 0;
        timerctl.next = timer->timeout;
        io_store_eflags(e);
        return;
    }
    t = timerctl.t0;
    if(timer->timeout <= t->timeout){
        // 先頭に入れる場合
        timerctl.t0 = timer;
        timer->next = t;
        timerctl.next = timer->timeout;
        io_store_eflags(e);
        return;
    }
    // どこに入れれば良いかを探す
    for(;;){
        s = t;
        t =(TIMER*)t->next;
        if(t == 0){
            break;
        }
        if(timer->timeout <= t->timeout){
            // sとtの間に入れる場合
            s->next = (struct TIMER *)timer;
            timer->next = t;
            io_store_eflags(e);
            return;
        }
    }
    // 一番後ろに入れる
    s->next = (struct TIMER *) timer;
    timer->next = (struct TIMER*) timer;     
    io_store_eflags(e);
    return;
}

void inthandler20(int *esp){
    int i, j;
    TIMER *timer;
    io_out8(PIC0_OCW2, 0x60); 
    timerctl.count +=1; 
    if(timerctl.next > timerctl.count){
        return;
    }
    timer = timerctl.t0;
    for(i=0; i< timerctl.using ; i++){
        // timersのタイマは全て動作中のものなのでflagsを認識しない
        if(timer->timeout > timerctl.count){
            break;
        }
        // タイムアウト
        timer->flags = TIMER_FLAGS_ALLOC;
        fifo32_put(timer->fifo, timer->data);
        timer = (TIMER *)timer->next;
      }      
    // i個のタイマがタイムアウトしたので残りをずらす
    timerctl.using -= i;
    timerctl.t0 = timer;

    if(timerctl.using > 0){
        timerctl.next = timerctl.t0->timeout;
    } else {
        timerctl.next = 0xffffffff;
    }
    return;
}

