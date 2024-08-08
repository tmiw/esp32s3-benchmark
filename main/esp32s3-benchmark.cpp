#include <stdio.h>
#include <string.h>

#include "esp_dsp.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

const int N = 1024; // 128 for testing datasets that fit entirely in internal RAM
const int M = 1024; // 128

void calculateAndPrintMMACS(int numOps, int64_t startTime, int64_t endTime)
{
    int64_t total = endTime - startTime;
    double timeInSecs = total / 1000000.0;
    double macs = numOps / timeInSecs;
    double mmacs = macs / 1000000.0;
    printf("completed in %" PRId64 " us (%f MMACS)\n", total, mmacs);
}

template<typename T>
void stdCTest(char* name, T* x, T* y)
{
    printf("Standard C test using %s\n", name);
    printf("---------------\n");

    printf("Performing %d multiply-accumulates...", N * M);
    T z[M];
    T* ptr;

    int64_t startTime = esp_timer_get_time();
    for (int i = 0; i < M; i++)
    {
        T acc = 0;
        ptr = &x[i*M];
        for (int j = 0; j < N; j++)
        {
            acc += *ptr++ * y[j];
        }
        z[i] = acc; 
    }    
    int64_t endTime = esp_timer_get_time();

    // To avoid optimizing out the above loop
    int tmpAcc = 0;
    for (int i = 0; i < N; i++)
    {
        tmpAcc += z[i];
    }

    // Print results
    printf("(accum %d--needed to avoid being optimized out)...", tmpAcc);
    calculateAndPrintMMACS(N * M, startTime, endTime);
    printf("---------------\n\n");
}

void simdTestInt16(short* x, short* y)
{
    printf("SIMD test using int16\n");
    printf("---------------\n");

    printf("Performing %d multiply-accumulates...", N * M);
    short z[M];

    int64_t startTime = esp_timer_get_time();
    for (int i = 0; i < M; i++)
    {
        dsps_dotprod_s16(&x[i*M], y, &z[i], N, 15);
    }    
    int64_t endTime = esp_timer_get_time();

    // To avoid optimizing out the above loop
    int tmpAcc = 0;
    for (int i = 0; i < N; i++)
    {
        tmpAcc += z[i];
    }

    // Print results
    printf("(accum %d--needed to avoid being optimized out)...", tmpAcc);
    calculateAndPrintMMACS(N * M, startTime, endTime);
    printf("---------------\n\n");
}

void simdTestInt16Matrix(short* x, short* y)
{
    printf("SIMD test using int16 matrix fns\n");
    printf("---------------\n");

    printf("Performing %d multiply-accumulates...", N * M);
    short z[M];

    int64_t startTime = esp_timer_get_time();
    dspm_mult_s16(x, y, z, M, N, 1, 15);
    int64_t endTime = esp_timer_get_time();

    // To avoid optimizing out the above loop
    int tmpAcc = 0;
    for (int i = 0; i < N; i++)
    {
        tmpAcc += z[i];
    }

    // Print results
    printf("(accum %d--needed to avoid being optimized out)...", tmpAcc);
    calculateAndPrintMMACS(N * M, startTime, endTime);
    printf("---------------\n\n");
}

void simdTestInt8(char* x, char* y)
{
    printf("SIMD test using int8\n");
    printf("---------------\n");

    printf("Perfoming %d multiply-accumulates...", N * M);
    int64_t startTime = esp_timer_get_time();

    char* ptr = x;
    char* dataPtr = y;
    char z[M];
    char* zPtr = &z[0];

    int n = M;
    int p = N >> 4;

    asm volatile(
        "movi a9, 0\n"                                                       // a9 = 0
        "simd_loop_begin:\n"                                                 // while (n > 0) {
        "    movi a11, 0\n"                                                  //    a11 = 0
        "    loopgtz %[p], inner_loop_end\n"                                 //    while (p > 0) {
        "        ee.zero.accx\n"                                             //        Zero accx
        "        ld.qr q0, %[ptr], 0\n"                                      //        Load ptr into q0
        "        ld.qr q1, %[dataPtr], 0\n"                                  //        Load dataPtr into q1
        "        addi %[ptr], %[ptr], 16\n"                                  //        ptr += 16
        "        addi %[dataPtr], %[dataPtr], 16\n"                          //        dataPtr += 16
        "        ee.vmulas.s8.accx q0, q1\n"                                 //        accx += q0[0] * q1[0] + q0[1] * q1[1] + ...
        "        ee.srs.accx a10, a9, 0\n"                                   //        a10 = accx >> a9 
        "        add a11, a11, a10\n"                                        //        a11 += a10
        "    inner_loop_end:\n"                                              //    }
        "    s8i a11, %[zPtr], 0\n"                                          //    Store value of a10 to memory location zPtr
        "    addi %[zPtr], %[zPtr], 1\n"                                     //    zPtr++
        "    addi %[dataPtr], %[dataPtr], -1024\n"                           //    dataPtr -= 1024
        "    addi %[n], %[n], -1\n"                                          //    n--
        "    bnez %[n], simd_loop_begin\n"                                   // }

        : /* outputs */ 
          [ptr] "=r"(ptr), 
          [zPtr] "=r"(zPtr),
          [dataPtr] "=r"(dataPtr),
          [p] "=r"(p),
          [n] "=r"(n)

        : /* inputs */
          "0"(ptr),
          "1"(zPtr),
          "2"(dataPtr),
          "3"(p),
          "4"(n)
  
        : /* clobbers */
          "a9", "a10", "a11", "memory"
    );

    int tmpAcc = 0;
    for (int i = 0; i < N; i++)
    {
        tmpAcc += z[i];
    }

    printf("(accum %d)...", tmpAcc);
    int64_t endTime = esp_timer_get_time();
    calculateAndPrintMMACS(N * M, startTime, endTime);
    printf("\n");
}

extern "C"
{

void app_main(void)
{
    printf("Initializing memory with random values...");

    int64_t startTime = esp_timer_get_time();
    char* x = (char*)malloc(N * M * sizeof(int));
    int y[N];

    assert(x != NULL);

    char* ptr = x;
    for (int i = 0; i < N * M; i++)
    {
        *ptr++ = rand() * 128;
    }

    ptr = (char*)y;
    for (int i = 0; i < N; i++)
    {
        *ptr++ = rand() * 128;
    }

    int64_t endTime = esp_timer_get_time();
    printf("completed in %" PRIu64 " us\n", endTime - startTime);

    stdCTest<char>("int8", x, (char*)y);
    simdTestInt8(x, (char*)y);
    stdCTest<short>("int16", (short*)x, (short*)y);
    simdTestInt16((short*)x, (short*)y);
    simdTestInt16Matrix((short*)x, (short*)y);
    stdCTest<int>("int32", (int*)x, (int*)y);
    //stdCTest<float>("float");

    // Free memory
    free(x);
}

}
