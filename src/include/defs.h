#include <iostream>
#include <cstring>
#include <cstdlib>
#include <stdlib.h>
#include <cmath>
#include <algorithm>
#include <vector>
#include <queue>
#include <climits>
#include <bitset>
#include <unistd.h>

#ifndef __DEFS__
#define __DEFS__

// DRAM
// #define MAX_LOGICAL_ADDRESS 256
#define MAX_LOGICAL_ADDRESS 1024
// *********Logical

// NAND-Physical

#define PAGE_SIZE 8	// 8B Page Size
#define MAX_BLOCK_ADDRESS 32
#define BLOCK_PER_NAND MAX_BLOCK_ADDRESS
// #define PAGE_PER_BLOCK 32
// #define MAX_PAGE_ADDRESS 1024
#define PAGE_PER_BLOCK 256
#define MAX_PAGE_ADDRESS MAX_BLOCK_ADDRESS*PAGE_PER_BLOCK

#define TOTAL_SSD_SIZE PAGE_SIZE*PAGE_PER_BLOCK*BLOCK_PER_NAND	// 512KB SSD

#define FAULT MAX_PAGE_ADDRESS

// Timing Parameters - unit : cycle, 1GHz Operation
// 1 cycle == 10^-9 sec


#define tCMD 1	// Send Command
#define tDRAM 5	// Access DRAM

#define tREAD 100000	// Access Page
#define tPROG 700000	// Program Page
#define tBERS 5000000	// Erase Block

typedef uint16_t lpa_t;
typedef uint16_t ppa_t;
typedef uint16_t pba_t;

typedef uint64_t unit_t;	// 8B for for Page Data

// Physical Entity
typedef struct page {
	unit_t data;	// 256 items
	uint16_t page_status;
} page_t;

typedef struct block {
	page_t *pages;	// 32 items
	pba_t pba;

	uint16_t free_pages;
	uint16_t invalid_pages;
	uint64_t erased_time;
} block_t;

pba_t GetPhysicalBlockAddress(ppa_t);
// Physical Done

enum PAGE_STATUS {
	INIT = 0,
	FREE,
	VALID,
	INVALID,
};

typedef struct table_entry {
	ppa_t PPA;
	uint16_t page_status;
} table_entry_t;

typedef struct buffer_entry {
	lpa_t LPA;
	unit_t data;
} buffer_entry_t;

// Cmd definition
enum CMD {
	READ = 0,
	PROGRAM,
	ERASE,
	NONE
};

// Host-side Command
typedef struct cmd {
	uint8_t type;
	lpa_t LPA;	
	unit_t data;
} cmd_t;

typedef struct history {
        uint64_t read_cnt;
        uint64_t erase_cnt;
        uint64_t program_cnt;
} history_t;

// Timer
extern uint64_t *ticks;	// Global Timer

void init_ticks();

void count_ticks(uint64_t);

uint64_t get_ticks();

void delete_ticks();
// done
#endif
