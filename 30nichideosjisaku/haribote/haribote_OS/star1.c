int api_openwin(char *buf, int xsiz, int ysiz, int col_inv, char *title);
void api_boxfilwin(int win, int x0, int y0, int x1, int y1, int col);
void api_initmalloc();
char *api_malloc(int size);
void api_point(int win, int x, int y, int col);
void api_end();

void HariMain(){

    api_initmalloc();
    char *buf = api_malloc(150*100);
    int win = api_openwin(buf, 150, 100, -1, "star1");
    api_boxfilwin(win, 6, 26, 143, 93, 0); // 黒
    api_point(win, 75, 59, 3); // 黒
    api_end();
}
