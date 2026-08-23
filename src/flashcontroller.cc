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

		// [FIX 2026/08/23] erased_count was never initialized here even though
		// every other block_t field is. nand[] is malloc'd (not calloc'd), so
		// this field started as whatever garbage bytes happened to be in that
		// heap memory, and get_total_erase_count()/get_erase_variation() sum
		// it across all blocks -- producing nonsense output (e.g. "total
		// erase of blocks : 15553137160186487780", "erase variation :
		// 1.7579e+38") even though nothing was actually broken by the actual
		// erase logic itself.
		nand[block].erased_count = 0;

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

    // count_ticks(tREAD);

	channel_t channel = get_channel(block);

	uint64_t start_tick = std::max(get_ticks(), channel_busy[channel]);

	channel_busy[channel] = start_tick + tREAD;

    // [FIX 2026/08/23] Moved the tREAD channel charge above the validity
    // check. A read that lands on a stale/invalidated page still requires
    // the SSD to actually access the NAND array to find that out -- it isn't
    // free. Charging it here also keeps per-transaction latency accounting
    // (completion_tick - submit_tick) consistent for every dispatched READ,
    // not just the ones that happen to still be valid by the time they're
    // serviced.
    if (nand[block].pages[page_idx].page_status != VALID) return NULL;

    unit_t data = nand[block].pages[page_idx].data;

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

			// [FIX 2026/08/23] Scanning page_status is a metadata lookup the
			// controller keeps in DRAM, not an actual NAND array access. This
			// used to charge a full tREAD to the channel for every page
			// scanned while searching for a free page (tens to hundreds of
			// scans per call as a block fills up -> inflated total cycles by
			// up to tens of ms per single program). The real NAND access is
			// already charged separately in program_page(), so the channel
			// cost here has been removed.
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

	// [FIX 2026/08/23] free_block_ptr is just a pointer kept in DRAM being
	// returned as-is, not an actual NAND access. Removed the tREAD channel
	// cost (and its queueing-delay calculation) that was charged here.
	if(free_block_ptr != NULL)
	{
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

		// [FIX 2026/08/23] invalid_pages/erased_count are per-block metadata
		// kept in DRAM, so scanning them to pick a GC victim is not an
		// actual NAND access. Removed the tREAD channel cost charged here.
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

	// [FIX 2026/08/23] This function is only used during GC scans to check
	// page status, and page_status is DRAM-resident metadata, so the tREAD
	// channel cost charged here has been removed.
	// Note: garbage_collection() also copies out the .data of VALID pages
	// through this same call, and that copy really does require a NAND
	// read whose cost isn't modeled anywhere right now. For better
	// accuracy, consider splitting "status lookup" from "valid-data read"
	// and charging tREAD only on the latter.

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

	uint64_t start_tick = std::max(get_ticks(), channel_busy[channel]);

	channel_busy[channel] = start_tick + tBERS;

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

	uint64_t start_tick = std::max(get_ticks(), channel_busy[channel]);

	channel_busy[channel] = start_tick + tPROG;

    return;
}

uint16_t flashcontroller::get_number_of_free_blocks() 
{
    uint16_t block_cnt = 0;

	for(pba_t block = 0; block < MAX_BLOCK_ADDRESS; block++){
		// count_ticks(tREAD);

		// [FIX 2026/08/23] Counting free_pages is a scan over DRAM
		// metadata, not an actual NAND access. Removed the tREAD channel
		// cost charged here.
		if(nand[block].free_pages == PAGE_PER_BLOCK) block_cnt++;
	}

    return block_cnt;
}

bool flashcontroller::has_invalid_page() 
{
	for(pba_t block = 0; block < MAX_BLOCK_ADDRESS; block++){
		// count_ticks(tREAD);

		// [FIX 2026/08/23] Counting invalid_pages is a scan over DRAM
		// metadata, not an actual NAND access. Removed the tREAD channel
		// cost charged here.
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

// [FIX 2026/08/23] Exposes a single channel's current busy tick to callers.
// Right after read_page()/program_page() sets channel_busy[channel] =
// start_tick + cost, this value IS that transaction's completion tick, so
// ssdcontroller::schedule_transaction() can compute per-request latency
// (completion_tick - submit_tick) without duplicating the timing model.
uint64_t flashcontroller::get_channel_busy(channel_t i_channel)
{
	return channel_busy[i_channel];
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