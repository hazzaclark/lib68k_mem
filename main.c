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
#define     M68K_OPT_DEVICE           (1 << 2)

#define     M68K_OPT_FLAGS            (M68K_OPT_BASIC | M68K_OPT_VERB)

#define     M68K_MAX_ADDR_START             0xFFFFFFF0
#define     M68K_MAX_ADDR_END               0xFFFFFFFF

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
    bool WRITE;

} M68K_MEM_BUFFER;

/////////////////////////////////////////////////////
//              GLOBAL DEFINITIONS
/////////////////////////////////////////////////////

static M68K_MEM_BUFFER MEM_BUFFERS[M68K_MAX_BUFFERS];
static unsigned MEM_NUM_BUFFERS = 0;
static bool TRACE_ENABLED;
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

#if M68K_JUMP_HOOK == M68K_OPT_ON
    #define M68K_BASE_JUMP_HOOK(ADDR, FROM_ADDR) \
        do { \
            printf("\n");                           \
            printf("[JUMP TRACE] TO: 0x%08x FROM: 0x%08x\n", (ADDR), (FROM_ADDR));\
        } while(0)
#else
    #define M68K_BASE_JUMP_HOOK(ADDR, FROM_ADDR) ((void)0)
#endif

#define SHOW_TRACE_STATUS() \
    printf("\nTRACE CONFIG:\n"); \
    printf("  BASIC:   %s\n", IS_TRACE_ENABLED(M68K_OPT_BASIC) ? "ENABLED" : "DISABLED"); \
    printf("  VERBOSE: %s\n", IS_TRACE_ENABLED(M68K_OPT_VERB) ? "ENABLED" : "DISABLED"); \
    printf("  DEVICE TRACES:  %s\n", IS_TRACE_ENABLED(M68K_OPT_DEVICE) ? "ENABLED" : "DISABLED"); \
    printf("\n")

/////////////////////////////////////////////////////
//             MEMORY READ AND WRITE
/////////////////////////////////////////////////////

static M68K_MEM_BUFFER* MEM_FIND(uint32_t ADDRESS)
{
    VERBOSE_TRACE("FINDING ADDRESS 0x%08x", ADDRESS);

    // ITERATE THROUGH ALL REGISTERED MEMORY BUFFERS
    for(unsigned INDEX = 0; INDEX < MEM_NUM_BUFFERS; INDEX++)
    {
        // GET A POINTER TO THE CURRENT MEMORY BUFFER
        M68K_MEM_BUFFER* MEM_BASE = MEM_BUFFERS + INDEX;

        // CHECK IF:
        // 1. THE BUFFER EXISTS (NOT NULL)
        // 2. THE REQUESTED ADDRESS IS >= THE BUFFER'S BASE ADDRESS
        // 3. THE OFFSET FROM BASE ADDRESS IS WITHIN THE BUFFER'S SIZE

        if((MEM_BASE->BUFFER != NULL) && (ADDRESS >= MEM_BASE->BASE) && ((ADDRESS - MEM_BASE->BASE) < MEM_BASE->SIZE))
        {
            // BUFFER FOUND - LOG DETAILS AND RETURN THE BUFFER POINTER
            VERBOSE_TRACE("FOUND BUFFER: %d: BASE = 0x%08x, SIZE = %d\n", INDEX, MEM_BASE->BASE, MEM_BASE->SIZE);
            return MEM_BASE;
        }
    }

    VERBOSE_TRACE("NO BUFFER FOUND FOR ADDRESS: 0x%08x\n", ADDRESS);
    return NULL;
}

// DEFINE A HELPER FUNCTION FOR BEING ABLE TO PLUG IN ANY RESPECTIVE
// ADDRESS AND SIZE BASED ON THE PRE-REQUISITE SIZING OF THE ENUM

static uint32_t MEMORY_READ(uint32_t ADDRESS, uint32_t SIZE)
{
    VERBOSE_TRACE("READING ADDRESS FROM 0x%08x (SIZE = %d)\n", ADDRESS, SIZE);

    // BOUND CHECKS FOR INVALID ADDRESSING

    if(ADDRESS == M68K_MAX_ADDR_START && ADDRESS <= M68K_MAX_ADDR_END)
    {
        VERBOSE_TRACE("ATTEMPT TO READ FROM RESERVED ADDRESS RANGE: 0x%08X\n", ADDRESS);
        goto MALFORMED_READ;
    }

    // FIND THE ADDRESS AND IT'S RELEVANT SIZE IN ACCORDANCE WITH WHICH VALUE IS BEING PROC.
    M68K_MEM_BUFFER* MEM_BASE = MEM_FIND(ADDRESS);

    if(MEM_BASE != NULL)
    {
        uint32_t OFFSET = (ADDRESS - MEM_BASE->BASE);

        if(OFFSET > (MEM_BASE->SIZE - (SIZE / 8)))
        {
            VERBOSE_TRACE("READ OUT OF BOUNDS: OFFSET = %d, SIZE = %d\n", OFFSET, SIZE);
            goto MALFORMED_READ;
        }
        
        // THIS MEMORY POINTER WILL ALLOCATE ITSELF RELATIVE TO THE BUFFER
        // AS WELL AS THE BIT SHIFT OFFSET THAT IS PRESENT WITH THE RESPECTIVE BIT VALUE

        uint8_t* MEM_PTR = MEM_BASE->BUFFER + OFFSET;
        uint32_t MEM_RETURN = 0;

        switch (SIZE)
        {
            case MEM_SIZE_32:
                MEM_RETURN = *MEM_PTR++;
                MEM_RETURN = (MEM_RETURN << 8) | *MEM_PTR++;
                MEM_RETURN = (MEM_RETURN << 8) | *MEM_PTR++;
                MEM_RETURN = (MEM_RETURN << 8) | *MEM_PTR;
                break;
                
            case MEM_SIZE_16:
                MEM_RETURN = *MEM_PTR++;
                MEM_RETURN = (MEM_RETURN << 8) | *MEM_PTR;
                break;
                
            case MEM_SIZE_8:
                MEM_RETURN = *MEM_PTR;
                break;
        }
        MEM_TRACE(MEM_READ, ADDRESS, SIZE, MEM_RETURN);
        return MEM_RETURN;
    }

MALFORMED_READ:
    fprintf(stderr, "BAD READ AT ADDRESS: 0x%08X\n", ADDRESS);
    MEM_TRACE(MEM_INVALID_READ, ADDRESS, SIZE, ~(uint32_t)0);
    return 0;
}

// NOW DO THE SAME FOR WRITES

static void MEMORY_WRITE(uint32_t ADDRESS, uint32_t SIZE, uint32_t VALUE)
{
    M68K_MEM_BUFFER* MEM_BASE = MEM_FIND(ADDRESS);

    if(MEM_BASE != NULL)
    {
        uint32_t OFFSET = (ADDRESS - MEM_BASE->BASE);
        uint32_t BYTES = SIZE / 8;

        if(!MEM_BASE->WRITE) 
        {
            VERBOSE_TRACE("WRITE ATTEMPT TO READ-ONLY MEMORY AT 0x%08x\n", ADDRESS);
            goto MALFORMED;
        }

        if((OFFSET + BYTES) > MEM_BASE->SIZE) 
        {
            VERBOSE_TRACE("WRITE OUT OF BOUNDS: OFFSET = %d, SIZE = %d\n", OFFSET, BYTES);
            goto MALFORMED;
        }

        uint8_t* MEM_PTR = MEM_BASE->BUFFER + OFFSET;
        MEM_TRACE(MEM_WRITE, ADDRESS, SIZE, VALUE);

        switch (SIZE)
        {
            case MEM_SIZE_32:
                *MEM_PTR++ = (VALUE >> 24) & 0xFF;
                *MEM_PTR++ = (VALUE >> 16) & 0xFF;
                *MEM_PTR++ = (VALUE >> 8) & 0xFF;
                *MEM_PTR = VALUE & 0xFF;
                break;

            case MEM_SIZE_16:
                *MEM_PTR++ = (VALUE >> 8) & 0xFF;
                *MEM_PTR = VALUE & 0xFF;
                break;
            
            case MEM_SIZE_8:
                *MEM_PTR = VALUE & 0xFF;
                break;
        }
        return;
    }

MALFORMED:
    fprintf(stderr, "BAD WRITE AT ADDRESS: 0x%08X (SIZE: 0x%02X, VALUE: 0x%08X)\n", 
            ADDRESS, SIZE, VALUE);
    MEM_TRACE(MEM_INVALID_WRITE, ADDRESS, SIZE, VALUE);
}

static void MEMORY_MAP(uint32_t BASE, uint32_t SIZE, bool WRITABLE) 
{
    if(MEM_NUM_BUFFERS >= M68K_MAX_BUFFERS) 
    {
        fprintf(stderr, "CANNOT MAP - TOO MANY BUFFERS\n");
        return;
    }

    M68K_MEM_BUFFER* BUF = &MEM_BUFFERS[MEM_NUM_BUFFERS++];
    BUF->BASE = BASE;
    BUF->SIZE = SIZE;
    BUF->WRITE = WRITABLE;
    BUF->BUFFER = malloc(SIZE);
    memset(BUF->BUFFER, 0, SIZE);
    
    printf("MAPPED MEMORY: 0x%08x-0x%08X (%d BYTES)\n", 
           BASE, BASE + SIZE - 1, SIZE);
    MEM_TRACE(MEM_MAP, BASE, SIZE, 0);
}

////////////////////////////////////////////////////////////////////////////////////////
//                      THE MAIN MEAT AND POTATOES
//          EACH OF THESE WILL REPRESENT AN UNSIGNE INT VALUE
//                  FROM THERE, BEING SIGNED A DEFINER
//          IN ACCORDANCE WITH THE ENUM (BASED ON THEIR BIT VALUE)
////////////////////////////////////////////////////////////////////////////////////////

unsigned int M68K_READ_MEMORY_8(unsigned int ADDRESS)
{
    return MEMORY_READ(ADDRESS, MEM_SIZE_8);
}

unsigned int M68K_READ_MEMORY_16(unsigned int ADDRESS)
{
    return MEMORY_READ(ADDRESS, MEM_SIZE_16);
}

unsigned int M68K_READ_MEMORY_32(unsigned int ADDRESS)
{
    return MEMORY_READ(ADDRESS, MEM_SIZE_32);
}

void M68K_WRITE_MEMORY_8(unsigned int ADDRESS, uint8_t VALUE) 
{
    MEMORY_WRITE(ADDRESS, MEM_SIZE_8, VALUE);
}

void M68K_WRITE_MEMORY_16(unsigned int ADDRESS, uint16_t VALUE) 
{
    MEMORY_WRITE(ADDRESS, MEM_SIZE_16, VALUE);
}

void M68K_WRITE_MEMORY_32(unsigned int ADDRESS, uint32_t VALUE) 
{
    MEMORY_WRITE(ADDRESS, MEM_SIZE_32, VALUE);
}

unsigned int M68K_READ_IMM_16(unsigned int ADDRESS)
{
    bool TRACE = TRACE_ENABLED;
    TRACE_ENABLED = false;

    unsigned int RESULT = M68K_READ_MEMORY_16(ADDRESS);
    TRACE_ENABLED = TRACE;

    return RESULT;
}

unsigned int M68K_READ_IMM_32(unsigned int ADDRESS)
{
    bool TRACE = TRACE_ENABLED;
    TRACE_ENABLED = false;

    unsigned int RESULT = M68K_READ_MEMORY_32(ADDRESS);
    TRACE_ENABLED = TRACE;

    return RESULT;
}

int main(void) 
{
    printf("======================================\n");
    printf("HARRY CLARK - LIB68K MEMORY VALIDATOR\n");
    printf("======================================\n");

    ENABLED_FLAGS = M68K_OPT_BASIC;
    SHOW_TRACE_STATUS();

    MEMORY_MAP(0x00001000, 0x1000, true);

    printf("TESTING BASIC READ AND WRITES\n");

    uint8_t TEST_8 = 0xAA;
    M68K_WRITE_MEMORY_8(0x1000, TEST_8);
    uint8_t READ_8 = M68K_READ_MEMORY_8(0x1000);
    printf("8-BIT: WROTE: 0x%02X, READ: 0x%02X\n", TEST_8, READ_8);

    uint16_t TEST_16 = 0xBBCC;
    M68K_WRITE_MEMORY_16(0x1010, TEST_16);
    uint16_t READ_16 = M68K_READ_MEMORY_16(0x1010);
    printf("16-BIT: WROTE: 0x%04X, READ: 0x%04X\n", TEST_16, READ_16);

    uint32_t TEST_32 = 0x13400000;
    M68K_WRITE_MEMORY_32(0x1020, TEST_32);
    uint32_t READ_32 = M68K_READ_MEMORY_32(0x1020);
    printf("32-BIT: WROTE: 0x%08X, READ: 0x%08X\n", TEST_32, READ_32);

    uint16_t IMM_16 = 0xFFFF;
    M68K_WRITE_MEMORY_16(0x1000, IMM_16);
    uint16_t IMM_READ_16 = M68K_READ_IMM_16(0x1000);
    printf("16-BIT IMM: WROTE: 0x%04X, READ: 0x%04X\n", IMM_16, IMM_READ_16);

    return 0;
}
