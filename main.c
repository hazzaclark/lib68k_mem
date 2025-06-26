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

#define     M68K_MAX_ADDR_START             0x00000000
#define     M68K_MAX_ADDR_END               0xFFFFFFFF

#define     M68K_OPT_OFF		0
#define     M68K_OPT_ON			1

// THESE WILL OF COURSE BE SUBSTITUTED FOR THEIR RESPECTIVE METHOD OF
// ACCESS WITHIN THE EMULATOR ITSELF

static unsigned int M68K_T0 = 0;
static unsigned int M68K_T1 = 1;
static unsigned int M68K_STOPPED;

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
    uint32_t VIOLATION;
    bool ACCESSED;

} M68K_MEM_USAGE;

typedef struct
{
    uint32_t BASE;
    uint32_t END;
    uint32_t SIZE;
    uint8_t* BUFFER;
    bool WRITE;
    M68K_MEM_USAGE USAGE;

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
    printf("\n%s MEMORY MAPS:\n", M68K_STOPPED ? "AFTER" : "BEFORE");
    printf("------------------------------------------------------------\n");
    printf("START        END         SIZE    STATE  READS   WRITES  ACCESS\n");
    printf("------------------------------------------------------------\n");

    for (unsigned INDEX = 0; INDEX < MEM_NUM_BUFFERS; INDEX++)
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

#define         MEM_MAP_TRACE_HOOK              M68K_OPT_ON
#define         MEM_TRACE_HOOK                  M68K_OPT_ON
#define         JUMP_HOOK                       M68K_OPT_ON
#define         VERBOSE_TRACE_HOOK              M68K_OPT_OFF

// TRACE VALIDATION HOOKS TO BE ABLE TO CONCLUSIVELY VALIDATE MEMORY READ AND WRITES
// WHAT MAKES THESE TWO DIFFERENT IS THAT 
// ONE FOCUSSES ON THE INNATE MEMORY FUNCTIONALITY WHEREAS 
// THE OTHER JUST PRINTS OUT A WIDE VARIETY OF INFORMATION IRRESPECTIVE OF THE CONDITIONS MET

// HENCE WHY THE MEM TRACE NEEDS TO PROPERLY VALIDATE THE CONDITIONAL BEFOREHAND

#if MEM_TRACE_HOOK == M68K_OPT_OFF
    #define MEM_TRACE(OP, ADDR, SIZE, VAL) \
        do { \
            if (IS_TRACE_ENABLED(M68K_OPT_BASIC) && CHECK_TRACE_CONDITION()) \
                printf("[TRACE] %c ADDR:0x%08X SIZE:%d VALUE:0x%08X\n", \
                      (char)(OP), (ADDR), (SIZE), (VAL)); \
        } while(0)
#else
    #define MEM_TRACE(OP, ADDR, SIZE, VAL) ((void)0)
#endif

#if MEM_MAP_TRACE_HOOK == M68K_OPT_ON
    #define MEM_MAP_TRACE(OP, BASE, END, SIZE, UNIT, VAL) \
        do { \
            if (IS_TRACE_ENABLED(M68K_OPT_BASIC) && CHECK_TRACE_CONDITION()) \
                printf("[TRACE] %c -> BASE:0x%08X END:0x%08X SIZE:%d%s\n", \
                      (char)(OP), (BASE), (END), (SIZE), (UNIT)); \
        } while(0)
#else
    #define MEM_MAP_TRACE(OP, BASE, END, SIZE, UNIT, VAL) ((void)0)
#endif

#define VERBOSE_TRACE(MSG, ...) \
    do { \
        if (VERBOSE_TRACE_HOOK == M68K_OPT_ON && IS_TRACE_ENABLED(M68K_OPT_VERB) && CHECK_TRACE_CONDITION()) \
            printf("[VERBOSE] " MSG "\n", ##__VA_ARGS__); \
    } while(0)

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
    printf("  BASIC:            %s\n", IS_TRACE_ENABLED(M68K_OPT_BASIC) ? "ENABLED" : "DISABLED"); \
    printf("  VERBOSE:          %s\n", (VERBOSE_TRACE_HOOK == M68K_OPT_ON && IS_TRACE_ENABLED(M68K_OPT_VERB)) ? "ENABLED" : "DISABLED"); \
    printf("  DEVICE TRACES:    %s\n", IS_TRACE_ENABLED(M68K_OPT_DEVICE) ? "ENABLED" : "DISABLED"); \
    printf("  T0 FLAG:          %s  (SHIFT: 0x%02X)\n", M68K_T0 ? "ON" : "OFF", M68K_T0_SHIFT); \
    printf("  T1 FLAG:          %s (SHIFT: 0x%02X)\n", M68K_T1 ? "ON" : "OFF", M68K_T1_SHIFT); \
    printf("  T0 ACTIVE:        %s\n", IS_TRACE_ENABLED(M68K_T0_SHIFT) ? "YES" : "NO"); \
    printf("  T1 ACTIVE:        %s\n", IS_TRACE_ENABLED(M68K_T1_SHIFT) ? "YES" : "NO"); \
    printf("\n")

/////////////////////////////////////////////////////
//             MEMORY READ AND WRITE
/////////////////////////////////////////////////////

static M68K_MEM_BUFFER* MEM_FIND(uint32_t ADDRESS)
{
    VERBOSE_TRACE("FOUND MEMORY: 0x%04X", ADDRESS);

    // ITERATE THROUGH ALL REGISTERED MEMORY BUFFERS
    for(unsigned INDEX = 0; INDEX < MEM_NUM_BUFFERS; INDEX++)
    {
        // GET A POINTER TO THE CURRENT MEMORY BUFFER
        M68K_MEM_BUFFER* MEM_BASE = MEM_BUFFERS + INDEX;

        if(MEM_BASE->BUFFER == NULL)
        {
            VERBOSE_TRACE("UNALLOCATED BUFFER AT INDEX %d\n", INDEX);
            continue;
        }

        // CHECK IF:
        // 1. THE REQUESTED ADDRESS IS >= THE BUFFER'S BASE ADDRESS
        // 2. THE OFFSET FROM BASE ADDRESS IS WITHIN THE BUFFER'S SIZE

        if((MEM_BASE->BUFFER != NULL) && 
                (ADDRESS >= MEM_BASE->BASE) && 
                ((ADDRESS - MEM_BASE->BASE) < MEM_BASE->SIZE))
        {
            VERBOSE_TRACE("ACCESSED: 0x%08X [%s] IN BUFFER %u: 0x%08X - 0x%08X\n", 
                ADDRESS, 
                MEM_BASE->WRITE ? "RW" : "RO", 
                INDEX, 
                MEM_BASE->BASE, 
                MEM_BASE->BASE + MEM_BASE->SIZE - 1);

            return MEM_BASE;
        }
    }

    VERBOSE_TRACE("NO BUFFER FOUND FOR ADDRESS: 0x%08X\n", ADDRESS);
    return NULL;
}

// DEFINE A HELPER FUNCTION FOR BEING ABLE TO PLUG IN ANY RESPECTIVE
// ADDRESS AND SIZE BASED ON THE PRE-REQUISITE SIZING OF THE ENUM

static uint32_t MEMORY_READ(uint32_t ADDRESS, uint32_t SIZE)
{
    VERBOSE_TRACE("READING ADDRESS FROM 0x%08X (SIZE = %d)\n", ADDRESS, SIZE);

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
            MEM_BASE->USAGE.VIOLATION++;
            VERBOSE_TRACE("READ OUT OF BOUNDS: OFFSET = %d, SIZE = %d, VIOLATION #%u\n", OFFSET, SIZE, MEM_BASE->USAGE.VIOLATION);
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
        // BEFORE ANYTHING, WE NEED TO VALIDATE IF THE MEMORY MAP
        // IS EITHER RW OR JUST RO

        if(!MEM_BASE->WRITE) 
        {
            MEM_BASE->USAGE.VIOLATION++;
            VERBOSE_TRACE("WRITE ATTEMPT TO READ-ONLY MEMORY AT 0x%08x, VIOLATION #%u\n", ADDRESS, MEM_BASE->USAGE.VIOLATION);
            goto MALFORMED;
        }

        // FIRST WE READ AND DETERMINE THE WRITE STATISTICS OF THE CURRENT MEMORY MAP BEING ALLOCATED

        MEM_BASE->USAGE.WRITE_COUNT++;
        MEM_BASE->USAGE.LAST_WRITE = ADDRESS;
        MEM_BASE->USAGE.ACCESSED = true;

        uint32_t OFFSET = (ADDRESS - MEM_BASE->BASE);
        uint32_t BYTES = SIZE / 8;

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
    fprintf(stderr, "BAD WRITE AT ADDRESS: 0x%08X (SIZE: %d, VALUE: 0x%08X)\n", 
            ADDRESS, SIZE, VALUE);
    MEM_TRACE(MEM_INVALID_WRITE, ADDRESS, SIZE, VALUE);
}

static void MEMORY_MAP(uint32_t BASE, uint32_t END, bool WRITABLE) 
{
    if(MEM_NUM_BUFFERS >= M68K_MAX_BUFFERS) 
    {
        fprintf(stderr, "CANNOT MAP - TOO MANY BUFFERS\n");
        return;
    }

    uint32_t SIZE = (END - BASE) + 1;

    M68K_MEM_BUFFER* BUF = &MEM_BUFFERS[MEM_NUM_BUFFERS++];
    BUF->BASE = BASE;
    BUF->END = END;
    BUF->SIZE = SIZE;
    BUF->WRITE = WRITABLE;
    BUF->BUFFER = malloc(SIZE);
    memset(BUF->BUFFER, 0, SIZE);

    // DETERMINE WHICH MEMORY MAPS ARE BEING USED AT ANY GIVEN TIME
    // FOR NOW, WE ARE ONLY CONCERNED WITH THE RAM AND IO TO COMMUNICATE
    // WITH THE 68K'S BUS

    memset(&BUF->USAGE, 0, sizeof(M68K_MEM_USAGE));
    BUF->USAGE.ACCESSED = false;

    MEM_MAP_TRACE(MEM_MAP, BUF->BASE, BUF->END, SIZE >= 1024*1024 ? SIZE/(1024*1024) 
    : SIZE >= 1024 ? SIZE/1024 : SIZE, SIZE >= 1024*1024 ? "MB" 
    : SIZE >= 1024 ? "KB" : "B", BUF->BUFFER);
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
    SET_TRACE_FLAGS(1,0);
    SHOW_TRACE_STATUS();

    MEMORY_MAP(0x000000, 0x0FFFFF, true);

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

    uint32_t TEST_32 = 0x134CA000;
    M68K_WRITE_MEMORY_32(0x1020, TEST_32);
    uint32_t READ_32 = M68K_READ_MEMORY_32(0x1020);
    printf("32-BIT: WROTE: 0x%08X, READ: 0x%08X\n", TEST_32, READ_32);

    uint16_t IMM_16 = 0xFFFF;
    M68K_WRITE_MEMORY_16(0x1000, IMM_16);
    uint16_t IMM_READ_16 = M68K_READ_IMM_16(0x1000);
    printf("16-BIT IMM: WROTE: 0x%04X, READ: 0x%04X\n", IMM_16, IMM_READ_16);

    uint32_t IMM_32 = 0xFFFFFFFF;
    M68K_WRITE_MEMORY_32(0x1030, IMM_32);
    printf("32-BIT IMM: WROTE: 0x%04X\n", IMM_32);

    printf("TESTING WRITE PROTECTION\n");
    M68K_WRITE_MEMORY_8(0x400000, 0x00);

    // THIS SHOULD FETCH THE ATTEMPT TO READ TRACE
    uint32_t RESERVED = M68K_READ_MEMORY_32(0xFFFFFFF0);

    M68K_STOPPED = 1;
    
    SHOW_MEMORY_MAPS();

    return 0;
}
