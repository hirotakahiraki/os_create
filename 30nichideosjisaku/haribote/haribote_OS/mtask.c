#include "bootpack.h"
TIMER *task_timer;
TASKCTL *taskctl;
int mt_tr;

TASK *task_init(MEMMAN *memman){
    int i;
    TASK *task;
    SEGMENT_DISCRIPTOR *gdt = (SEGMENT_DISCRIPTOR *) ADR_GDT;
    taskctl = (TASKCTL *) memman_alloc_4k(memman, sizeof(TASKCTL));
    for(i = 0; i<MAX_TASK; i++){
        taskctl->tasks0[i].flags = 0;
        taskctl->tasks0[i].sel = (TASK_GDT0 + i) * 8;
        set_segmdesc(gdt + TASK_GDT0 + i, 103, (int) &taskctl->tasks0[i].tss, AR_TSS32);
    }
    task = task_alloc();
    task->flags = 2; // 動作中
    task->priority = 2; // 0.02秒
    taskctl->running = 1;
    taskctl->now = 0;
    taskctl->tasks[0] = task;
    load_tr(task->sel);
    task_timer = timer_alloc();
    timer_settime(task_timer, task->priority);
    return task;
}

TASK *task_alloc(){
    int i;
    TASK *task;
    for(i =0 ;i< MAX_TASK; i++){
        if(taskctl->tasks0[i].flags == 0){
            task = &taskctl->tasks0[i];
            task->flags = 1;
            task->tss.eflags = 0x00000202;
            task->tss.eax = 0;
            task->tss.ecx = 0;
            task->tss.edx = 0;
            task->tss.ebx = 0;
            task->tss.ebp = 0;
            task->tss.esi = 0;
            task->tss.edi = 0;
            task->tss.es = 0;
            task->tss.ds = 0;
            task->tss.fs = 0;
            task->tss.gs = 0;
            task->tss.ldtr = 0;
            task->tss.iomap = 0x40000000;
            return  task;
        }
    }
    return 0; 
}

void task_run(TASK *task, int priority){
    if(task->priority != 2){
        task->priority = priority;
    }
    if(task->flags = 2){
    task->flags = 2; // 動作中
    taskctl->tasks[taskctl->running] = task;
    taskctl->running +=1;
    }
    return;
}

void task_switch(){
    TASK *task;
    taskctl->now +=1;
    if(taskctl->now == taskctl->running){
        taskctl->now = 0;
    }
    task = taskctl->tasks[taskctl->now];
    timer_settime(task_timer, task->priority);
    if(taskctl->running >= 2){
        farjmp(0, task->sel);
    }
    return;
}

void task_sleep(TASK *task){
    int i;
    char ts = 0;
    // 指定タスクが起きていたら
    if(task->flags == 2){
        // 自分自身を寝かせるのであとでタスクスイッチする。
        if(task == taskctl->tasks[taskctl->now]){
            ts = 1;
        }
        // タスクがどこにいるかを探す
        for(i = 0;i < taskctl->running; i++){
            if(taskctl->tasks[i] == task){
                break;
            }
        }
        taskctl->running -= 1;
        if(i < taskctl->now){
            // ずれるので、これも合わせておく。
            taskctl->now -= 1;
        }
        // ずらし
        for(; i < taskctl->running; i++){
            taskctl->tasks[i] = taskctl->tasks[i+1];
        }
        // 動作していない状態
        task->flags = 1;
        if(ts != 0){
            // タクトスイッチする
            if(taskctl->now >= taskctl->running){
                // nowがおかしな値になっていたら修正する。
                taskctl->now = 0;
            }
            farjmp(0, taskctl->tasks[taskctl->now]->sel);
        }
    }
    return;
}