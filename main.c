// COPYRIGHT (C) HARRY CLARK 2025
// SMALL LIB68K MEMORY UTILITY/VALIDATOR

// THIS SMALL PROJECT AIMS TO PROVIDE ME WITH THE TESTING FACILITIES NEEDED
// IN ORDER TO ISOLATE ANY AND ALL READING AND WRITING LOGIC ADJACENT FROM THE EMULATOR ITSELF

// THE FOLLOWING AIMS TO SHOWCASE THE MEMORY READ AND WRITES ISOLATED TO PROPERLY VALIDATE THEIR
// RUNTIME, THEIR EXECUTION AND ANY SUBSEQUENT NUANCES THAT NEED IRONING OUT

// NESTED INCLUDES

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define     M68K_MAX_BUFFERS          10

#define     M68K_OPT_BASIC            (1 << 0)
#define     M68K_OPT_VERB             (1 << 1)

#define     M68K_OPT_FLAGS            (M68K_OPT_BASIC | M68K_OPT_VERB)

/////////////////////////////////////////////////////
//        BASE MEMORY VALIDATOR STRUCTURES
/////////////////////////////////////////////////////

typedef enum
{
    MEM_READ = 'R',
    MEM_WRITE = 'W',
    MEM_INVALID_READ = 'r',
    MEM_INVALID_WRITE = 'w',
    MEM_MAP = 'M',
    MEM_UNMAP = 'U',
    MEM_MOVE = 'O', 

} M68K_MEM_OP;

typedef enum
{
    MEM_SIZE_8 = 8,
    MEM_SIZE_16 = 16,
    MEM_SIZE_32 = 32

} M68K_MEM_SIZE;

typedef struct
{
    uint32_t BASE;
    uint32_t SIZE;
    uint8_t* BUFFER;

} M68K_MEM_BUFFER;

/////////////////////////////////////////////////////
//              GLOBAL DEFINITIONS
/////////////////////////////////////////////////////

static M68K_MEM_BUFFER MEM_BUFFERS[M68K_MAX_BUFFERS];
static unsigned MEM_NUM_BUFFERS = 0;
static uint8_t ENABLED_FLAGS = M68K_OPT_FLAGS;
static bool BUS_ERR = false;

/////////////////////////////////////////////////////
//            TRACE CONTROL FUNCTIONS
/////////////////////////////////////////////////////

void ENABLE_TRACE_FLAG(uint8_t FLAG)
{
    ENABLED_FLAGS |= FLAG;
}

void DISABLE_TRACE_FLAG(uint8_t FLAG)
{
    ENABLED_FLAGS &= FLAG;
}

bool IS_TRACE_ENABLED(uint8_t FLAG)
{
    return (ENABLED_FLAGS & FLAG) != 0;
}

/////////////////////////////////////////////////////
//              TRACE CONTROL MACROS
/////////////////////////////////////////////////////

#if DEFAULT_TRACE_FLAGS & TRACE_BASIC
    #define MEM_TRACE(OP, ADDR, SIZE, VAL) \
        do { \
            if (IS_TRACE_ENABLED(TRACE_BASIC)) \
                printf("[TRACE] %c ADDR:0x%08x SIZE:%d VALUE:0x%08x\n", \
                      (char)(OP), (ADDR), (SIZE), (VAL)); \
        } while(0)
#else
    #define MEM_TRACE(OP, ADDR, SIZE, VAL) ((void)0)
#endif

#if DEFAULT_TRACE_FLAGS & TRACE_VERBOSE
    #define VERBOSE_TRACE(MSG, ...) \
        do { \
            if (IS_TRACE_ENABLED(TRACE_VERBOSE)) \
                printf("[VERBOSE] %s:%d " MSG "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
        } while(0)
#else
    #define VERBOSE_TRACE(MSG, ...) ((void)0)
#endif

/////////////////////////////////////////////////////
/////////////////////////////////////////////////////

int main(void)
{
    printf("======================================\n");
    printf("HARRY CLARK - LIB68K MEMORY VALIDATOR\n");
    printf("======================================\n");

    return 0;
} 
