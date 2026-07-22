#include "flashcontroller.h"

flashcontroller::flashcontroller() 
{

}

flashcontroller::~flashcontroller() 
{
    for (pba_t block = 0; block < MAX_BLOCK_ADDRESS; block++) 
        free(nand[block].pages);

    free(nand);
}

void flashcontroller::initialize() 
{
    nand = (block_t*)malloc(sizeof(block_t) * MAX_BLOCK_ADDRESS);

	for(channel_t channel = 0; channel < NUMBER_OF_CHANNEL; channel++)
	{
		channel_busy[channel] = 0;
	}

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

    free_block_ptr = (block_t*)malloc(sizeof(block_t));
	free_block_ptr = nand;

    return;
}

pba_t flashcontroller::get_block_address(ppa_t i_ppa) 
{
    return i_ppa / PAGE_PER_BLOCK;
}

uint16_t flashcontroller::get_page_index(ppa_t i_ppa) 
{
    return i_ppa - (get_block_address(i_ppa) * PAGE_PER_BLOCK);
}

channel_t flashcontroller::get_channel(pba_t i_pba)
{
	return i_pba % NUMBER_OF_CHANNEL;	
}

unit_t flashcontroller::read_page(ppa_t i_ppa) 
{
    pba_t block = get_block_address(i_ppa);
    ppa_t page_idx = get_page_index(i_ppa);

    if (nand[block].pages[page_idx].page_status != VALID) return NULL;

    unit_t data = nand[block].pages[page_idx].data;

	channel_t channel = get_channel(block);

	channel_busy[channel] += tREAD;
	
    return data;
}

ppa_t flashcontroller::find_free_page()
{
	// 2026/07/02 tREAD unnecessary, metadata
	ppa_t ppa;
	while(true)
	{
		if(free_block_ptr == NULL)
		{
			std::cout << "free block pointer is null\n";
			return FAULT;
		}

		pba_t block_idx = free_block_ptr->pba;

		for(uint16_t page = 0; page < PAGE_PER_BLOCK; page++){
			// count_ticks(tREAD);

			channel_t channel = get_channel(block_idx);

			// uint64_t start_tick = std::max(get_ticks(), channel_busy[channel]);

			channel_busy[channel] += tREAD;

			uint16_t page_status = nand[block_idx].pages[page].page_status;
			if(page_status == FREE || page_status == INIT)
			{
				ppa = block_idx * PAGE_PER_BLOCK + page;
				update_page_status(ppa, VALID);
				
				return ppa;
			}
		}

		// if get to here, all blocks are full.

		set_free_block_ptr();	

		if(free_block_ptr == NULL){
			return FAULT;
		}

		if(free_block_ptr->pba == block_idx)
		{
			return FAULT;
		}
	}	
}

pba_t flashcontroller::find_free_block() 
{
	// count_ticks(tREAD);
	// 2026/07/02 tREAD 불필요, DRAM에 저장해야 할 메타데이터

	if(free_block_ptr != NULL) 
	{
		channel_t channel = get_channel(free_block_ptr->pba);

		// uint64_t start_tick = std::max(get_ticks(), channel_busy[channel]);

		channel_busy[channel] += tREAD;

		return free_block_ptr->pba;
	}

    return FAULT;
}

pba_t flashcontroller::get_victim_block() 
{
	// 2026/07/02 tREAD 불필요, DRAM에 저장해야되는 메타데이터임
    pba_t victim_block = FAULT;

	double best_score = -1e30;
   
	for(pba_t block = 0; block < MAX_BLOCK_ADDRESS; block++)
	{
		// count_ticks(tREAD);

		channel_t channel = get_channel(block);

		// uint64_t start_tick = std::max(get_ticks(), channel_busy[channel]);

		channel_busy[channel] += tREAD;

		if(nand[block].invalid_pages == 0) continue;

		double score = nand[block].invalid_pages - SCORE_PARAMETER * nand[block].erased_count;

		if(score > best_score)
		{
			best_score = score;
			victim_block = block;
		}
	}

	if(victim_block == FAULT)
	{
		std::cout << "no victim block\n";
		exit(-1);
	}

    return victim_block;
}

page_t flashcontroller::get_page(pba_t i_pba, ppa_t i_ppa) 
{
	// 2026/07/02 page status를 가져온다면 tREAD 불필요, page status는 DRAM에 저장될 메타데이터임
    uint16_t page_idx = get_page_index(i_ppa);

    // count_ticks(tREAD);
	channel_t channel = get_channel(i_pba);

	// uint64_t start_tick = std::max(get_ticks(), channel_busy[channel]);

	channel_busy[channel] += tREAD;

    return nand[i_pba].pages[page_idx];
}

void flashcontroller::update_page_status(ppa_t i_ppa, uint16_t i_page_status) 
{    
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

void flashcontroller::erase_block(pba_t i_pba) 
{
    // count_ticks(tBERS);
    
	for (uint16_t page = 0; page < PAGE_PER_BLOCK; page++) {
        nand[i_pba].pages[page].data = NULL;
	
		ppa_t ppa = i_pba * PAGE_PER_BLOCK + page;

		update_page_status(ppa, FREE);
   }

	free_block_ptr = nand + i_pba;

    channel_t channel = get_channel(i_pba);

	nand[i_pba].erased_time = channel_busy[channel];
	nand[i_pba].erased_count++;
	
	// uint64_t start_tick = std::max(get_ticks(), channel_busy[channel]);

	channel_busy[channel] += tBERS;

	return;
}

void flashcontroller::set_free_block_ptr()
{
    uint64_t oldest = ULLONG_MAX;
    pba_t selected = FAULT;

    for(pba_t block = 0; block < MAX_BLOCK_ADDRESS; block++)
    {
        if(nand[block].free_pages != PAGE_PER_BLOCK)
            continue;

        if(nand[block].erased_time < oldest)
        {
            oldest = nand[block].erased_time;
            selected = block;
        }
    }

    if(selected == FAULT)
    {
        free_block_ptr = NULL;
        return;
    }

    free_block_ptr = &nand[selected];
}

void flashcontroller::program_page(ppa_t i_ppa, unit_t i_data) 
{    
	pba_t block = get_block_address(i_ppa);
    uint16_t page_idx = get_page_index(i_ppa);

    nand[block].pages[page_idx].data = i_data;

	channel_t channel = get_channel(block);

	channel_busy[channel] += tPROG;

    return;
}

uint16_t flashcontroller::get_number_of_free_blocks() 
{
    uint16_t block_cnt = 0;

	for(pba_t block = 0; block < MAX_BLOCK_ADDRESS; block++){
		// count_ticks(tREAD);

		channel_t channel = get_channel(block);

		// uint64_t start_tick = std::max(get_ticks(), channel_busy[channel]);

		channel_busy[channel] += tREAD;
		
		if(nand[block].free_pages == PAGE_PER_BLOCK) block_cnt++;
	}

    return block_cnt;
}

bool flashcontroller::has_invalid_page() 
{
	for(pba_t block = 0; block < MAX_BLOCK_ADDRESS; block++){
		// count_ticks(tREAD);

		channel_t channel = get_channel(block);

		// uint64_t start_tick = std::max(get_ticks(), channel_busy[channel]);

		channel_busy[channel] += tREAD;

		if(nand[block].invalid_pages > 0) return true;
	}

    return false;
}

bool flashcontroller::is_block_full(pba_t i_pba)
{
	if(nand[i_pba].free_pages == 0) return true;
	else return false;
}

double flashcontroller::get_erase_variation()
{
    double total_erase_cnt = get_total_erase_count();
	double mean_value = 0;

    mean_value = total_erase_cnt / MAX_BLOCK_ADDRESS; 

    double variation = 0;
    
	for(pba_t block = 0; block < MAX_BLOCK_ADDRESS; block++){
        variation += (mean_value - nand[block].erased_count) * (mean_value - nand[block].erased_count);
    }

    variation /= MAX_BLOCK_ADDRESS;

    return variation;
}

uint64_t flashcontroller::get_total_erase_count()
{
	uint64_t total_erase_cnt = 0;
	
	for(pba_t block = 0; block < MAX_BLOCK_ADDRESS; block++){
        total_erase_cnt += nand[block].erased_count;
    }

	return total_erase_cnt;
}

uint64_t flashcontroller::get_total_cycles()
{
	uint64_t total_cycles = 0;

	for(channel_t channel = 0; channel < NUMBER_OF_CHANNEL; channel++)
	{
		total_cycles = std::max(total_cycles, channel_busy[channel]);
	}

	return total_cycles;
}

void flashcontroller::show_valid_flash_pages() 
{
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
	std::cout << "\nerase variation : " << erase_variation << "\n";
   
	return;
}

void flashcontroller::log_flash_status()
{
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

void flashcontroller::log_gc(int cnt)
{
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
