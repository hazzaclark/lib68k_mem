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

#define     M68K_MAX_BUFFERS          5

#define     M68K_OPT_FLAGS            (M68K_OPT_BASIC | M68K_OPT_VERB)

#define     M68K_OPT_BASIC            (1 << 0)
#define     M68K_OPT_VERB             (1 << 1)
#define     M68K_OPT_DEVICE           (1 << 2)

#define     M68K_MAX_ADDR_START             0x0000000
#define     M68K_MAX_MEMORY_SIZE            0x1000000
#define     M68K_MAX_ADDR_END               (M68K_MAX_ADDR_START + M68K_MAX_MEMORY_SIZE)

#define     M68K_OPT_OFF        0
#define     M68K_OPT_ON         1

// THESE WILL OF COURSE BE SUBSTITUTED FOR THEIR RESPECTIVE METHOD OF
// ACCESS WITHIN THE EMULATOR ITSELF

static unsigned int M68K_T0 = 0;
static unsigned int M68K_T1 = 1;
static unsigned int M68K_STOPPED;

#define         M68K_T0_SHIFT                   (1 << 3)
#define         M68K_T1_SHIFT                   (1 << 4)

#define         KB_TO_BYTES                     1024
#define         MB_TO_BYTES                     (1024 * 1024)

#define         M68K_LSB_MASK                   0xFF

#define         M68K_BERR                       2
#define         M68K_BERR_ADDR                  (M68K_BERR * 4)

#define         FORMAT_SIZE(SIZE) \
                (SIZE) >= MB_TO_BYTES ? (SIZE)/MB_TO_BYTES : \
                (SIZE) >= KB_TO_BYTES ? (SIZE)/KB_TO_BYTES : (SIZE)

#define         FORMAT_UNIT(SIZE) \
                (SIZE) >= MB_TO_BYTES ? "MB" : \
                (SIZE) >= KB_TO_BYTES ? "KB" : "B"

/////////////////////////////////////////////////////
//        BASE MEMORY VALIDATOR STRUCTURES
/////////////////////////////////////////////////////

typedef enum
{
    MEM_READ = 'R',
    MEM_WRITE = 'W',
    MEM_INVALID_READ = '!',
    MEM_INVALID_WRITE = '?',
    MEM_MAP = 'M',
    MEM_UNMAP = 'U',
    MEM_MOVE = 'O',
    MEM_ERR = 'E',
    MEM_BUS = 'B'

} M68K_MEM_OP;

typedef enum
{
    MEM_SIZE_8 = 8,
    MEM_SIZE_16 = 16,
    MEM_SIZE_32 = 32

} M68K_MEM_SIZE;

typedef enum
{   
    MEM_OK,
    MEM_ERR_BOUNDS,
    MEM_ERR_READONLY,
    MEM_ERR_UNMAPPED,
    MEM_ERR_BUS,
    MEM_ERR_BUFFER,
    MEM_ERR_SIZE,
    MEM_ERR_RESERVED,
    MEM_ERR_OVERFLOW,
    MEM_ERR_BAD_READ,
    MEM_ERR_BAD_WRITE

} M68K_MEM_ERROR;

typedef enum
{
    BERR_NONE,
    BERR_UNMAPPED_READ,
    BERR_UNMAPPED_WRITE,
    BERR_READONLY,
    BERR_ALIGN,
    BERR_BOUNDS,
    BERR_TIMEOUT
    
} M68K_BERR_TYPE;

typedef struct
{
    bool ACTIVE;                // 0x00
    bool HALT_LINE;             // 0x01
    uint32_t CURRENT_ADDRESS;   // 0x05
    uint32_t CURRENT_PC;        // 0x09
    uint32_t ACCESS_SIZE;       // 0x0D
    uint32_t FAULT_COUNT;       // 0x11

    M68K_BERR_TYPE TYPE;        // 0x15
    M68K_MEM_OP OP;             // 0x19

} M68K_BERR_STATE;

typedef struct
{
    uint32_t READ_COUNT;
    uint32_t WRITE_COUNT;
    uint32_t MOVE_COUNT;
    uint32_t LAST_READ;
    uint32_t LAST_WRITE;
    uint32_t LAST_MOVE_SRC;
    uint32_t LAST_MOVE_DEST;
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

static const char* M68K_MEM_ERR[] = 
{
    "OK",
    "MEMORY OUT OF BOUNDS",
    "MEMORY IS READ ONLY",
    "MEMORY REGION IS UNMAPPED",
    "MEMORY HAS EXCEEDED BUS LIMIT",
    "MEMORY HAS TOO MANY BUFFERS",
    "MEMORY HAS AN INVALID SIZE FOR REGION",
    "MEMORY VIOLATES A RESERVED RANGE",
    "MEMORY OVERFLOW",
    "MEMORY ENCOUNTERED A BAD READ",
    "MEMORY ENCOUNTERED A BAD WRITE"
};

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
    printf("----------------------------------------------------------------------------------------------\n");
    printf("START        END         SIZE    STATE      READS   WRITES   MOVES      ACCESS     VIOLATIONS  \n");
    printf("----------------------------------------------------------------------------------------------\n");

    for (unsigned INDEX = 0; INDEX < MEM_NUM_BUFFERS; INDEX++)
    {
        M68K_MEM_BUFFER* BUF = &MEM_BUFFERS[INDEX];
        printf("0x%08X 0x%08X    %3d%s    %2s     %6u  %6u  %6u          %s             %u\n",
                BUF->BASE,
                BUF->BASE + BUF->SIZE - 1,
                FORMAT_SIZE(BUF->SIZE), 
                FORMAT_UNIT(BUF->SIZE),
                BUF->WRITE ? "RW" : "RO",
                BUF->USAGE.READ_COUNT,
                BUF->USAGE.WRITE_COUNT,
                BUF->USAGE.MOVE_COUNT,
                BUF->USAGE.ACCESSED ? "YES" : "NO",
                BUF->USAGE.VIOLATION);
    }

    printf("----------------------------------------------------------------------------------------------\n");
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

#if MEM_TRACE_HOOK == M68K_OPT_ON
    #define MEM_TRACE(OP, ADDR, SIZE, VAL) \
        do { \
            if (IS_TRACE_ENABLED(M68K_OPT_BASIC) && CHECK_TRACE_CONDITION()) \
                printf("[TRACE] %s ADDR:0x%X SIZE:%d VALUE:0x%X\n", \
                      (OP), (ADDR), (SIZE), (VAL)); \
        } while(0)
#else
    #define MEM_TRACE(OP, ADDR, SIZE, VAL) ((void)0)
#endif

#if MEM_TRACE_HOOK == M68K_OPT_ON
    #define MEM_MOVE_TRACE(SRC, DST, SIZE, COUNT) \
        do { \
            if (IS_TRACE_ENABLED(M68K_OPT_BASIC) && CHECK_TRACE_CONDITION()) \
                printf("[TRACE] [MOVE] SRC:0x%08X -> DEST:0x%08X | SIZE:%d BYTES | COUNT:%u\n", \
                      (SRC), (DST), (SIZE)/8, (COUNT)); \
        } while(0)
#else
    #define MEM_MOVE_TRACE(SRC, DST, SIZE, COUNT) ((void)0)
#endif

#if MEM_MAP_TRACE_HOOK == M68K_OPT_ON
    #define MEM_MAP_TRACE(OP, BASE, END, SIZE, VAL) \
    do { \
        if (IS_TRACE_ENABLED(M68K_OPT_BASIC) && CHECK_TRACE_CONDITION()) \
            printf("[TRACE] %c -> START:0x%08X END:0x%08X SIZE:%d%s\n", \
                  (char)(OP), (BASE), (END), FORMAT_SIZE(SIZE), FORMAT_UNIT(SIZE)); \
    } while(0)
#else
    #define MEM_MAP_TRACE(OP, BASE, END, SIZE, UNIT, VAL) ((void)0)
#endif

#define MEM_ERROR(OP, ERROR_CODE, SIZE, MSG, ...) \
    do { \
        if (IS_TRACE_ENABLED(M68K_OPT_VERB) && CHECK_TRACE_CONDITION()) \
            printf("[ERROR] %c -> %-18s [SIZE: %d]: " MSG "\n", \
                (char)(OP), M68K_MEM_ERR[ERROR_CODE], \
                (int)(SIZE), ##__VA_ARGS__); \
    } while(0)

#define VERBOSE_TRACE(MSG, ...) \
    do { \
        if (VERBOSE_TRACE_HOOK == M68K_OPT_ON && IS_TRACE_ENABLED(M68K_OPT_VERB) && CHECK_TRACE_CONDITION()) \
            printf("[VERBOSE] " MSG "\n", ##__VA_ARGS__); \
    } while(0)

#if JUMP_HOOK == M68K_OPT_ON
    #define M68K_BASE_JUMP_HOOK(ADDR, FROM_ADDR) \
        do { \
            printf("[JUMP TRACE] TO: 0x%X FROM: 0x%X\n", (ADDR), (FROM_ADDR));\
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
            return NULL;
        }

        // CHECK IF:
        // 1. THE REQUESTED ADDRESS IS >= THE BUFFER'S BASE ADDRESS
        // 2. THE OFFSET FROM BASE ADDRESS IS WITHIN THE BUFFER'S SIZE

        if((MEM_BASE->BUFFER != NULL) && 
                (ADDRESS >= MEM_BASE->BASE) && 
                (ADDRESS < (MEM_BASE->BASE + MEM_BASE->SIZE)))
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
    VERBOSE_TRACE("ATTEMPTING TO READ ADDRESS: 0x%08X (SIZE = %d)\n", ADDRESS, SIZE);

    // BOUND CHECKS FOR INVALID ADDRESSING
    if(ADDRESS > M68K_MAX_ADDR_END || ADDRESS > M68K_MAX_MEMORY_SIZE)
    {
        MEM_ERROR(MEM_ERR, MEM_ERR_RESERVED, SIZE, "ATTEMPT TO READ FROM RESERVED ADDRESS RANGE: 0x%08X", ADDRESS);
        MEM_ERROR(MEM_ERR, MEM_ERR_BOUNDS, SIZE, "ATTEMPT TO READ FROM AN ADDRESS RANGE BEYOND THE ADDRESSABLE SPACE: 0x%08X", ADDRESS);
        goto MALFORMED_READ;
    }

    // FIND THE ADDRESS AND IT'S RELEVANT SIZE IN ACCORDANCE WITH WHICH VALUE IS BEING PROC.
    M68K_MEM_BUFFER* MEM_BASE = MEM_FIND(ADDRESS);

    if(MEM_BASE != NULL)
    {
        uint32_t OFFSET = (ADDRESS - MEM_BASE->BASE);
        uint32_t BYTES = SIZE / 8;

        if((OFFSET + BYTES + 1) > MEM_BASE->SIZE)
        {
            MEM_BASE->USAGE.VIOLATION++;
            MEM_ERROR(MEM_ERR, MEM_ERR_BOUNDS, SIZE, "READ OUT OF BOUNDS: OFFSET = %d, SIZE = %d, VIOLATION #%u", OFFSET, BYTES, MEM_BASE->USAGE.VIOLATION);
            goto MALFORMED_READ;
        }

        // FIRST WE READ AND DETERMINE THE READ STATISTICS OF THE CURRENT MEMORY MAP BEING ALLOCATED
        // THIS CHECK COMES AFTER WHICH WE DETERMINE THE SIZE OF THE MEMORY REGION AS THIS IS TO
        // AVOID POTENTIAL SPILL-OVERS WITH ADDITIONAL READS

        MEM_BASE->USAGE.READ_COUNT++;
        MEM_BASE->USAGE.LAST_READ = ADDRESS;
        MEM_BASE->USAGE.ACCESSED = true;

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
        MEM_TRACE("[READ]", ADDRESS, SIZE, MEM_RETURN);
        return MEM_RETURN;
    }

    MEM_ERROR(MEM_ERR, MEM_ERR_UNMAPPED, SIZE, "NO BUFFER FOUND FOR ADDRESS: 0x%08X", ADDRESS);

MALFORMED_READ:
    MEM_ERROR(MEM_ERR, MEM_ERR_BAD_READ, SIZE, "ADDRESS: 0x%08X", ADDRESS);
    MEM_TRACE("[INVALID READ]", ADDRESS, SIZE, ~(uint32_t)0);
    return 0;
}

// NOW DO THE SAME FOR WRITES

static void MEMORY_WRITE(uint32_t ADDRESS, uint32_t SIZE, uint32_t VALUE)
{
    M68K_MEM_BUFFER* MEM_BASE = MEM_FIND(ADDRESS);

    VERBOSE_TRACE("ATTEMPTING WRITE TO ADDRESS: 0x%X (SIZE = %d, VALUE = 0x%X)\n", ADDRESS, SIZE, VALUE);

    // BOUND CHECKS FOR INVALID ADDRESSING
    if(ADDRESS > M68K_MAX_ADDR_END || ADDRESS > M68K_MAX_MEMORY_SIZE)
    {
        MEM_ERROR(MEM_ERR, MEM_ERR_RESERVED, SIZE, "ATTEMPT TO WRITE TO RESERVED ADDRESS RANGE: 0x%X", ADDRESS);
        MEM_ERROR(MEM_ERR, MEM_ERR_BOUNDS, SIZE, "ATTEMPT TO WRITE TO AN ADDRESS RANGE BEYOND THE ADDRESSABLE SPACE: 0x%X", ADDRESS);
        goto MALFORMED_WRITE;
    }

    if(MEM_BASE != NULL)
    {
        // BEFORE ANYTHING, WE NEED TO VALIDATE IF THE MEMORY MAP
        // IS EITHER RW OR JUST RO

        if(!MEM_BASE->WRITE) 
        {
            MEM_BASE->USAGE.VIOLATION++;
            MEM_ERROR(MEM_ERR, MEM_ERR_READONLY, SIZE, "WRITE ATTEMPT TO READ-ONLY MEMORY AT 0x%0x, VIOLATION #%u", ADDRESS, MEM_BASE->USAGE.VIOLATION);
            goto MALFORMED_WRITE;
        }

        uint32_t OFFSET = (ADDRESS - MEM_BASE->BASE);
        uint32_t BYTES = SIZE / 8;

        if((OFFSET + BYTES - 1) > MEM_BASE->SIZE) 
        {
            MEM_BASE->USAGE.VIOLATION++;
            MEM_ERROR(MEM_ERR, MEM_ERR_BOUNDS, SIZE, "WRITE OUT OF BOUNDS: OFFSET = %d, SIZE = %d, VIOLATION #%u", OFFSET, BYTES, MEM_BASE->USAGE.VIOLATION);
            goto MALFORMED_WRITE;
        }

        // FIRST WE READ AND DETERMINE THE WRITE STATISTICS OF THE CURRENT MEMORY MAP BEING ALLOCATED
        // THIS CHECK COMES AFTER WHICH WE DETERMINE THE SIZE OF THE MEMORY REGION AS THIS IS TO
        // AVOID POTENTIAL SPILL-OVERS WITH ADDITIONAL WRITES

        MEM_BASE->USAGE.WRITE_COUNT++;
        MEM_BASE->USAGE.LAST_WRITE = ADDRESS;
        MEM_BASE->USAGE.ACCESSED = true;

        uint8_t* MEM_PTR = MEM_BASE->BUFFER + OFFSET;
        MEM_TRACE("[WRITE]", ADDRESS, SIZE, VALUE);

        switch (SIZE)
        {
            case MEM_SIZE_32:
                *MEM_PTR++ = (VALUE >> 24) & M68K_LSB_MASK;
                *MEM_PTR++ = (VALUE >> 16) & M68K_LSB_MASK;
                *MEM_PTR++ = (VALUE >> 8) & M68K_LSB_MASK;
                *MEM_PTR = VALUE & M68K_LSB_MASK;
                break;

            case MEM_SIZE_16:
                *MEM_PTR++ = (VALUE >> 8) & M68K_LSB_MASK;
                *MEM_PTR = VALUE & M68K_LSB_MASK;
                break;
            
            case MEM_SIZE_8:
                *MEM_PTR = VALUE & M68K_LSB_MASK;
                break;
        }
        return;
    }

    MEM_ERROR(MEM_ERR, MEM_ERR_UNMAPPED, SIZE, "NO BUFFER FOUND FOR ADDRESS: 0x%0X", ADDRESS);

MALFORMED_WRITE:
    MEM_ERROR(MEM_ERR, MEM_ERR_BAD_WRITE, SIZE, "VALUE: 0x%0X, ADDRESS: 0x%0X", VALUE, ADDRESS);
    MEM_TRACE("[INVALID WRITE]", ADDRESS, SIZE, VALUE);
}

// MEMORY MOVE OPERATIONS - HANDLES THE SPECIFICS BETWEEN SOURCE
// AND DESTINATION OPERATIONS
//
// LOOKS TO FIND THE SOURCE AND DESTINATION OPERANDS TO PROPERLY
// VALIDATE THEIR EXECUTION
static void MEMORY_MOVE(uint32_t SRC, uint32_t DEST, uint32_t SIZE, uint32_t COUNT)
{
    VERBOSE_TRACE("MOVING %u BYTES FROM 0x%08X TO 0x%08X\n", COUNT, SRC, DEST);

    // FIND BOTH OF THE CURRENT OPERANDS WITHIN THE OPERATION

    M68K_MEM_BUFFER* SRC_BUFFER = MEM_FIND(SRC);
    M68K_MEM_BUFFER* DEST_BUFFER = MEM_FIND(DEST);

    if(SRC_BUFFER == NULL)
    {
        MEM_ERROR(MEM_MOVE, MEM_ERR_UNMAPPED, SIZE, "NO SOURCE BUFFER FOUND FOR ADDRESS: 0x%08X", SRC);
        return;
    }

    if(DEST_BUFFER == NULL)
    {
        MEM_ERROR(MEM_MOVE, MEM_ERR_UNMAPPED, SIZE, "NO DESTINATION BUFFER FOUND FOR ADDRESS: 0x%08X", DEST);
        return;
    }

    // CAN WE WRITE TO SAID DESTINATION?!
    // ASSUME THAT WE HAVE THE PROPER CONDITIONS FOR THE HIGH BOUND OF THE EA
    // THAT OF WHICH ENCOMPASSESS THE WRITE CONDITION

    if(!DEST_BUFFER->WRITE)
    {
        DEST_BUFFER->USAGE.VIOLATION++;
        MEM_ERROR(MEM_MOVE, MEM_ERR_READONLY, SIZE, "MOVE ATTEMPT TO READ-ONLY MEMORY: 0x%08X, VIOLATION: #%u", DEST, DEST_BUFFER->USAGE.VIOLATION);
    }

    // GET THE ALL ENCOMPASSING SIZE OF THE OPERATION
    uint32_t TRANSFER_SIZE = SIZE / 8;

    // DETERMINE THE CONCURRENT BYTES IS REQUIRED PER EACH OPERATION
    // SIZE IS ALL VARIABLE BASED ON THE DISTANCE BETWEEN INDIRECT AND EA
    for(uint32_t INDEX = 0; INDEX < COUNT; INDEX += TRANSFER_SIZE)
    {
        uint32_t CURRENT_SRC = SRC + INDEX;
        uint32_t CURRENT_DEST = DEST + INDEX;

        // READ FROM THE CURRENT SORUCE AGAINST THE SIZE
        // OF THE OPERATION
        uint32_t SRC_READ = MEMORY_READ(CURRENT_SRC, SIZE);

        // WRITE TO DESTINATION
        MEMORY_WRITE(CURRENT_DEST, SIZE, SRC_READ);
    }

    SRC_BUFFER->USAGE.MOVE_COUNT++;
    SRC_BUFFER->USAGE.LAST_MOVE_SRC = SRC;
    DEST_BUFFER->USAGE.MOVE_COUNT++;
    DEST_BUFFER->USAGE.LAST_MOVE_DEST = DEST;

    MEM_MOVE_TRACE(SRC, DEST, SIZE, COUNT);
} 

static void MEMORY_MAP(uint32_t BASE, uint32_t END, bool WRITABLE) 
{
    uint32_t SIZE = (END - BASE) + 1;

    if(MEM_NUM_BUFFERS >= M68K_MAX_BUFFERS) 
    {
        MEM_ERROR(MEM_ERR, MEM_ERR_BUFFER, SIZE, "CANNOT MAP - TOO MANY BUFFERS %s", " ");
        return;
    }

    if(END >= M68K_MAX_MEMORY_SIZE)
    {
        MEM_ERROR(MEM_ERR, MEM_ERR_BUS, SIZE, "END ADDRESS 0x%X EXCEEDS THE BUS LIMIT: (%d%s)", M68K_MAX_ADDR_END, FORMAT_SIZE(M68K_MAX_ADDR_END), FORMAT_UNIT(M68K_MAX_ADDR_END));
        return;
    }

    // DEFINE AND INITIALISE BUFFER-RELATED PARAMS
    // ONLY ALLOCATE HOW MANY BUFFERS WE NEED BASED ON THE MAX
    M68K_MEM_BUFFER* BUF = &MEM_BUFFERS[MEM_NUM_BUFFERS++];
    BUF->BASE = BASE;
    BUF->END = END;
    BUF->SIZE = SIZE;
    BUF->WRITE = WRITABLE;
    BUF->USAGE.MOVE_COUNT = 0;
    BUF->BUFFER = malloc(SIZE);
    memset(BUF->BUFFER, 0, SIZE);

    // DETERMINE WHICH MEMORY MAPS ARE BEING USED AT ANY GIVEN TIME
    // FOR NOW, WE ARE ONLY CONCERNED WITH THE RAM AND IO TO COMMUNICATE
    // WITH THE 68K'S BUS

    memset(&BUF->USAGE, 0, sizeof(M68K_MEM_USAGE));
    BUF->USAGE.ACCESSED = false;

    MEM_MAP_TRACE(MEM_MAP, BUF->BASE, BUF->END, BUF->SIZE, BUF->BUFFER);
}

////////////////////////////////////////////////////////////////////////////////////////
//              EACH OF THESE WILL REPRESENT AN UNSIGNED INT VALUE   
//                FROM THERE, BEING SIGNED A SIZE DEFINER
//                  IN ACCORDANCE WITH AN ENUM VALUE
////////////////////////////////////////////////////////////////////////////////////////

unsigned int M68K_READ_MEMORY_8(unsigned int ADDRESS)  { return MEMORY_READ(ADDRESS, MEM_SIZE_8); }
unsigned int M68K_READ_MEMORY_16(unsigned int ADDRESS) { return MEMORY_READ(ADDRESS, MEM_SIZE_16); }
unsigned int M68K_READ_MEMORY_32(unsigned int ADDRESS) { return MEMORY_READ(ADDRESS, MEM_SIZE_32); }

void M68K_WRITE_MEMORY_8(unsigned int ADDRESS, uint8_t VALUE)   { MEMORY_WRITE(ADDRESS, MEM_SIZE_8, VALUE); }
void M68K_WRITE_MEMORY_16(unsigned int ADDRESS, uint16_t VALUE) { MEMORY_WRITE(ADDRESS, MEM_SIZE_16, VALUE); }
void M68K_WRITE_MEMORY_32(unsigned int ADDRESS, uint32_t VALUE) { MEMORY_WRITE(ADDRESS, MEM_SIZE_32, VALUE); }

void M68K_MOVE_MEMORY_8(unsigned SRC, unsigned DEST, unsigned COUNT)    { MEMORY_MOVE(SRC, DEST, MEM_SIZE_8, COUNT); }
void M68K_MOVE_MEMORY_16(unsigned SRC, unsigned DEST, unsigned COUNT)   { MEMORY_MOVE(SRC, DEST, MEM_SIZE_16, COUNT); }
void M68K_MOVE_MEMORY_32(unsigned SRC, unsigned DEST, unsigned COUNT)   { MEMORY_MOVE(SRC, DEST, MEM_SIZE_32, COUNT); }
// OF COURSE THESE ARE CHANGED IN LIB68K TO HAVE NO LOCAL ARGS
// AS THE IMMEDIATE READ IS GOVERNED BY THE EA LOADED INTO MEMORY

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

    MEMORY_MAP(0x00000, 0x7FFFF, true);
    MEMORY_MAP(0x080000, 0x0FFFFF, false);  

    SHOW_MEMORY_MAPS();

    printf("TESTING BASIC READ AND WRITES\n");

    uint8_t TEST_8 = 0xAA;
    M68K_WRITE_MEMORY_8(0x1000, TEST_8);
    uint8_t READ_8 = M68K_READ_MEMORY_8(0x1000);

    uint16_t TEST_16 = 0xBBCC;
    M68K_WRITE_MEMORY_16(0x1010, TEST_16);
    uint16_t READ_16 = M68K_READ_MEMORY_16(0x1010);

    uint32_t TEST_32 = 0x134CA000;
    M68K_WRITE_MEMORY_32(0x1020, TEST_32);
    uint32_t READ_32 = M68K_READ_MEMORY_32(0x1020);

    uint16_t IMM_16 = 0xFFFF;
    M68K_WRITE_MEMORY_16(0x1000, IMM_16);
    uint16_t IMM_READ_16 = M68K_READ_IMM_16(0x1000);

    uint32_t IMM_32 = 0xFFFFFFFF;
    M68K_WRITE_MEMORY_32(0x1030, IMM_32);

    printf("\nTESTING WRITE PROTECTION\n");
    M68K_WRITE_MEMORY_32(0x400000, 0x42069);

    printf("\nTESTING MOVE OPERATIONS\n");
    uint8_t SRC = 0x20;
    uint8_t DEST = 0x40;
    unsigned COUNT = 8;

    M68K_MOVE_MEMORY_8(SRC, DEST, COUNT);

    M68K_STOPPED = 1;
    SHOW_MEMORY_MAPS();

    return 0;
}
