# ESP32-S3 Benchmark

This repository is to help benchmark various math operations on the ESP32-S3 microcontroller. Currently, this is set up to
multiply a million numbers in memory (thus exercising the PSRAM) using the following methods:

* int8 using the Xtensa/Espressif SIMD instructions
* int8 using standard C
* Floating point using standard C

Timing information is collected during execution and displayed in the console in terms of raw time (microseconds) and "million multiply-accumulates per second" (MMACS).

To build and execute:

```
. ~/esp/esp-idf/export.sh
idf.py build flash monitor
```

Current timing results:

```
Initializing memory with random values...completed in 491437 us
Perfoming 1048576 multiply-accumulates using SIMD...completed in 8618 us (121.672778 MMACS)
Performing 1048576 multiply-accumulates using standard C...(accum -1075281920--needed to avoid being optimized out)...completed in 20628 us (50.832655 MMACS)
Performing 1048576 FP multiply-accumulates using standard C...(accum 558403217753760595968.000000--needed to avoid being optimized out)...completed in 36000 us (29.127111 MMACS)
```
