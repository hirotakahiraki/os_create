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
    for(i =0; i < MAX_TASKLEVELS; i++){
        taskctl->level[i].running = 0;
        taskctl->level[i].now = 0;
    }
    task = task_alloc();
    task->flags = 2; // 動作中
    task->priority = 2; // 0.02秒
    task->level = 0; // 最高レベル
    task_add(task);
    task_switchsub(); // レベル設定
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

void task_run(TASK *task, int level, int priority){
    if(level < 0){
        level = task->level;
    }
    if(priority > 0){
        task->priority = priority;
    }
    // 動作中のレベルの変更
    if(task->flags == 2 && task->level != level){
        task_remove(task);
    }
    if(task->flags != 2){
        // sleepから起こされる場合
        task->level = level;
        task_add(task);
    }
    taskctl->lv_change = 1; // 次回タクトスイッチのときにレベルを見直す
    return;
}

void task_switch(){
    TASKLEVEL *tl = &taskctl->level[taskctl->now_lv];
    TASK *new_task, *now_task = tl->tasks[tl->now];
    tl->now +=1;
    if(tl->now == tl->running){
        tl->now = 0;
    }
    if(taskctl->lv_change != 0){
        task_switchsub();
        tl = &taskctl->level[taskctl->now_lv];
    }
    new_task = tl->tasks[tl->now];
    timer_settime(task_timer, new_task->priority);
    if(new_task != now_task){
        farjmp(0, new_task->sel);
    }
    return;
}

void task_sleep(TASK *task){
    TASK *now_task;
    // 指定タスクが起きていたら
    if(task->flags == 2){
        now_task = task_now();
        task_remove(task);
        if(task == now_task){
            // 自分自身がスリープだったのでタスクスイッチが必要
            task_switchsub();
            now_task = task_now(); // 設定後での、現在のタスクを教えてもらう
            farjmp(0, now_task->sel);
        }
    }
    return;
}

TASK *task_now(){
    TASKLEVEL *tl = &taskctl->level[taskctl->now_lv];
    return tl->tasks[tl->now];
}

void task_add(TASK *task){
    TASKLEVEL *tl = &taskctl->level[task->level];
    tl->tasks[tl->running] = task;
    tl->running += 1;
    task->flags = 2; // 動作中
    return;
}

void task_remove(TASK *task){
    int i;
    TASKLEVEL *tl = &taskctl->level[task->level];

    // taskがどこにいるかを表す
    for(i = 0; i< tl->running; i++){
        // いた場合
        if(tl->tasks[i] == task){
            break;
        }
    }
    tl->running -= 1;
    if(i< tl->now){
        tl->now -= 1;
    }
    // nowがおかしな値になっていたら修正
    if(tl->now >= tl->running){
        tl->now = 0;
    }
    task->flags = 1;
    // ずらし
    for(;i > tl->running; i++){
        tl->tasks[i] = tl->tasks[i+1];
    }
    return;
}

void task_switchsub(void){
    int i;
    // 一番上のレベルを探す
    for(i =0;i < MAX_TASKLEVELS; i++){
        if(taskctl->level[i].running > 0){
            break; // 見つかった
        }
    }
    taskctl->now_lv = i;
    taskctl->lv_change = 0;
    return;
}