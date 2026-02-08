#include "flashcontroller.h"

_flashcontroller::_flashcontroller() {

}

_flashcontroller::~_flashcontroller() {
    for (pba_t block = 0; block < MAX_BLOCK_ADDRESS; block++) 
        free(nand[block].pages);

    free(nand);
	free(history_table);
}

void _flashcontroller::initialize() {

    nand = (block_t*)malloc(sizeof(block_t) * MAX_BLOCK_ADDRESS);

    for (pba_t block = 0; block < MAX_BLOCK_ADDRESS; block++) {
        nand[block].pages = (page_t*)malloc(sizeof(page_t) * PAGE_PER_BLOCK);
	}

	for (pba_t block = 0; block < MAX_BLOCK_ADDRESS; block++) {
		
		nand[block].pba = block;
		nand[block].free_pages = PAGE_PER_BLOCK;
		nand[block].invalid_pages = 0;
		nand[block].erased_time = 0;
	
        for (uint16_t page = 0; page < PAGE_PER_BLOCK; page++) {
            nand[block].pages[page].page_status = INIT;
            nand[block].pages[page].data = NULL;
        }
    }
	
	history_table = (history_t*)malloc(sizeof(history_t) * MAX_PAGE_ADDRESS);

	for(ppa_t ppa = 0; ppa < MAX_PAGE_ADDRESS; ppa++){
		history_table[ppa].read_cnt = 0;
		history_table[ppa].program_cnt = 0;
		history_table[ppa].erase_cnt = 0;
	}

    free_block_ptr = (block_t*)malloc(sizeof(block_t));
	free_block_ptr = nand;

    return;
}

pba_t _flashcontroller::get_block_address(ppa_t i_ppa) {
    return i_ppa / PAGE_PER_BLOCK;
}

uint16_t _flashcontroller::get_page_index(ppa_t i_ppa) {
    return i_ppa - (get_block_address(i_ppa) * PAGE_PER_BLOCK);
}

unit_t _flashcontroller::read_page(ppa_t i_ppa) {
    pba_t block = get_block_address(i_ppa);
    ppa_t page_idx = get_page_index(i_ppa);

    if (nand[block].pages[page_idx].page_status != VALID) return NULL;

    unit_t data = nand[block].pages[page_idx].data;

    count_ticks(tREAD);
	
	history_table[i_ppa].read_cnt++;

    return data;
}

ppa_t _flashcontroller::find_free_page() {
	pba_t block_idx = -1;
    ppa_t page_idx = -1;

    for (pba_t block = 0; block < MAX_BLOCK_ADDRESS; block++) {
		if(block == free_block_ptr -> pba) continue;

        for (uint16_t page = 0; page < PAGE_PER_BLOCK; page++) {
            count_ticks(tREAD);

            if (nand[block].pages[page].page_status == FREE || nand[block].pages[page].page_status == INIT) {
                block_idx = block;
                page_idx = page;
                ppa_t ppa = block_idx * PAGE_PER_BLOCK + page_idx;

                return ppa;
            }
        }
    }

    return FAULT;
}

pba_t _flashcontroller::find_free_block() {
   
	pba_t free_block;
	
	count_ticks(tREAD);

	if(free_block_ptr != NULL) return free_block_ptr->pba;

    return FAULT;
}

pba_t _flashcontroller::get_victim_block() {
    pba_t victim_block = FAULT;

	uint16_t max_invalid_cnt = 0;

    for (pba_t block = 0; block < MAX_BLOCK_ADDRESS; block++) {
		count_ticks(tREAD);

        if (nand[block].invalid_pages > max_invalid_cnt) {
            victim_block = block;
            max_invalid_cnt = nand[block].invalid_pages;
        }
    }

    return victim_block;
}

page_t _flashcontroller::get_page(pba_t i_pba, ppa_t i_ppa) {
    uint16_t page_idx = get_page_index(i_ppa);

    count_ticks(tREAD);

    return nand[i_pba].pages[page_idx];
}

void _flashcontroller::update_page_status(ppa_t i_ppa, uint16_t i_page_status) {
    
	pba_t block = get_block_address(i_ppa);
    uint16_t page_idx = get_page_index(i_ppa);
	
	switch(nand[block].pages[page_idx].page_status){
		case INIT:
			if(i_page_status == FREE);
			else if(i_page_status == VALID) nand[block].free_pages--;
			else if(i_page_status == INVALID) {
				nand[block].free_pages--;
				nand[block].invalid_pages++;
			}
			else;
			break;
	
		case FREE:
			if(i_page_status == VALID) {
				nand[block].free_pages--;
			}
			else if(i_page_status == INVALID) {
				nand[block].free_pages--;
				nand[block].invalid_pages++;
			}
			else;
			break;

		case VALID:
			if(i_page_status == FREE) {
				nand[block].free_pages++;
			}
			else if(i_page_status == INVALID) {
				nand[block].invalid_pages++;
			}
			else;
			break;
		
		case INVALID:
			if(i_page_status == FREE) {
				nand[block].free_pages++;
				nand[block].invalid_pages--;
			}
			else if(i_page_status == VALID) {
				nand[block].invalid_pages--;
			}
			else;
			break;
	}

    nand[block].pages[page_idx].page_status = i_page_status;

    return;
}

void _flashcontroller::erase_block(pba_t i_pba) {

    count_ticks(tBERS);
    
	for (uint16_t page = 0; page < PAGE_PER_BLOCK; page++) {
        nand[i_pba].pages[page].data = NULL;
	
		ppa_t ppa = i_pba * PAGE_PER_BLOCK + page;

		history_table[ppa].erase_cnt++;

		update_page_status(ppa, FREE);
   }

	nand[i_pba].erased_time = get_ticks();

	set_free_block_ptr();
    
	return;
}

void _flashcontroller::set_free_block_ptr(){
	uint64_t oldest_erased_order = get_ticks();	
	uint64_t min = get_ticks();

	pba_t oldest_erased_block = free_block_ptr->pba;

	for(pba_t block = 0; block < MAX_BLOCK_ADDRESS; block++){
	
		count_ticks(tREAD);

		if(nand[block].free_pages < PAGE_PER_BLOCK) continue;

		if(oldest_erased_order > nand[block].erased_time){
			oldest_erased_order =  nand[block].erased_time;
			oldest_erased_block = block;
		}	
		/*
		if(oldest_erased_order - nand[block].erased_time < min){
			min = oldest_erased_order - nand[block].erased_time;
			oldest_erased_block = block;
		}*/
	}

	free_block_ptr = nand + oldest_erased_block;
	return;
}

void _flashcontroller::program_page(ppa_t i_ppa, unit_t i_data) {
    
	pba_t block = get_block_address(i_ppa);
    uint16_t page_idx = get_page_index(i_ppa);

    nand[block].pages[page_idx].data = i_data;

    count_ticks(tPROG);

	history_table[i_ppa].program_cnt++;
	
	update_page_status(i_ppa, VALID);
    return;
}

uint16_t _flashcontroller::get_number_of_free_blocks() {

    uint16_t block_cnt = 0;

	for(pba_t block = 0; block < MAX_BLOCK_ADDRESS; block++){
		count_ticks(tREAD);
		
		if(nand[block].free_pages == PAGE_PER_BLOCK) block_cnt++;
	}

    return block_cnt;
}

bool _flashcontroller::has_invalid_page() {

	for(pba_t block = 0; block < MAX_BLOCK_ADDRESS; block++){
		count_ticks(tREAD);

		if(nand[block].invalid_pages > 0) return true;
	}

    return false;
}

bool _flashcontroller::is_block_full(pba_t i_pba){
	if(nand[i_pba].free_pages == 0) return true;
	else return false;
}

double _flashcontroller::get_erase_variation(){
    double total_erase_cnt = 0;
	double mean_value = 0;
    double num_pages = MAX_PAGE_ADDRESS;

    for(ppa_t ppa = 0; ppa < MAX_PAGE_ADDRESS; ppa++){
        total_erase_cnt += history_table[ppa].erase_cnt;
    }

    mean_value = total_erase_cnt / num_pages; 

    double variation = 0;

    for(ppa_t ppa = 0; ppa < MAX_PAGE_ADDRESS; ppa++){
        variation += (mean_value - history_table[ppa].erase_cnt) * (mean_value - history_table[ppa].erase_cnt);
    }

    variation /= num_pages;

    return variation;
}

void _flashcontroller::show_valid_flash_pages() {
	
	std::cout << "\n/***********************************/\n";	
	std::cout << "\n* valid pages\n";
	
	uint64_t valid_cnt = 0;
    
	for (pba_t block = 0; block < MAX_BLOCK_ADDRESS; block++) {
        for (uint16_t page = 0; page < PAGE_PER_BLOCK; page++) {
            
			if (nand[block].pages[page].page_status == VALID) {
				ppa_t ppa = block * PAGE_PER_BLOCK + page;
				std::cout << "\nppa : " << ppa << " | data : " << nand[block].pages[page].data;
				valid_cnt++;
            }
		}
    }

	std::cout << "\n\ntotal valid pages : " << valid_cnt;
	
	double valid_page_rate = (static_cast<double>(valid_cnt)/static_cast<double>(MAX_PAGE_ADDRESS)) * 100.0;
	std::cout << "\nvalid pages rate : " << valid_page_rate << "%";	

	double erase_variation = get_erase_variation();
	std::cout << "\nerase variation : " << erase_variation;
	
	std::cout << "\n\n/***********************************/\n";	
   
	return;
}

void _flashcontroller::log_flash_status(){
    std::ofstream status_file("./log/flash_status");
    
	for (pba_t block = 0; block < MAX_BLOCK_ADDRESS; block++) {
        for (uint16_t page = 0; page < PAGE_PER_BLOCK; page++) {
            
			status_file << "\nblock : " << block << " | page : " << page << " | data : " << nand[block].pages[page].data << " | ";

            if (nand[block].pages[page].page_status == FREE) {
                status_file << "status : FREE";
            } else if (nand[block].pages[page].page_status == VALID) {
                status_file << "status : VALID";
            } else if (nand[block].pages[page].page_status == INVALID) {
                status_file << "status : INVALID";
            } else {
                status_file << "status : INIT";
            }
        }
        status_file << "\n\n/******************************************************/\n\n";
    }

    status_file << "free block pointer : " << free_block_ptr->pba << '\n';
    status_file.close();

    return;
}

void _flashcontroller::log_flash_history(){
    std::ofstream status_file("./log/history_status");

    for (pba_t block = 0; block < MAX_BLOCK_ADDRESS; block++) {
        for (uint16_t page = 0; page < PAGE_PER_BLOCK; page++) {
			ppa_t ppa = block * PAGE_PER_BLOCK + page;

			status_file << "\nPPA : " << ppa;
			status_file << " | read : " << history_table[ppa].read_cnt;
			status_file << " | program : " << history_table[ppa].program_cnt;
			status_file << " | erase : " << history_table[ppa].erase_cnt;
        }
        status_file << "\n\n/************************************************/\n";
    }

    status_file.close();

    return;
}

void _flashcontroller::log_gc(int cnt){
	std::string file_name = std::to_string(cnt);

	std::ofstream status_file("./gc/" + file_name);

	for (pba_t block = 0; block < MAX_BLOCK_ADDRESS; block++) {
        for (uint16_t page = 0; page < PAGE_PER_BLOCK; page++) {

            status_file << "\nblock : " << block << " | page : " << page << " | data : " << nand[block].pages[page].data << " | ";

            if (nand[block].pages[page].page_status == FREE) {
                status_file << "status : FREE";
            } else if (nand[block].pages[page].page_status == VALID) {
                status_file << "status : VALID";
            } else if (nand[block].pages[page].page_status == INVALID) {
                status_file << "status : INVALID";
            } else {
                status_file << "status : INIT";
            }
        }
        status_file << "\n\n/******************************************************/\n\n";
    }

    status_file << "free block pointer : " << free_block_ptr->pba << '\n';
    status_file.close();	
	return;	
}
