# lib68k_mem
Standalone Reference for validating Memory Functionality in lib68k

# Motive:

The following aims to act as a little, subsection of lib68k, to be able to validate and verify the efficiency and accuracy in terms of the Memory read and writes of my 68k Emulator.

This doesn't aim to be anything special outside of serving to provide aid in subsequent developments without breaking the main functionality of my stuff.

If my commits on lib68k are anything to go by, I have been scratching my head and wrestling with the Memory logic for as long as I can remember.
Therefore, this aims to put those quarms to rest

![image](https://github.com/user-attachments/assets/aa269801-7986-40ec-a5dd-073f82391230)

```c
// MEMORY DEBUG INFORMATION WITH TRACE FLAGS: T0, T1
SET_TRACE_FLAGS(1, 0);
```

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

## Sophisticated Error Handlers:

One of the most intrinsic features that this memory utility offers is that, as a conjunctive effort with the variables defined as per the use-case of emulating a systems memory, it is able to dynamically alter to throw whichever error necessary. Errors that are handled and thrown are: Out of Bounds; Reserved Memory Range; Buffer Overflow, etc

Each of these come with their unique string literals to define the intrinsic circumstance per error message, opting for an O(1) solution to map and print each unqiue property.

The Error Handlers also come with a unique flag identifier that allows them to stand out more conducively in the TRACE output, providing further nuance and clarity for ease of use debugging

```c
// DEFINE AN ERROR HANDLER THROUGH OP, TYPE, SIZE AND DEBUG
#define MEM_ERROR(OP, ERROR_CODE, SIZE, MSG, ...) \
    do { \
        if (IS_TRACE_ENABLED(M68K_OPT_VERB) && CHECK_TRACE_CONDITION()) \
            printf("[ERROR] %c -> %-18s [SIZE: %d]: " MSG "\n", \
                (char)(OP), M68K_MEM_ERR[ERROR_CODE], \
                (int)(SIZE), ##__VA_ARGS__); \
    } while(0)
```

![image](https://github.com/user-attachments/assets/07b26181-8bec-4589-945c-fac71245efe6)

## Known Bugs:

As of right now, one of the known bugs is that when you are working with said ``MEM_ERROR`` macro, due to working with C99 standard, there is an inadvertant change that results in the varidatic args being a bit more strict and nuanced with it's declaration. Therefore, should debug statements ever need to be quite verbose, and in order to fill out the requirements of the macro, provide a dummy char literal that meets this requirement.

```c
- lib68k_mem

if(MEM_NUM_BUFFERS >= M68K_MAX_BUFFERS) 
{
   // WORKS FINE WITH BASE GCC, IT IS MUCH MORE LENIENT
   MEM_ERROR(MEM_ERR, MEM_ERR_BUFER, SIZE, "CANNOT MAP - TOO MANY BUFFERS");
   return;
}
```

```c
- lib68k

if(MEM_NUM_BUFFERS >= M68K_MAX_BUFFERS) 
{
   // MORE INDICATIVE OF THE STANDARD - MORE POSIX COMPLIANT
   MEM_ERROR(MEM_ERR, MEM_ERR_BUFER, SIZE, "CANNOT MAP - TOO MANY BUFFERS %s", " ");
   return;
}
```

With this being said, why would you need to change the standard between projects? This comes down to the nuance surrounding the portability sake of the C99 standard. Without giving much of a History lesson within a README, the C99 standard introduced a lot of defacto standard technicalities and features that were vacant in other C standards - it's the most commercial standard of C for a reason.

As such, even for a small utility like that, for withstanding a few adjustments to meet the standard of lib68k, opting for the C99 standard promotes scalibility and portable while promoting safety. 

## Validate Memory Map usage:

The following aims to provide an example of one of the many features to be able to control the flow of memory throughout lib68k

This is by defining memory regions, and the determining factors they attribute such as R/W modes, their length and size, and whether they have been accessed throughout execution

Such is the case with the Bus, it is quite expandable in determining which memory locations can be used in accordance with whichever mode the CPU is running in (supervisor or user).

This becomes especially apparent when being able to accommodate for systems emulation whereby you have various memory regions, each with their respective, intrinsic nature of accessing memory (such as RO - readonly, for the SEGA Mega Drive's VDP)

In addition to this, one of the many intrinsic features encompassing this Memory Utility is that there is a sophisticated means of preventing memory overflows.

The 68000, for all intents and purposes as a standalone unit, has 16MB of Addressable Space on the Bus - so ensuring that none of the memory maps exceeds that was paramount in preventing spill-overs into other memory maps and or causing a potential overflow.

![image](https://github.com/user-attachments/assets/2e57e7aa-8866-4794-98f1-9c7a4435a061)

## Usage:

Given the versatility of the intrinsic nature of how this memory utility is setup is that, you can adjust for any use case with any sort of systems emulations (through size, means of accessing memory, banks, etc)

Should you want to use this, it's a simple case of:

```
gcc main.c -o mem && ./mem
```

# Sources:

[68K PROGRAMMER MANUAL](https://www.nxp.com/docs/en/reference-manual/M68000PRM.pdf#page=43)

[SEGA MEGA DRIVE MEMORY MAP](https://segaretro.org/Sega_Mega_Drive/Memory_map)

[COND. MD MEMORY MAP](https://wiki.megadrive.org/index.php?title=Main_68k_memory_map)
