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
void sendAT(char* string);
int getAT(char* buffer);
int waitREADY(void);
FILE *f;

void main(int argc, char **argv) {
	//We're running in app context. We have full control over the badge and can do with the hardware what we want. As
	//soon as main() returns, however, we will go back to the IPL.
	printf("Hello World app: main running\n");
	initFB();
	f=fopen("/dev/console", "w");
	setvbuf(f, NULL, _IONBF, 0); //make console line unbuffered
	fprintf(f, "\033C"); //clear the console. Note '\033' is the escape character.
	fprintf(f, "Hello Bluetooth!\n\n"); // Print a nice greeting.

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

	// Set baud t 115200
	UART_REG(UART_WIFI_DIV_REG) = 0x19e;

	// 7th bit is PMOD B3
	// set to output will turn module off
	// because defualt output value is zero
	MISC_REG(MISC_GPEXT_OE_REG) = (1<<(15+7));
	delay(1000);
	// turn module on
	MISC_REG(MISC_GPEXT_OUT_REG) = (1<<(15+7));
	delay(500);

	// wait for ready
	waitREADY();

	// buf to put rx'd stuff. clear it.
	char buffer[256];
	for(int i=0; i<256; i++) buffer[i]=0;
	int st=0;


	// Turn echo off
	sendAT("ATE0");
	st=getAT(buffer);
	fprintf(f,"rx--\n%s\n--[%d]\n",buffer,st);





	// talk to it.
	//sendAT("at+bleinit=2");
	//st=getAT(buffer);
	//fprintf(f,"rx--\n%s\n--[%d]\n",buffer,st);

	sendAT("at+bleaddr?");
	st=getAT(buffer);
	fprintf(f,"rx--\n%s\n--[%d]\n",buffer,st);
/*
	sendAT("wrong");
	st=getAT(buffer);
	fprintf(f,"rx--\n%s\n--[%d]\n",buffer,st);

*/
	
	printf("Hello World ready. Press a button to exit.\n");
	//Wait until all buttons are released
	wait_for_button_release();

	//Wait until button A is pressed
	wait_for_button_press(BUTTON_A);
	printf("Hello World done. Bye!\n");
}


// Get a response to an AT command. The responses end in ERROR or OK.
// This pulls bytes untill it sees 'ERROR' or 'OK' or times out.

int getAT(char* buffer){
	int timeout=2000;
	int chars=0;
	while(1){
		int c = UART_REG(UART_WIFI_DATA_REG);
		if (c&0x80000000){
			timeout--;
			delay(1);
			if (timeout<1) return -1;
		} else {
			//fprintf(f,"%x ",(char)c);
			buffer[chars]=c;
			if (chars>2){
				if ((char)buffer[chars]=='K' && (char)buffer[chars-1]=='O'){
					chars++;
					buffer[chars]=0;
					return chars;
				}
			}
			if (chars>5){
				if ((char)buffer[chars]=='R' &&
					 (char)buffer[chars-1]=='O'&&
					 (char)buffer[chars-2]=='R'&&
					 (char)buffer[chars-3]=='R'&&
					 (char)buffer[chars-4]=='E'){
					chars++;
					buffer[chars]=0;
					return chars;
				}

			}		
			chars++;	
		}
	}

}


// This waits for 'ready' and returns 0 if it sees it or -1 if it times out.
// The module will boot and spit out a bunch of boot status messages.
// this 'consumes' thoes.
int waitREADY(){
	char buffer[10];
	int timeout=1000;


	while(1){
			for (int i=0; i<10; i++)
		if (i!=0) buffer[i-1] = buffer[i];
		int c = UART_REG(UART_WIFI_DATA_REG);
		if (c&0x80000000){
			timeout--;
			delay(1);
			if (timeout<1) return -1;
		} else {
			//fprintf(f,"%c",(char)c);
			buffer[9]=c;
			if ((char)buffer[9]=='y' && 
				 (char)buffer[8]=='d'&& 
				 (char)buffer[7]=='a'&& 
				 (char)buffer[6]=='e'&& 
				 (char)buffer[5]=='r'){
				return 0;
			}
		}
	}
	
}

// Send an AT string. This adds a CRLF ot the end.
void sendAT(char* string){
	int i=0;
	while(string[i]!=0){
		UART_REG(UART_WIFI_DATA_REG)=string[i];
		fprintf(f,"%c",string[i]);
		i++;
	}
	fprintf(f,"\n");
	UART_REG(UART_WIFI_DATA_REG) = 0x0d;
	UART_REG(UART_WIFI_DATA_REG) = 0x0a;
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
