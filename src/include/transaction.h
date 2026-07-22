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

    ppa_t old_ppa;
    
    channel_t channel;

    uint64_t submit_tick;  // pushed time

    bool is_gc_transaction;
};