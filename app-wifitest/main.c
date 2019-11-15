#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "mach_defines.h"
#include "sdk.h"
#include "gfx_load.h"
#include "cache.h"
#include "badgetime.h"

//The bgnd.png image got linked into the binary of this app, and these two chars are the first
//and one past the last byte of it.
extern char _binary_bgnd_png_start;
extern char _binary_bgnd_png_end;

extern char _binary_tilemap_tmx_start;
extern char _binary_tilemap_tmx_end;

//Pointer to the framebuffer memory.
uint8_t *fbmem;

#define FB_WIDTH 512
#define FB_HEIGHT 320

void initFB(void);

void main(int argc, char **argv) {
	//We're running in app context. We have full control over the badge and can do with the hardware what we want. As
	//soon as main() returns, however, we will go back to the IPL.
	printf("Hello World app: main running\n");
	initFB();
	FILE *f;
	f=fopen("/dev/console", "w");
	setvbuf(f, NULL, _IONBF, 0); //make console line unbuffered
	fprintf(f, "\033C"); //clear the console. Note '\033' is the escape character.
	fprintf(f, "Hello WiFi!\n\n"); // Print a nice greeting.

	// Set PMOD pins and boot module.
	// ok how tf does pmod gpio work?
	// in out OE for all extension connectors.
	// MISC_REG_GPEXT_IN/OUT/OE

	// What pins are the signals we need.
	// none? as long as they're highz we should be good.

	// Read from UART..
	// UART_REG()

	// UART has 1:0 addr pins, 4 regs.
	// reg0 is our reg.

	// set our baud.

#define UART_WIFI_DATA_REG	0x20
#define UART_WIFI_DIV_REG	0x24

	UART_REG(UART_WIFI_DIV_REG) = 0x19e;

	//UART_REG(UART_WIFI_DATA_REG) = 'a';
	//UART_REG(UART_WIFI_DATA_REG) = 't';
	//UART_REG(UART_WIFI_DATA_REG) = 0x0d;
	//UART_REG(UART_WIFI_DATA_REG) = 0x0a;
int i=0;
	while(1){
UART_REG(UART_WIFI_DATA_REG) = 'u';
		int c = UART_REG(UART_WIFI_DATA_REG);
	fprintf(f, "\0335X"); //set Xpos to 5
	fprintf(f, "\0338Y"); //set Ypos to 8
			fprintf(f,"val: %x\n%x",c,i);
i++;
	}

	
	printf("Hello World ready. Press a button to exit.\n");
	//Wait until all buttons are released
	wait_for_button_release();

	//Wait until button A is pressed
	wait_for_button_press(BUTTON_A);
	printf("Hello World done. Bye!\n");
}

void initFB(){
//Blank out fb while we're loading stuff by disabling all layers. This just shows the background color.
	GFX_REG(GFX_BGNDCOL_REG)=0x202020; //a soft gray
	GFX_REG(GFX_LAYEREN_REG)=0; //disable all gfx layers
	
	//First, allocate some memory for the background framebuffer. We're gonna dump a fancy image into it. The image is
	//going to be 8-bit, so we allocate 1 byte per pixel.

	fbmem=calloc(FB_WIDTH,FB_HEIGHT);
	for (int i=0; i<(FB_WIDTH*FB_HEIGHT); i++) fbmem[i]=0;
	printf("Hello World: framebuffer at %p\n", fbmem);
	
	//Tell the GFX hardware to use this, and its pitch. We also tell the GFX hardware to use palette entries starting
	//from 128 for the frame buffer; the tiles left by the IPL will use palette entries 0-16 already.
	GFX_REG(GFX_FBPITCH_REG)=(128<<GFX_FBPITCH_PAL_OFF)|(FB_WIDTH<<GFX_FBPITCH_PITCH_OFF);
	//Set up the framebuffer address
	GFX_REG(GFX_FBADDR_REG)=((uint32_t)fbmem);

	GFX_REG(GFX_LAYEREN_REG)=GFX_LAYEREN_FB_8BIT|GFX_LAYEREN_FB|GFX_LAYEREN_TILEA;
}
