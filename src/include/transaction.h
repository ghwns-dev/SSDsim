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

    ppa_t ppa;

    pba_t pba;

    unit_t data;

    channel_t channel;

    uint64_t tick;  // pushed time
};