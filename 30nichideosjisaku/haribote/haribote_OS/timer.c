#include  "bootpack.h"
TIMERCTL timerctl;

void init_pit(void){
    int i;
    TIMER *t;
    io_out8(PIT_CTRL, 0x34);
    io_out8(PIT_CNT0, 0x9c);
    io_out8(PIT_CNT0, 0x2e);
    timerctl.count = 0;
    for(i = 0; i< MAX_TIMER; i++){
        timerctl.timers0[i].flags = 0;
    }
    t = timer_alloc(); // 1つもらってくる
    t->timeout = 0xffffffff;
    t->flags = TIMER_FLAGS_USING;
    t->next = 0;
    timerctl.t0 = t;
    timerctl.next = 0xffffffff;
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
    t = timerctl.t0;
    if(timer->timeout <= t->timeout){
        // 先頭に入れる場合
        timerctl.t0 = timer;
        timer->next = (struct TIMER*)t;
        timerctl.next = timer->timeout;
        io_store_eflags(e);
        return;
    }
    // どこに入れれば良いかを探す
    for(;;){
        s = t;
        t =(TIMER*)t->next;
        if(timer->timeout <= t->timeout){
            // sとtの間に入れる場合
            s->next = (struct TIMER *)timer;
            timer->next = (struct TIMER *)t;
            io_store_eflags(e);
            return;
        }
    }
    return;
}

void inthandler20(int *esp){
    TIMER *timer;
    char ts = 0;
    io_out8(PIC0_OCW2, 0x60); 
    timerctl.count +=1; 
    if(timerctl.next > timerctl.count){
        return;
    }
    timer = timerctl.t0;
    for(;;){
        // timersのタイマは全て動作中のものなのでflagsを認識しない
        if(timer->timeout > timerctl.count){
            break;
        }
        // タイムアウト
        timer->flags = TIMER_FLAGS_ALLOC;
        if(timer != task_timer){
            fifo32_put(timer->fifo, timer->data);
        } else {
            ts = 1;
        }
        timer = (TIMER *)timer->next;
      }      
    timerctl.t0 = timer;
    timerctl.next = timerctl.t0->timeout;
    if(ts != 0){
        task_switch();
    }
    return;
}
