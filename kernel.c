#include "uart.h"
#include "mbox.h"
#include "rand.h"
#include "delays.h"
#include "lfb.h"
#include "power.h"

void get_board_num();

void main()
{
    // set up serial console
    uart_init();
    lfb_init();

    // display an ASCII string on screen with PSF
    //lfb_print(20, 20, "Screen Resolution: ");
    drawCircle(640, 360, 50, RED, 1);
    drawCircle(640, 360, 40, BLACK, 1);
    drawCircle(640, 360, 30, RED, 1);
    drawCircle(640, 360, 20, BLACK, 1);
 
    drawString(50, 50, "This is the drawString function\n", 0x02, 3);
    // echo everything back
    while(1) {
        uart_send(uart_getc());
    }
}

