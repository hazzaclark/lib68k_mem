# lib68k_mem
Standalone Reference for validating Memory Functionality in lib68k

![image](https://github.com/user-attachments/assets/7a7c2648-eb9d-42f1-9b81-19bbdc1233fa)

```c
// MEMORY DEBUG INFORMATION WITH TRACE FLAGS: T0, T1
SET_TRACE_FLAGS(1, 0);
```

# Motive:

The following aims to act as a little, subsection of lib68k, to be able to validate and verify the efficiency and accuracy in terms of the Memory read and writes of my 68k Emulator.

This doesn't aim to be anything special outside of serving to provide aid in subsequent developments without breaking the main functionality of my stuff.

If my commits on lib68k are anything to go by, I have been scratching my head and wrestling with the Memory logic for as long as I can remember.
Therefore, this aims to put those quarms to rest


## Features:

Using the trace addressing space defined in lib68k, this isolated scheme allows for the validation of TRACE Level Addressing for handling pre-fetch

Returns proper TRACE evidence of the Memory R/W, how it communciates with other areas, fallthroughs for Buffer overflows, etc

The governance of which is handled by the ENABLE and DISBALE FLAG functions respectively, providing ease of use when the state changes

```c
#define SET_TRACE_FLAGS(T0, T1) \
        do {    \
            M68K_T0 = (T0); \
            M68K_T1 = (T1); \
            (T0) ? ENABLE_TRACE_FLAG(M68K_T0_SHIFT) : DISABLE_TRACE_FLAG(M68K_T0_SHIFT); \
            (T1) ? ENABLE_TRACE_FLAG(M68K_T1_SHIFT) : DISABLE_TRACE_FLAG(M68K_T1_SHIFT); \
    } while(0)
```

## Auto-disable functionality:

In a similar vein to [lib68k](https://github.com/hazzaclark/lib68k), for the sake of simplicity when it comes to the debugging utilities, I have implemented the functionality to turn HOOKS on and off

Simply use the oscillating macros of ``M68K_OPT_ON`` or ``M68K_OPT_OFF`` should you ever need more or less debugging information

```C

#define         CHECK_TRACE_CONDITION()         (IS_TRACE_ENABLED(M68K_T0_SHIFT) || IS_TRACE_ENABLED(M68K_T1_SHIFT))

#define         MEM_TRACE_HOOK                  M68K_OPT_ON

#if MEM_TRACE_HOOK == M68K_OPT_ON
    #define MEM_TRACE(OP, ADDR, SIZE, VAL) \
        do { \
            if (IS_TRACE_ENABLED(M68K_OPT_BASIC) && CHECK_TRACE_CONDITION()) \
                printf("[TRACE] %c ADDR:0x%08X SIZE:%d VALUE:0x%08X\n", \
                      (char)(OP), (ADDR), (SIZE), (VAL)); \
        } while(0)
#else
    #define MEM_TRACE(OP, ADDR, SIZE, VAL) ((void)0)
#endif
```

## Validate Memory Map usage:

The following aims to provide an example of one of the many features to be able to control the flow of memory throughout lib68k

This is by defining memory regions, and the determining factors they attribute such as R/W modes, their length and size, and whether they have been accessed throughout execution

Such is the case with the Bus, it is quite expandable in determining which memory locations can be used in accordance with whichever mode the CPU is running in (supervisor or user)

For the sake of convienience, the emulator DOES run using supervisor mode by default so a lot of the features are readily available 

![image](https://github.com/user-attachments/assets/f8013f11-28ea-49ee-881b-c56dbfb86dbb)

