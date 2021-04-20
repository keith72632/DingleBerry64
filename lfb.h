void lfb_init();
void lfb_print(int x, int y, char *s);
void lfb_proprint(int x, int y, char *s);
void drawPixel(int x, int y, unsigned char attribute);
void drawLine(int x1, int y1, int x2, int y2, unsigned char attr);
void drawRect(int x1, int y1, int x2, int y2, unsigned char attr, int fill);
void drawCircle(int x0, int y0, int radius, unsigned char attr, int fill);



typedef enum {
    BLACK = 0x000000, // black 
    RED = 0x0000AA, // red
    GREEN = 0x00AA00, // green
    BABY_SHIT = 0x00AAAA, // baby shit
    DARK_BLUE = 0xAA0000, // dark blue
    VIOLET = 0xAA00AA, // violet
    BLUE = 0xAA5500, // blue
    LIGHT_GRAY = 0xAAAAAA, // light gray
    GARY = 0x555555, // grey
    PEACH = 0x5555FF, // peach
    LIME = 0x55FF55, // lime
    YELLOW = 0x55FFFF, // yellow 
    PURPLE = 0xFF5555, // purple
    PINK = 0xFF55FF, // pink
    SKY = 0xFFFF55, // sky
    WHITE = 0xFFFFFF  // white
} pixel_color_t;
