#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include "twister.h"

void generate_hex_file(int seed, int words, const char *file) {
	seedMT(seed);
	FILE *f=fopen(file, "w");
	if (f==NULL) {
		perror(file);
		exit(1);
	}
	for (int i=0; i<words; i++) {
		uint32_t c=randomMT();
		fprintf(f, "%08X\n", c);
	}
	fclose(f);
}

void loadhex(char *file, uint32_t *d, int l) {
	memset(d, 0, l*4);
	FILE *f=fopen(file, "r");
	if (f==NULL) {
		perror(file);
		exit(1);
	}
	char buff[1024];
	int p=0;
	while (!feof(f)) {
		fgets(buff, 1023, f);
		d[p++]=strtol(buff, NULL, 16);
	}
	printf("%s: %d words\n", file, p);
	fclose(f);
}

void read_brambuf(FILE *f, uint16_t *buf) {
	char lbuf[1024];
	for (int i=0; i<256; i++) {
		fgets(lbuf, 1023, f);
		char *p=lbuf;
		for (int j=0; j<8; j++) {
			while (*p==' ') p++; //skip spaces
			buf[i*8+j]=strtol(p, &p, 16); //read val
		}
	}
}

void write_brambuf(FILE *f, uint16_t *buf) {
	for (int i=0; i<256; i++) {
		for (int j=0; j<8; j++) {
			if (j!=0) fprintf(f, " ");
			fprintf(f, "%03x", (int)buf[i*8+j]);
		}
		fprintf(f, "\n");
	}
}

void create_brambuf(uint16_t *out, uint32_t *in, int bits, int bpos) {
	uint32_t mask=(0xffffffff^(0xffffffff<<bits))<<bpos;
	int pos=0;
	int j;
	for (j=0; j<2048; j++) {
		uint16_t e=0;
		for (int k=0; k<8/bits; k++) {
			e>>=bits;
			e|=((in[pos++]&mask)>>bpos)<<(8-bits);
		}
//		printf("%03X ", e);
//		if ((j&7)==7) printf("\n");
		out[j]=e;
	}
}

int try_replace_brambuf(uint16_t *brambuf, uint32_t *rnd, uint32_t *hex, int len) {
	uint16_t tstbuf[2048];
	for (int bits=1; bits<=8; bits<<=1) {
		if ((len*bits) < 2048*8) continue;
		for (int bpos=0; bpos<32; bpos+=bits) {
			create_brambuf(tstbuf, rnd, bits, bpos);
			if (memcmp(brambuf, tstbuf, sizeof(tstbuf))==0) {
				printf("Found bram_init data: bits %d bpos %d\n", bits, bpos);
				create_brambuf(brambuf, hex, bits, bpos);
				return 1;
			}
		}
	}
	return 0;
}

int bram_replace(int len, uint32_t seed, char *hexdata, char *infile, char *outfile) {
	uint32_t *hex=malloc(len*4);
	loadhex(hexdata, hex, len);

	uint32_t *randdata=malloc(len*4);
	seedMT(seed);
	for (int i=0; i<len; i++) randdata[i]=randomMT();

	FILE *in=fopen(infile, "r");
	if (in==NULL) {
		perror(infile);
		exit(1);
	}
	FILE *out=fopen(outfile, "w");
	if (out==NULL) {
		perror(outfile);
		exit(1);
	}
	char buff[1024]={0};
	uint16_t brambuf[2048];
	int replaced=0;
	while (!feof(in)) {
		fgets(buff, 1023, in);
		if (strncmp(buff, ".bram_init ", 11)==0) {
			fputs(buff, out);
			read_brambuf(in, brambuf);
			if (try_replace_brambuf(brambuf, randdata, hex, len)) {
				replaced++;
			}
			write_brambuf(out, brambuf);
		} else {
			fputs(buff, out);
		}
	}
	if (replaced*2048 < len*4) {
		printf("ERROR! Only replaced %d bram init statements, worth %dK of data. Data is %dK!\n", replaced, replaced*2, len*4/1024);
		unlink(outfile);
		return 0;
	} else {
		printf("Replaced %d bram init statements\n", replaced);
		return 1;
	}
}


int main(int argc, char **argv) {
	uint32_t seed=0x123456;
	int do_gen=0;
	int do_repl=0;
	int size=0;
	char *infile=NULL;
	char *outfile=NULL;
	char *hexfile=NULL;
	int error=0;
	for (int i=1; i<argc; i++) {
		if (strcmp(argv[i], "-g")==0 && i<argc-1) {
			outfile=argv[i+1];
			do_gen=1;
			i++;
		} else if (strcmp(argv[i], "-l")==0 && i<argc-1) {
			size=strtol(argv[i+1], NULL, 0);
			i++;
		} else if (strcmp(argv[i], "-s")==0 && i<argc-1) {
			seed=strtol(argv[i+1], NULL, 0);
			i++;
		} else if (strcmp(argv[i], "-r")==0 && i<argc-3) {
			do_repl=1;
			infile=argv[i+1];
			outfile=argv[i+2];
			hexfile=argv[i+3];
			i+=3;
		} else {
			error=1;
			break;
		}
	}
	if (!error && do_gen==do_repl) {
		printf("Error: Need one of -s / -r!\n");
		error=1;
	}
	if (!error && size==0) {
		printf("Error: need -l argument!\n");
		error=1;
	}

	if (error) {
		printf("Quick-and-dirty ECP5 bram replacer. Synthesize the design with\n");
		printf("a hexfile generated by the -g option, then replace with the\n");
		printf("real hex data using the -r option.\n");
		printf("\n");
		printf("Usage:\n");
		printf(" %s -s seed -l length-in-words -g generated-hexfile.hex\n", argv[0]);
		printf(" %s -s seed -l length-in-words -r infile.config outfile.config real-hexfile.hex\n", argv[0]);
	}

	if (do_gen) {
		generate_hex_file(seed, size/4, outfile);
	}
	if (do_repl) {
		if (!bram_replace(size, seed, hexfile, infile, outfile)) {
			printf("Replace failed.\n");
			exit(1);
		}
	}
	printf("Done.\n");
	return 0;
}