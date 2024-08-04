#include <stdio.h>

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Profile fun! microseconds to seconds
double GetTime() { return (double)esp_timer_get_time() / 1000000; }

static double tv[8];
const int N = 3200000;

#define TEST(type,name,ops) void IRAM_ATTR name (void) {\
    type f0 = tv[0],f1 = tv[1],f2 = tv[2],f3 = tv[3];\
    type f4 = tv[4],f5 = tv[5],f6 = tv[6],f7 = tv[7];\
    for (int j = 0; j < N/16; j++) {\
        ops \
    }\
    tv[0] = f0;tv[1] = f1;tv[2] = f2;tv[3] = f3;\
    tv[4] = f4;tv[5] = f5;tv[6] = f6;tv[7] = f7;\
    }
    
#define fops(op1,op2) f0 op1##=f1 op2 f2;f1 op1##=f2 op2 f3;\
    f2 op1##=f3 op2 f4;f3 op1##=f4 op2 f5;\
    f4 op1##=f5 op2 f6;f5 op1##=f6 op2 f7;\
    f6 op1##=f7 op2 f0;f7 op1##=f0 op2 f1;

#define addops fops(,+) fops(,+)
#define divops fops(,/) fops(,/)
#define mulops fops(,*) fops(,*)
#define muladdops fops(+,*)

TEST(int,mulint,mulops)
TEST(float,mulfloat,mulops)
TEST(double,muldouble,mulops)
TEST(int,addint,addops)
TEST(float,addfloat,addops)
TEST(double,adddouble,addops)
TEST(int,divint,divops)
TEST(float,divfloat,divops)
TEST(double,divdouble,divops)
TEST(int,muladdint,muladdops)
TEST(float,muladdfloat,muladdops)
TEST(double,muladddouble,muladdops)

void timeit(char *name,void fn(void)) {
    vTaskDelay(1);
    tv[0]=tv[1]=tv[2]=tv[3]=tv[4]=tv[5]=tv[6]=tv[7]=1;
    // get time since boot in microseconds
    uint64_t time=esp_timer_get_time();
    unsigned ccount,icount,ccount_new;
    RSR(CCOUNT,ccount);
    WSR(ICOUNT, 0);
    WSR(ICOUNTLEVEL,2);
    fn();
    RSR(CCOUNT,ccount_new);
    RSR(ICOUNT,icount);
    time=esp_timer_get_time()-time;
    float cpi=(float)(ccount_new-ccount)/icount;
    printf ("%s \t %f MOP/S   \tCPI=%f\n",name, (float)N/time,cpi);
}

void simdTest()
{
    int NUM_CONSTS = 1048576;

    printf("Initializing memory with random values...");

    int64_t startTime = esp_timer_get_time();
    char* eightBitValues = malloc(NUM_CONSTS);
    char data[16];

    assert(eightBitValues != NULL);

    for (int i = 0; i < NUM_CONSTS; i++)
    {
        eightBitValues[i] = rand() * 128;
    }

    for (int i = 0; i < 16; i++)
    {
        data[i] = rand() * 128;
    }

    int64_t endTime = esp_timer_get_time();
    printf("completed in %" PRIu64 " us\n", endTime - startTime);

    printf("Perfoming %d multiply-accumulates using SIMD...", NUM_CONSTS);
    startTime = esp_timer_get_time();

    char* ptr = eightBitValues;
    int n = NUM_CONSTS >> 4;

 asm volatile(
      "ld.qr q0, %[ptr], 0\n"                                              // Load ptr into q0
      "ld.qr q1, %[data], 0\n"                                             // Load data into q1
      "loopgtz %[n], simd_loop_end\n"                                      // while (n > 0) {
      "    ee.vmulas.s8.accx.ld.ip q2, %[ptr], 16, q0, q1\n"               //    accx += q0[0] * q1[0] + q0[1] * q1[1] + ...
                                                                           //    ptr += 16
                                                                           //    load ptr into q0
      "simd_loop_end:\n"                                                   //    n--
                                                                           // }
      
      : /* outputs */ 
        [ptr] "=r"(ptr), 
        [n] "=r"(n)
      : /* inputs */
        "0"(ptr),
        [data] "r"(data), 
        "1"(n) 
    );

    endTime = esp_timer_get_time();
    int64_t total = endTime - startTime;
    double usPerMAC = (double)total / (double)NUM_CONSTS;
    double macs = 1000000.0 / usPerMAC;
    printf("completed in %" PRId64 " us (%f MMACS)\n", total, macs / 1000000.0);

    printf("Performing %d multiply-accumulates using standard C...", NUM_CONSTS);
    startTime = esp_timer_get_time();
    int accum = 0;
    ptr = eightBitValues;
    for (int i = 0; i < NUM_CONSTS; i += 16)
    {
        accum += *ptr++ * data[0];
        accum += *ptr++ * data[1];
        accum += *ptr++ * data[2];
        accum += *ptr++ * data[3];
        accum += *ptr++ * data[4];
        accum += *ptr++ * data[5];
        accum += *ptr++ * data[6];
        accum += *ptr++ * data[7];
        accum += *ptr++ * data[8];
        accum += *ptr++ * data[9];
        accum += *ptr++ * data[10];
        accum += *ptr++ * data[11];
        accum += *ptr++ * data[12];
        accum += *ptr++ * data[13];
        accum += *ptr++ * data[14];
        accum += *ptr++ * data[15];
    }
    printf("(accum %d--needed to avoid being optimized out)...", accum);
    endTime = esp_timer_get_time();
    total = endTime - startTime;
    usPerMAC = (double)total / (double)NUM_CONSTS;
    macs = 1000000.0 / usPerMAC;
    printf("completed in %" PRId64 " us (%f MMACS)\n", total, macs / 1000000.0);

    free(eightBitValues);

    float* floatValues = malloc(NUM_CONSTS * sizeof(float));
    assert(floatValues);
    float floatData[16];

    for (int i = 0; i < 16; i++)
    {
        floatData[i] = rand() * 1000;
    }
    for (int i = 0; i < NUM_CONSTS; i++)
    {
         floatValues[i] = rand() * 1000;
    }

    printf("Performing %d FP multiply-accumulates using standard C...", NUM_CONSTS);
    startTime = esp_timer_get_time();
    float floatAccum = 0;
    float* floatPtr = floatValues;
    for (int i = 0; i < NUM_CONSTS; i += 16)
    {
        floatAccum += *floatPtr++ * floatData[0];
        floatAccum += *floatPtr++ * floatData[1];
        floatAccum += *floatPtr++ * floatData[2];
        floatAccum += *floatPtr++ * floatData[3];
        floatAccum += *floatPtr++ * floatData[4];
        floatAccum += *floatPtr++ * floatData[5];
        floatAccum += *floatPtr++ * floatData[6];
        floatAccum += *floatPtr++ * floatData[7];
        floatAccum += *floatPtr++ * floatData[8];
        floatAccum += *floatPtr++ * floatData[9];
        floatAccum += *floatPtr++ * floatData[10];
        floatAccum += *floatPtr++ * floatData[11];
        floatAccum += *floatPtr++ * floatData[12];
        floatAccum += *floatPtr++ * floatData[13];
        floatAccum += *floatPtr++ * floatData[14];
        floatAccum += *floatPtr++ * floatData[15];
    }
    printf("(accum %f--needed to avoid being optimized out)...", floatAccum);
    endTime = esp_timer_get_time();
    total = endTime - startTime;
    usPerMAC = (double)total / (double)NUM_CONSTS;
    macs = 1000000.0 / usPerMAC;
    printf("completed in %" PRId64 " us (%f MMACS)\n", total, macs / 1000000.0);

    free(floatValues);
}

int RamTest()
	{
	int rs[] = { 1,2,4,8,16,32,64,128,256,512,1024,2048,4000 };
	printf("Ram Speed Test!\n\n");
	char xx = 0;
	for (int a = 0; a < 13; a++)
		{
		printf("Read Speed 8bit ArraySize %4dkb ", rs[a]);
		int ramsize = rs[a] * 1024;
		char * rm = (char*)malloc(ramsize);

		int iters = 10; // Just enuff to boot the dog
		if (rs[a] < 512) iters = 50;
		double st = GetTime();
		for (int b = 0; b < iters; b++)
			for (int c = 0; c < ramsize; c++)
				xx |= rm[c];
		st = GetTime() - st;
		vTaskDelay(1); // Dog it!
		double speed = ((double)(iters*ramsize ) / (1024 * 1024)) / (st);
		printf(" time: %2.1f %2.1f mb/sec  \n", st, speed);
		free(rm);
		}
	printf("\n");
	for (int a = 0; a < 13; a++)
		{
		printf("Read Speed 16bit ArraySize %4dkb ", rs[a]);
		int ramsize = rs[a] * 1024;
		short * rm = (short*)malloc(ramsize);

		int iters = 10; // Just enuff to boot the dog
		if (rs[a] < 512) iters = 50;
		double st = GetTime();
		for (int b = 0; b < iters; b++)
			for (int c = 0; c < ramsize/2; c++)
				xx |= rm[c];
		st = GetTime() - st;
		vTaskDelay(1); // Dog it!
		double speed = ((double)(iters*ramsize) / (1024 * 1024)) / (st);
		printf(" time: %2.1f %2.1f mb/sec  \n", st, speed);
		free(rm);
		}
	printf("\n");
	for (int a = 0; a < 13; a++)
		{
		printf("Read Speed 32bit ArraySize %4dkb ", rs[a]);
		int ramsize = rs[a] * 1024;
		int * rm = (int*)malloc(ramsize);

		int iters = 10; // Just enuff to boot the dog
		if (rs[a] < 512) iters = 50;
		double st = GetTime();
		for (int b = 0; b < iters; b++)
			for (int c = 0; c < ramsize/4; c++)
				xx |= rm[c];
		st = GetTime() - st;
		vTaskDelay(1); // Dog it!
		double speed = ((double)(iters*ramsize) / (1024 * 1024)) / (st);
		printf(" time: %2.1f %2.1f mb/sec  \n", st, speed);
		free(rm);
		}
	printf("Test done!\n");
	return xx;
	}

void app_main(void)
{
#if 0
    RamTest();

    timeit("Integer Addition",addint);
    timeit("Integer Multiply",mulint);
    timeit("Integer Division",divint);
    timeit("Integer Multiply-Add",muladdint);

    timeit("Float Addition ", addfloat);
    timeit("Float Multiply ", mulfloat);
    timeit("Float Division ", divfloat);
    timeit("Float Multiply-Add", muladdfloat);

    timeit("Double Addition", adddouble);
    timeit("Double Multiply", muldouble);
    timeit("Double Division", divdouble);
    timeit("Double Multiply-Add", muladddouble);
#endif

    simdTest();
    simdTest();
    simdTest();
    simdTest();
    simdTest();
}
