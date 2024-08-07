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
Initializing memory with random values...completed in 491707 us
Standard C test using int8
---------------
Performing 1048576 multiply-accumulates...(accum 0--needed to avoid being optimized out)...completed in 34887 us (30.056353 MMACS)
---------------

SIMD test using int8
---------------
Perfoming 1048576 multiply-accumulates...(accum 0)...completed in 8489 us (123.521734 MMACS)

Standard C test using int16
---------------
Performing 1048576 multiply-accumulates...(accum -68815--needed to avoid being optimized out)...completed in 47609 us (22.024743 MMACS)
---------------

SIMD test using int16
---------------
Performing 1048576 multiply-accumulates...(accum -68815--needed to avoid being optimized out)...completed in 16562 us (63.312160 MMACS)
---------------

Standard C test using int32
---------------
Performing 1048576 multiply-accumulates...(accum -1019413636--needed to avoid being optimized out)...completed in 42689 us (24.563143 MMACS)
---------------
```
