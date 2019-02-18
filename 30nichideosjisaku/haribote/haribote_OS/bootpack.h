// struct 
typedef struct {
	char cyls, leds, vmode, reverse;
	short scrnx,scrny;
	char  *vram;
}BOOTINFO;

#define ADR_BOOTINFO    0x00000ff0

/* color setting */
#define COL8_000000		0
#define COL8_FF0000		1
#define COL8_00FF00		2
#define COL8_FFFF00		3
#define COL8_0000FF		4
#define COL8_FF00FF		5
#define COL8_00FFFF		6
#define COL8_FFFFFF		7
#define COL8_C6C6C6		8
#define COL8_840000		9
#define COL8_008400		10
#define COL8_848400		11
#define COL8_000084		12
#define	COL8_840084		13
#define COL8_008484		14
#define COL8_848484		15 

/* naskfunc.nas */
void io_hlt(void);
void io_cli(void);
void io_sti(void);
void io_stihlt(void);
void io_out8(int port,int data);
int io_in8(int port);
int io_load_eflags(void);
void io_store_eflags(int eflags);

void write_mem8(int addr, int data);
void load_gdtr(int limit, int addr);
void load_idtr(int limit, int addr);
void asm_inthandler20(void);
void asm_inthandler21(void);
void asm_inthandler27(void);
void asm_inthandler2c(void);
unsigned int memtest_sub(unsigned int start, unsigned int end);

int load_cr0(void);
void store_cr0(int cr0);

/* graphic.c */
void init_palette(void);
void set_palette(int start, int end, unsigned char *rgb);
void boxfill8(unsigned char *vram, int xsize, unsigned char c, int x0, int y0, int x1, int y1);
void init_screen(char *vram, int x, int y);
void putfont8(char *vram, int xsize, int x, int y, char c, char *font);
void putfonts8_asc(char *vram, int xsize, int x, int y, char c, char *s);
void init_mouse_cursor8(char *mouse, char bc);
void putblock8_8(char *vram, int vxsize, int pxsize,int pysize,int px0, int py0, char *buf, int bxsize);

/* dsctbl.c */
typedef struct{
	short limit_low, base_low;
	char base_mid, access_right;
	char limit_high, base_high;
}SEGMENT_DISCRIPTOR;

typedef struct 
{
	short offset_low, selector;
	char dw_count, access_right;
	short offset_high;
}GATE_DISCRIPTOR;
void init_gdtidt(void);
void set_segmdesc(SEGMENT_DISCRIPTOR *sd, unsigned int limit, int base, int ar);
void set_gatedesc(GATE_DISCRIPTOR *gd, int offset, int selector, int ar);
#define ADR_IDT         0x0026f800
#define LIMIT_IDT       0x000007ff
#define ADR_GDT         0x00270000
#define LIMIT_GDT       0x0000ffff
#define ADR_BOTPAK      0x00280000
#define LIMIT_BOTPAK    0x0007ffff
#define AR_DATA32_RW    0x4092
#define AR_CODE32_ER    0x409a
#define AR_INTGATE32    0x008e

/* int.c */
typedef struct {
	unsigned char data[32];
	int next_r, next_w, len;
}KEYBUF;

void init_pic(void);
void inthandler2c(int *esp);

#define PIC0_ICW1       0x0020
#define PIC0_OCW2       0x0020
#define PIC0_IMR        0x0021
#define PIC0_ICW2       0x0021
#define PIC0_ICW3       0x0021
#define PIC0_ICW4       0x0021
#define PIC1_ICW1       0x00a0
#define PIC1_OCW2       0x00a0
#define PIC1_IMR        0x00a1
#define PIC1_ICW2       0x00a1
#define PIC1_ICW3       0x00a1
#define PIC1_ICW4       0x00a0


/* fifo.c */
typedef struct{
	unsigned char *buf;
	int p, q, size, free, flags;
}FIFO8;
void fifo8_init(FIFO8 *fifo, int size, unsigned char *buf);
int fifo8_put(FIFO8 *fifo, unsigned char data);
int fifo8_get(FIFO8 *fifo);
int fifo8_status(FIFO8 *fifo);
#define FLAGS_OVERRUN		0x0001

/* mouse.c */
typedef struct 
{
	unsigned char buf[3],phase;
	int x, y, btn;
}MOUSE_DEC;
void inthandler27(int *esp);
void enable_mouse(MOUSE_DEC *mdec);
int mouse_decode(BOOTINFO *binfo, MOUSE_DEC *mdec, unsigned char data);

/* keyboard.c */
void inthandler21(int *esp);
void wait_KBC_sendReady(void);
void init_keyboard(void);

#define PORT_KEYDAT				0x0060
#define PORT_KEYSTA				0x0064
#define PORT_KEYCMD				0x0064
#define KEYSTA_SEND_NOTREADY	0x02
#define KEYCMD_WRITE_MODE		0x60
#define KBC_MODE				0x47
#define KEYCMD_SENDTO_MOUSE		0xd4
#define MOUSECMD_ENABLE			0xf4

/* memory.c */
#define	MEMMAN_FREES			4090	/* 約32kB*/
#define MEMMAN_ADDR				0x003c0000
typedef struct /* 空き情報 */{
	unsigned int addr, size;
}FREEINFO;

typedef struct /* メモリ管理 */{
	int frees, maxfrees, lostsize, losts;
	FREEINFO free[MEMMAN_FREES];
} MEMMAN;

unsigned int memtest(unsigned int start, unsigned int end);
void memman_init(MEMMAN *man);
unsigned int memman_total(MEMMAN *man);
unsigned int  memman_alloc(MEMMAN *man, unsigned int size);
int memman_free(MEMMAN *man, unsigned int addr, unsigned int size);
unsigned int memman_alloc_4k(MEMMAN *man, unsigned int size);
int memman_free_4k(MEMMAN *man, unsigned int addr, unsigned int size);

/* sheet.c */
#define MAX_SHEETS				256
#define SHEET_USE				1
typedef struct {
	unsigned char *buf;
	int bxsize, bysize, vx0, vy0, col_inv, height, flags;
	struct SHTCTL *ctl;
}SHEET;

typedef struct {
	unsigned char *vram, *map;
	int xsize, ysize, top;
	SHEET *sheets[MAX_SHEETS];
	SHEET sheets0[MAX_SHEETS];
}SHTCTL;

SHTCTL *shtctl_init(MEMMAN *memman, unsigned char *vram, int xsize, int ysize);
SHEET *sheet_alloc(SHTCTL *ctl);
void sheet_setbuf(SHEET *sht, unsigned char *buf, int xsize, int ysize, int col_inv);
void sheet_updown(SHEET *sht, int height);
void sheet_refresh(SHEET *sht, int bx0, int by0, int bx1, int by1);
void sheet_slide(SHEET *sht, int vx0, int vy0);
void sheet_free(SHEET *sht);
void sheet_refreshsub(SHTCTL *ctl, int vx0, int vy0, int vx1, int vy1, int h0, int h1);
void sheet_refreshmap(SHTCTL *ctl, int vx0, int vy0, int vx1, int vy1, int h0);

/* timer.c */
#define PIT_CTRL				0x0043
#define PIT_CNT0				0x0040
#define MAX_TIMER				500
#define TIMER_FLAGS_ALLOC		1 /* 確保 */
#define TIMER_FLAGS_USING		2 /* タイマ作動中 */
typedef struct{
	unsigned int timeout, flags;
	FIFO8 *fifo;
	unsigned char data;
}TIMER;

typedef struct{
	unsigned int count, next, using;
	TIMER *timers[MAX_TIMER];
	TIMER timers0[MAX_TIMER]; 
}TIMERCTL;
void init_pit(void);
TIMER *timer_alloc(void);
void timer_free(TIMER *timer);
void timer_init(TIMER *timer, FIFO8 *fifo, unsigned char data);
void timer_settime(TIMER *timer, unsigned int timeout);
void inthandler20(int *esp);

/* bootpack.c */
#define EFLAGS_AC_BIT			0x00040000
#define CR0_CACHE_DISABLE		0x60000000
void make_window8(unsigned char *buf, int xsize, int ysize, char *title);
