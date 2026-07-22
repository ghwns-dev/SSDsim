#include "defs.h"

enum TRANSACTION_TYPE
{
    NAND_READ,
    NAND_PROGRAM,
    NAND_ERASE,
    NAND_NONE
};

struct transaction_t
{
    TRANSACTION_TYPE type;

    unit_t data;

    lpa_t lpa;
    ppa_t ppa;
    pba_t pba;
    
    channel_t channel;

    uint64_t submit_tick;  // pushed time
};