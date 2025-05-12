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

#define     M68K_OPT_FLAGS            (M68K_OPT_BASIC | M68K_OPT_VERB)

#define     M68K_OPT_BASIC            (1 << 0)
#define     M68K_OPT_VERB             (1 << 1)
#define     M68K_OPT_DEVICE           (1 << 2)

#define     M68K_MAX_ADDR_START             0xFFFFFFF0
#define     M68K_MAX_ADDR_END               0xFFFFFFFF

#define     M68K_OPT_OFF		0
#define     M68K_OPT_ON			1

// THESE WILL OF COURSE BE SUBSTITUTED FOR THEIR RESPECTIVE METHOD OF
// ACCESS WITHIN THE EMULATOR ITSELF

static unsigned int M68K_T0 = 0;
static unsigned int M68K_T1 = 1;

#define         M68K_T0_SHIFT                   (1 << 3)
#define         M68K_T1_SHIFT                   (1 << 4)

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
    uint32_t READ_COUNT;
    uint32_t WRITE_COUNT;
    uint32_t LAST_READ;
    uint32_t LAST_WRITE;
    bool ACCESSED;

} M68K_MEM_USAGE;

typedef enum 
{
    MEM_REGION_RAM,
    MEM_REGION_ROM,
    MEM_REGION_IO,
    MEM_REGION_VECTORS,
    MEM_REGION_OTHER
} MEM_REGION_TYPE;

typedef struct
{
    uint32_t BASE;
    uint32_t SIZE;
    uint8_t* BUFFER;
    bool WRITE;
    M68K_MEM_USAGE USAGE;
    MEM_REGION_TYPE TYPE;

} M68K_MEM_BUFFER;

/////////////////////////////////////////////////////
//              GLOBAL DEFINITIONS
/////////////////////////////////////////////////////

static M68K_MEM_BUFFER MEM_BUFFERS[M68K_MAX_BUFFERS];
static unsigned MEM_NUM_BUFFERS = 0;
static bool TRACE_ENABLED = true;
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
    ENABLED_FLAGS &= ~FLAG;
}

bool IS_TRACE_ENABLED(uint8_t FLAG)
{
    return (ENABLED_FLAGS & FLAG) == FLAG;
}

void SHOW_MEMORY_MAPS(void)
{
    printf("\nACTIVE MEMORY MAPS:\n");
    printf("------------------------------------------------------------\n");
    printf("START        END         SIZE    STATE  READS   WRITES  ACCESS\n");
    printf("------------------------------------------------------------\n");

    for(unsigned INDEX = 0; INDEX < MEM_NUM_BUFFERS; INDEX++)
    {
        M68K_MEM_BUFFER* BUF = &MEM_BUFFERS[INDEX];
        printf(" 0x%08X 0x%08X %6dKB  %s  %6u  %6u      %s\n",
                BUF->BASE,
                BUF->BASE + BUF->SIZE - 1,
                BUF->SIZE / 1024,
                BUF->WRITE ? "RW" : "RO",
                BUF->USAGE.READ_COUNT,
                BUF->USAGE.WRITE_COUNT,
                BUF->USAGE.ACCESSED ? "YES" : "NO");
    }

    printf("------------------------------------------------------------\n");
}

/////////////////////////////////////////////////////
//              TRACE CONTROL MACROS
/////////////////////////////////////////////////////

#define         CHECK_TRACE_CONDITION()         (IS_TRACE_ENABLED(M68K_T0_SHIFT) || IS_TRACE_ENABLED(M68K_T1_SHIFT))

/////////////////////////////////////////////////////
//                 HOOK OPTIONS
/////////////////////////////////////////////////////

#define         MEM_TRACE_HOOK                  M68K_OPT_ON
#define         JUMP_HOOK                       M68K_OPT_ON
#define         VERBOSE_TRACK_HOOK              M68K_OPT_ON

// TRACE VALIDATION HOOKS TO BE ABLE TO CONCLUSIVELY VALIDATE MEMORY READ AND WRITES
// WHAT MAKES THESE TWO DIFFERENT IS THAT 
// ONE FOCUSSES ON THE INNATE MEMORY FUNCTIONALITY WHEREAS 
// THE OTHER JUST PRINTS OUT A WIDE VARIETY OF INFORMATION IRRESPECTIVE OF THE CONDITIONS MET

// HENCE WHY THE MEM TRACE NEEDS TO PROPERLY VALIDATE THE CONDITIONAL BEFOREHAND

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

#if VERBOSE_TRACK_HOOK == M68K_OPT_OFF
    #define VERBOSE_TRACE(MSG, ...) \
        do { \
            if (IS_TRACE_ENABLED(M68K_OPT_VERB)) \
                printf("[VERBOSE] " MSG "\n", ##__VA_ARGS__); \
        } while(0)
#else
    #define VERBOSE_TRACE(MSG, ...) ((void)0)
#endif

#if JUMP_HOOK == M68K_OPT_ON
    #define M68K_BASE_JUMP_HOOK(ADDR, FROM_ADDR) \
        do { \
            printf("[JUMP TRACE] TO: 0x%08X FROM: 0x%08X\n", (ADDR), (FROM_ADDR));\
        } while(0)
#else
    #define M68K_BASE_JUMP_HOOK(ADDR, FROM_ADDR) ((void)0)
#endif

#define SET_TRACE_FLAGS(T0, T1) \
        do {    \
            M68K_T0 = (T0); \
            M68K_T1 = (T1); \
            (T0) ? ENABLE_TRACE_FLAG(M68K_T0_SHIFT) : DISABLE_TRACE_FLAG(M68K_T0_SHIFT); \
            (T1) ? ENABLE_TRACE_FLAG(M68K_T1_SHIFT) : DISABLE_TRACE_FLAG(M68K_T1_SHIFT); \
    } while(0)

#define SHOW_TRACE_STATUS() \
    printf("\nTRACE CONFIG:\n"); \
    printf("  BASIC:   %s\n", IS_TRACE_ENABLED(M68K_OPT_BASIC) ? "ENABLED" : "DISABLED"); \
    printf("  VERBOSE: %s\n", IS_TRACE_ENABLED(M68K_OPT_VERB) ? "ENABLED" : "DISABLED"); \
    printf("  DEVICE TRACES:  %s\n", IS_TRACE_ENABLED(M68K_OPT_DEVICE) ? "ENABLED" : "DISABLED"); \
    printf("  T0 FLAG:    %s (SHIFT: 0x%02X)\n", M68K_T0 ? "ON" : "OFF", M68K_T0_SHIFT); \
    printf("  T1 FLAG:    %s  (SHIFT: 0x%02X)\n", M68K_T1 ? "ON" : "OFF", M68K_T1_SHIFT); \
    printf("  T0 ACTIVE:  %s\n", IS_TRACE_ENABLED(M68K_T0_SHIFT) ? "YES" : "NO"); \
    printf("  T1 ACTIVE:  %s\n", IS_TRACE_ENABLED(M68K_T1_SHIFT) ? "YES" : "NO"); \
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
        // FIRST WE READ AND DETERMINE THE READ STATISTICS OF THE CURRENT MEMORY MAP BEING ALLOCATED
        
        MEM_BASE->USAGE.READ_COUNT++;
        MEM_BASE->USAGE.LAST_READ = ADDRESS;
        MEM_BASE->USAGE.ACCESSED = true;

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
        // FIRST WE READ AND DETERMINE THE WRITE STATISTICS OF THE CURRENT MEMORY MAP BEING ALLOCATED

        MEM_BASE->USAGE.READ_COUNT++;
        MEM_BASE->USAGE.LAST_READ = ADDRESS;
        MEM_BASE->USAGE.ACCESSED = false;

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
    BUF->BUFFER = calloc(SIZE, 1);
    memset(BUF->BUFFER, 0, SIZE);

    // DETERMINE WHICH MEMORY MAPS ARE BEING USED AT ANY GIVEN TIME
    // FOR NOW, WE ARE ONLY CONCERNED WITH THE RAM AND IO TO COMMUNICATE
    // WITH THE 68K'S BUS

    memset(&BUF->USAGE, 0, sizeof(M68K_MEM_USAGE));
    BUF->USAGE.ACCESSED = false;

    #if M68K_MEM_DEBUG == M68K_OPT_OFF
    
    printf("MAPPED MEMORY: 0x%08x-0x%08X (%d BYTES)\n", 
           BASE, BASE + SIZE - 1, SIZE);

    #endif
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

    ENABLED_FLAGS = M68K_OPT_FLAGS;
    SET_TRACE_FLAGS(0, 1);
    SHOW_TRACE_STATUS();

    MEMORY_MAP(0x00001000, 0x1000, true);
    SHOW_MEMORY_MAPS();

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
