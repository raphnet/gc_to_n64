#include <stdio.h>
#include <math.h>

int main(void)
{
	double lambda = 0.8;
	double corr;
	int corr_int;
	int i;

	printf("#include <avr/pgmspace.h>\n\n");
	printf("unsigned char correction_lut[256] PROGMEM = {\n");

	for (i=0; i<256; i++)
	{
		if (i && (!(i%16)))
			printf("\n");
		
		corr = (pow(((double)i)/255.0, 1.0/lambda) * 255.0);

		corr += ((i)/2000.0 * 255);

		corr_int = (int)corr;
		
		if (corr_int < 0)
			corr_int = 0;
		if (corr_int > 255)
			corr_int = 255;

		printf("0x%02x ", corr_int);

		if (i!=255)
			printf(", ");
	}

	printf("\n};\n");

}
