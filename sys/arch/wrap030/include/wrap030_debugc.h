
#ifndef DEBUG_BOOTSTRAP_C
#define DEBUG_BOOTSTRAP_C

/*
volatile char * aciaComC = (char *)0x80080000;
volatile char * aciaDatC = (char *)0x80080004;

static inline void debugPrintStr(const char * str)
{
    char c;
    c = *str++;
    while(c)
    {
        while(!(*aciaComC & 2));
        *aciaDatC = c;
        c = *str++;
    }
}
*/

static inline void debugPrintChar(char c)
{
	volatile char * comPtr = (char *)0x80080000;
	volatile char * datPtr = (char *)0x80080004;
	while(!(*comPtr & 2));
	*datPtr = c;
}

static inline void debugPrintStr(const char * str)
{
	char c = *str++;
	while(c)
	{
		debugPrintChar(c);
		c = *str++;
	}
}

static inline void debugPrintByte(unsigned char n)
{
	unsigned char c;
	c = n >> 4;
	if(c < 10) c += 0x30;
	else c += 0x37;
	debugPrintChar(c);
	c = n & 0x0f;
	if(c < 10) c += 0x30;
	else c += 0x37;
	debugPrintChar(c);
}

static inline void debugPrintShort(unsigned short n)
{
	debugPrintByte(n >> 8);
	debugPrintByte(n & 0x0ff);
}

static inline void debugPrintInt(unsigned int n)
{
	debugPrintShort(n >> 16);
	debugPrintShort(n & 0x0ffff);
}

#endif
