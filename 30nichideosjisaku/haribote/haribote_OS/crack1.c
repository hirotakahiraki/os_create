void api_end(void);

void HariMain(){
    *((char *) 0x00102600) = 0;
    api_end();
}