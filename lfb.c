#include "uart.h"
#include "mbox.h"
#include "lfb.h"
#include "terminal.h"

void draw_pixel(int x, int y, unsigned char attribute);

/* PC Screen Font as used by Linux Console */
typedef struct {
    unsigned int magic;
    unsigned int version;
    unsigned int headersize;
    unsigned int flags;
    unsigned int numglyph;
    unsigned int bytesperglyph;
    unsigned int height;
    unsigned int width;
    unsigned char glyphs;
} __attribute__((packed)) psf_t;
extern volatile unsigned char _binary_font_psf_start;

/* Scalable Screen Font (https://gitlab.com/bztsrc/scalable-font2) */
typedef struct {
    unsigned char  magic[4];
    unsigned int   size;
    unsigned char  type;
    unsigned char  features;
    unsigned char  width;
    unsigned char  height;
    unsigned char  baseline;
    unsigned char  underline;
    unsigned short fragments_offs;
    unsigned int   characters_offs;
    unsigned int   ligature_offs;
    unsigned int   kerning_offs;
    unsigned int   cmap_offs;
} __attribute__((packed)) sfn_t;
extern volatile unsigned char _binary_font_sfn_start;

unsigned int width, height, pitch;
unsigned char *lfb;

/**
 * Set screen resolution to 1024x768
 */
void lfb_init()
{
  
    mbox[0] = 35*4;
    mbox[1] = MBOX_REQUEST;

    mbox[2] = 0x48003;  //set phy wh
    mbox[3] = 8;
    mbox[4] = 8;
    mbox[5] = RES_WIDTH;         //FrameBufferInfo.width
    mbox[6] = RES_HEIGHT;          //FrameBufferInfo.height

    mbox[7] = 0x48004;  //set virt wh
    mbox[8] = 8;
    mbox[9] = 8;
    mbox[10] = 1024;        //FrameBufferInfo.virtual_width
    mbox[11] = 768;         //FrameBufferInfo.virtual_height

    mbox[12] = 0x48009; //set virt offset
    mbox[13] = 8;
    mbox[14] = 8;
    mbox[15] = 0;           //FrameBufferInfo.x_offset
    mbox[16] = 0;           //FrameBufferInfo.y.offset

    mbox[17] = 0x48005; //set depth
    mbox[18] = 4;
    mbox[19] = 4;
    mbox[20] = 32;          //FrameBufferInfo.depth

    mbox[21] = 0x48006; //set pixel order
    mbox[22] = 4;
    mbox[23] = 4;
    mbox[24] = 1;           //RGB, not BGR preferably

    mbox[25] = 0x40001; //get framebuffer, gets alignment on request
    mbox[26] = 8;
    mbox[27] = 8;
    mbox[28] = 4096;        //FrameBufferInfo.pointer
    mbox[29] = 0;           //FrameBufferInfo.size

    mbox[30] = 0x40008; //get pitch
    mbox[31] = 4;
    mbox[32] = 4;
    mbox[33] = 0;           //FrameBufferInfo.pitch

    mbox[34] = MBOX_TAG_LAST;

    if(mbox_call(MBOX_CH_PROP) && mbox[20]==32 && mbox[28]!=0) {
        mbox[28]&=0x3FFFFFFF;
        width=mbox[5];
        height=mbox[6];
        pitch=mbox[33];
        lfb=(void*)((unsigned long)mbox[28]);
    } else {
        uart_puts("Unable to set screen resolution to 1024x768x32\n");
    }

    print_resolution(RES_WIDTH, RES_HEIGHT);
    uart_puts("Pitch: ");
    uart_hex(mbox[33]);
    uart_puts("\n");
    uart_puts("depth: ");
    uart_hex(mbox[20]);
    uart_puts("\n");
}

/**
 * Display a string using fixed size PSF
 */
void lfb_print(int x, int y, char *s)
{
    // get our font
    psf_t *font = (psf_t*)&_binary_font_psf_start;
    // draw next character if it's not zero
    while(*s) {
        // get the offset of the glyph. Need to adjust this to support unicode table
        unsigned char *glyph = (unsigned char*)&_binary_font_psf_start +
         font->headersize + (*((unsigned char*)s)<font->numglyph?*s:0)*font->bytesperglyph;
        // calculate the offset on screen
        int offs = (y * pitch) + (x * 4);
        // variables
        int i,j, line,mask, bytesperline=(font->width+7)/8;
        // handle carrige return
        if(*s == '\r') {
            x = 0;
        } else
        // new line
        if(*s == '\n') {
            x = 20; y += font->height;
        } else {
            // display a character
            for(j=0;j<font->height;j++){
                // display one row
                line=offs;
                mask=1<<(font->width-1);
                for(i=0;i<font->width;i++){
                    // if bit set, we use white color, otherwise black
                    *((unsigned int*)(lfb + line))=((int)*glyph) & mask ? RED :0;
                    mask>>=1;
                    line+=4;
                }
                // adjust to next line
                glyph+=bytesperline;
                offs+=pitch;
            }
            x += (font->width+1);
        }
        // next character
        s++;
    }
}

/**
 * Display a string using proportional SSFN
 */
void lfb_proprint(int x, int y, char *s)
{
    // get our font
    sfn_t *font = (sfn_t*)&_binary_font_sfn_start;
    unsigned char *ptr, *chr, *frg;
    unsigned int c;
    unsigned long o, p;
    int i, j, k, l, m, n;

    while(*s) {
        // UTF-8 to UNICODE code point
        if((*s & 128) != 0) {
            if(!(*s & 32)) { c = ((*s & 0x1F)<<6)|(*(s+1) & 0x3F); s += 1; } else
            if(!(*s & 16)) { c = ((*s & 0xF)<<12)|((*(s+1) & 0x3F)<<6)|(*(s+2) & 0x3F); s += 2; } else
            if(!(*s & 8)) { c = ((*s & 0x7)<<18)|((*(s+1) & 0x3F)<<12)|((*(s+2) & 0x3F)<<6)|(*(s+3) & 0x3F); s += 3; }
            else c = 0;
        } else c = *s;
        s++;
        // handle carrige return
        if(c == '\r') {
            x = 0; continue;
        } else
        // new line
        if(c == '\n') {
            x = 0; y += font->height; continue;
        }
        // find glyph, look up "c" in Character Table
        for(ptr = (unsigned char*)font + font->characters_offs, chr = 0, i = 0; i < 0x110000; i++) {
            if(ptr[0] == 0xFF) { i += 65535; ptr++; }
            else if((ptr[0] & 0xC0) == 0xC0) { j = (((ptr[0] & 0x3F) << 8) | ptr[1]); i += j; ptr += 2; }
            else if((ptr[0] & 0xC0) == 0x80) { j = (ptr[0] & 0x3F); i += j; ptr++; }
            else { if((unsigned int)i == c) { chr = ptr; break; } ptr += 6 + ptr[1] * (ptr[0] & 0x40 ? 6 : 5); }
        }
        if(!chr) continue;
        // uncompress and display fragments
        ptr = chr + 6; o = (unsigned long)lfb + y * pitch + x * 4;
        for(i = n = 0; i < chr[1]; i++, ptr += chr[0] & 0x40 ? 6 : 5) {
            if(ptr[0] == 255 && ptr[1] == 255) continue;

            frg = (unsigned char*)font + (chr[0] & 0x40 ? ((ptr[5] << 24) | (ptr[4] << 16) | (ptr[3] << 8) | ptr[2]) :
                ((ptr[4] << 16) | (ptr[3] << 8) | ptr[2]));

            if((frg[0] & 0xE0) != 0x80) continue;
            o += (int)(ptr[1] - n) * pitch; 
            n = ptr[1];
            k = ((frg[0] & 0x1F) + 1) << 3; 
            j = frg[1] + 1; frg += 2;
            for(m = 1; j; j--, n++, o += pitch)
                for(p = o, l = 0; l < k; l++, p += 4, m <<= 1) {
                    if(m > 0x80) 
                        { frg++; m = 1; }

                    if(*frg & m) 
                        *((unsigned int*)p) = GREEN;
                }
        }
        // add advances
        x += chr[4]+1; y += chr[5];
    }
}

void drawPixel(int x, int y, unsigned char attribute)
{
    pitch = mbox[33];
    int offset = (y * pitch) + (x * 4);
    *((unsigned int*)(lfb + offset)) = vgapal[attribute];
}

void drawLine(int x1, int y1, int x2, int y2, unsigned char attr)  
{  
    int dx, dy, p, x, y;

    dx = x2-x1;
    dy = y2-y1;
    x = x1;
    y = y1;
    p = 2*dy-dx;

    while (x<x2) {
       if (p >= 0) {
          drawPixel(x,y,attr);
          y++;
          p = p+2*dy-2*dx;
       } else {
          drawPixel(x,y,attr);
          p = p+2*dy;
       }
       x++;
    }
}


void drawRect(int x1, int y1, int x2, int y2, unsigned char attr, int fill)
{
    int y=y1;

    while (y <= y2) {
       int x=x1;
       while (x <= x2) {
	  if ((x == x1 || x == x2) || (y == y1 || y == y2)) drawPixel(x, y, attr);
	  else if (fill) drawPixel(x, y, (attr & 0xf0) >> 4);
          x++;
       }
       y++;
    }
}


void drawCircle(int x0, int y0, int radius, unsigned char attr, int fill)
{
    int x = radius;
    int y = 0;
    int err = 0;
 
    while (x >= y) {
	if (fill) {
	   drawLine(x0 - y, y0 + x, x0 + y, y0 + x, (attr & 0xf0) >> 4);
	   drawLine(x0 - x, y0 + y, x0 + x, y0 + y, (attr & 0xf0) >> 4);
	   drawLine(x0 - x, y0 - y, x0 + x, y0 - y, (attr & 0xf0) >> 4);
	   drawLine(x0 - y, y0 - x, x0 + y, y0 - x, (attr & 0xf0) >> 4);
	}
	drawPixel(x0 - y, y0 + x, attr);
	drawPixel(x0 + y, y0 + x, attr);
	drawPixel(x0 - x, y0 + y, attr);
        drawPixel(x0 + x, y0 + y, attr);
	drawPixel(x0 - x, y0 - y, attr);
	drawPixel(x0 + x, y0 - y, attr);
	drawPixel(x0 - y, y0 - x, attr);
	drawPixel(x0 + y, y0 - x, attr);

	if (err <= 0) {
	    y += 1;
	    err += 2*y + 1;
	}
 
	if (err > 0) {
	    x -= 1;
	    err -= 2*x + 1;
	}
    }
}

void drawChar(unsigned char ch, int x, int y, unsigned char attr, int zoom)
{
    unsigned char *glyph = (unsigned char *)&font + (ch < FONT_NUMGLYPHS ? ch : 0) * FONT_BPG;

    for (int i=1;i<=(FONT_HEIGHT*zoom);i++) {
	for (int j=0;j<(FONT_WIDTH*zoom);j++) {
	    unsigned char mask = 1 << (j/zoom);
	    unsigned char col = (*glyph & mask) ? attr & 0x0f : (attr & 0xf0) >> 4;

	    drawPixel(x+j, y+i, col);
	}
	glyph += (i%zoom) ? 0 : FONT_BPL;
    }
}

void drawString(int x, int y, char *s, unsigned char attr, int zoom)
{
    while (*s) {
       if (*s == '\r') {
          x = 0;
       } else if(*s == '\n') {
          x = 0; y += (FONT_HEIGHT*zoom);
       } else {
	  drawChar(*s, x, y, attr, zoom);
          x += (FONT_WIDTH*zoom);
       }
       s++;
    }
}

void print_resolution(unsigned int width, unsigned int height)
{
    switch(width)
    {
        case 1280:
            //lfb_print(20, 20, "Screen Resolution: 1280 x 720\n");
            lfb_proprint(20, 20, "Screen Resolution: 1280 x 720\n");
            break;
        default:
            lfb_print(20, 20, "Resolution Auto-Detect Failed\n");
            break;
    }
}

