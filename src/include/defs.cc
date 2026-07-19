#include "defs.h"
#include <chrono>

uint64_t *ticks = nullptr;

static std::chrono::steady_clock::time_point start_time;

void init_ticks(){
        ticks = (uint64_t*)malloc(sizeof(uint64_t));
        std::memset(ticks, 0x0, sizeof(uint64_t));

        start_time = std::chrono::steady_clock::now();
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
        /*
        to be returning the current system time cycle
        */

        auto now = std::chrono::steady_clock::now();

        return std::chrono::duration_cast<std::chrono::microseconds>(now - start_time).count();
}

void delete_ticks(){
        free(ticks);
        return;
}




