#include "defs.h"

uint64_t *ticks = nullptr;

void init_ticks(){
        ticks = (uint64_t*)malloc(sizeof(uint64_t));
        std::memset(ticks, 0x0, sizeof(uint64_t));
        return;
}

void count_ticks(uint64_t increment){
        uint64_t current_ticks = *ticks;
        current_ticks += increment;
        std::memcpy(ticks, &current_ticks, sizeof(uint64_t));

        return;
}

uint64_t get_ticks(){
        uint64_t _ticks = *ticks;
        return _ticks;
}

void delete_ticks(){
        free(ticks);
        return;
}




