#include "ssdcontroller.h"

ssdcontroller::ssdcontroller(int size) 
{
    init_ticks();
    dram_controller = new dramcontroller(size);
    flash_controller = new flashcontroller();
}

ssdcontroller::~ssdcontroller() 
{
    delete dram_controller;
    delete flash_controller;

    delete_ticks();
    free(history_table);
}

void ssdcontroller::initialize()
{
    dram_controller->initialize();
    flash_controller->initialize();

    history_table = (history_t*)malloc(sizeof(history_t) * MAX_LOGICAL_ADDRESS);

    for(lpa_t lpa = 0; lpa < MAX_LOGICAL_ADDRESS; lpa++){
        history_table[lpa].read_cnt = 0;
        history_table[lpa].program_cnt = 0;
    }

	uint64_t failed_command = 0;
	uint64_t succeed_command = 0;

    return;
}

table_entry_t ssdcontroller::get_mapping_table_entry(lpa_t i_lpa)
{
    table_entry_t table_entry = dram_controller->get_mapping_table_entry(i_lpa);

    return table_entry;
}

void ssdcontroller::push_command(host_command_t i_cmd)
{
    dram_controller->push_command_queue(i_cmd);
    return;
}

host_command_t ssdcontroller::get_command()
{	
	if(is_cmd_queue_empty()) return { HOST_NONE, NULL, NULL };
	
    host_command_t cmd = dram_controller->get_command();
    return cmd;
}

bool ssdcontroller::is_cmd_queue_empty()
{
	return dram_controller->is_cmd_queue_empty();
}

bool ssdcontroller::is_idle()
{   
    return
        dram_controller->is_cmd_queue_empty() &&
        dram_controller->is_transaction_queue_empty();
        // && !scheduler.has_pending_transaction();
}

void ssdcontroller::push_read_transaction(ppa_t i_ppa)
{
    transaction_t trx;
    
    trx.type = NAND_READ;
    trx.ppa = i_ppa;
    trx.pba = flash_controller->get_block_address(i_ppa);
    trx.channel = flash_controller->get_channel(trx.pba);
    trx.submit_tick = get_ticks();

    dram_controller->push_transaction_queue(trx);

    return;
}

void ssdcontroller::push_program_transaction(ppa_t i_ppa, unit_t i_data)
{
    transaction_t trx;
    
    trx.type = NAND_PROGRAM;
    trx.ppa = i_ppa;
    trx.pba = flash_controller->get_block_address(i_ppa);
    trx.channel = flash_controller->get_channel(trx.pba);
    trx.data = i_data;
    trx.submit_tick = get_ticks();

    dram_controller->push_transaction_queue(trx);

    return;
}

void ssdcontroller::push_erase_transaction(pba_t i_pba)
{
    transaction_t trx;

    trx.type = NAND_ERASE;
    trx.pba = i_pba;
    trx.channel = flash_controller->get_channel(trx.pba);
    trx.submit_tick = get_ticks();

    dram_controller->push_transaction_queue(trx);

    return;
}


bool ssdcontroller::garbage_collection_triggered()
{
	if(flash_controller->get_number_of_free_blocks()  < GC_START_THRESHOLD) return true;
 
	return false;
}

void ssdcontroller::garbage_collection() 
{
	if(!flash_controller->has_invalid_page()) return;

	pba_t victim_block = flash_controller->get_victim_block();

	if(victim_block == FAULT)
	{
		std::cout << "no victim block\n";
		return;
	}

	std::queue<lpa_t> lpa_buffer;

    for(uint16_t page = 0; page < PAGE_PER_BLOCK; page++)
    {
        page_t page_entry = flash_controller->get_page(victim_block, page);

        ppa_t old_ppa = victim_block * PAGE_PER_BLOCK + page;

        if(page_entry.page_status != VALID)
            continue;

        lpa_t lpa = dram_controller->get_lpa_from_mapping_table(old_ppa);

        lpa_buffer.push(lpa);

        dram_controller->push_copy_data_buffer(page_entry.data); // data buffer for being copied
    }

    flash_controller->erase_block(victim_block);

    while(!lpa_buffer.empty() &&
          !dram_controller->is_copy_data_buffer_empty())
    {
        lpa_t lpa = lpa_buffer.front();
        lpa_buffer.pop();

        unit_t data = dram_controller->get_copy_data_buffer();

        ppa_t new_ppa = flash_controller->find_free_page();  // request for lpa with data should be done with new_ppa

        if(new_ppa == FAULT)
        {
            std::cout
                << "gc error : no free page\n";
            exit(-1);
        }

        dram_controller->update_mapping_table(lpa,new_ppa,VALID);

        // write_to_nand(new_ppa, data);
        push_program_transaction(new_ppa, data);
    }
}

bool ssdcontroller::read(lpa_t i_lpa, unit_t *i_data_ptr)
{
    table_entry_t table_entry = get_mapping_table_entry(i_lpa);

    if(table_entry.page_status != VALID){
        std::memset(i_data_ptr, 0x00, sizeof(unit_t));
        return false;
    }

    unit_t data = flash_controller->read_page(table_entry.PPA);

    std::memcpy(i_data_ptr, &data, sizeof(unit_t));

    history_table[i_lpa].read_cnt++;
    
	return true;
}

bool ssdcontroller::program(lpa_t i_lpa, unit_t i_data)
{
	history_table[i_lpa].program_cnt++;
	
	if(dram_controller->get_write_buffer_size() < dram_controller->get_max_write_buffer_size()) {
		write_to_buffer(i_lpa, i_data);
		return true;
	}

	while(!dram_controller->is_write_buffer_empty()){
		
		buffer_entry_t buffer_entry = dram_controller->get_front_buffer_entry();

		table_entry_t table_entry = dram_controller->get_mapping_table_entry(buffer_entry.LPA);

		lpa_t lpa = buffer_entry.LPA;
	
		if(table_entry.page_status == INIT || table_entry.page_status == FREE){
			ppa_t ppa = flash_controller->find_free_page();
			
			if(ppa == FAULT)
			{
				garbage_collection();       // foreground Garbage Collection
				
				ppa = flash_controller->find_free_page();
			
				if(ppa == FAULT)
				{
					std::cout << "SSD is full\n";
					exit(-1);
				}
			}
			
			dram_controller->update_mapping_table(lpa, ppa, VALID);
			
			// write_to_nand(ppa, buffer_entry.data);	// segfault, ppa == 8192
            push_program_transaction(ppa, buffer_entry.data);
		}
		else {	
			// VALID or INVALID
			// There is no case that table entry is set as FREE status
			ppa_t prev_ppa = table_entry.PPA;
			
			ppa_t ppa = flash_controller->find_free_page();
			
			if(ppa == FAULT)
			{
				garbage_collection();

				ppa = flash_controller->find_free_page();
				// 2026/06/15
				if(ppa == FAULT)
				{
					std::cout << "program failed\n";
					exit(-1);
				}
			}	
			
            // write_to_nand(ppa, buffer_entry.data); 
            push_program_transaction(ppa, buffer_entry.data);

			dram_controller->update_mapping_table(lpa, ppa, VALID); 
			
			if(table_entry.page_status == VALID) flash_controller->update_page_status(prev_ppa, INVALID);
		}
	}
    return true;
}

void ssdcontroller::generate_transaction_from_command(host_command_t i_cmd)
{
    transaction_t trx;
    table_entry_t table_entry = dram_controller->get_mapping_table_entry(i_cmd.LPA);

    if(table_entry.page_status != VALID)
    {
        while(table_entry.PPA == FAULT)
        {
            table_entry.PPA = flash_controller->find_free_page();
        }
       
        dram_controller->update_mapping_table(i_cmd.LPA, table_entry.PPA, VALID);
    }

    switch(i_cmd.type)
    {
        case HOST_READ:
            trx.type = NAND_READ;
            trx.ppa = table_entry.PPA;
            trx.data = NULL;
            trx.pba = flash_controller->get_block_address(trx.ppa);
            trx.channel = flash_controller->get_channel(trx.pba);
            trx.submit_tick = get_ticks();
            break;
        
        case HOST_WRITE:
            trx.type = NAND_PROGRAM;
            trx.ppa = table_entry.PPA;
            trx.data = i_cmd.data;
            trx.pba = flash_controller->get_block_address(trx.ppa);
            trx.channel = flash_controller->get_channel(trx.pba);
            trx.submit_tick = get_ticks();
            break;

        default:
            trx.type = NAND_NONE;
            break;
    }

    dram_controller->push_transaction_queue(trx);
    return;
}

bool ssdcontroller::schedule_transaction()
{
    while(!dram_controller->is_transaction_queue_empty())
    {
        transaction_t trx = select_transaction();
        // transaction_t trx = dram_controller->get_transaction();

        if(trx.type == NAND_NONE) return false;

        unit_t data;
        pba_t pba = flash_controller->get_block_address(trx.ppa);

        switch(trx.type)
        {
            case NAND_PROGRAM:
                flash_controller->program_page(trx.ppa, trx.data);
                break;

            case NAND_READ:
                data = flash_controller->read_page(trx.ppa);
                break;

            case NAND_ERASE:
                flash_controller->erase_block(pba);
                break;
        }
    }

    return true;
}

transaction_t ssdcontroller::select_transaction()
{
    transaction_t trx;

    int transaction_queue_size = dram_controller->get_transaction_queue_size();

    int iteration_cnt = transaction_queue_size;

    transaction_t read_table[transaction_queue_size];
    transaction_t program_table[transaction_queue_size];
    transaction_t erase_table[transaction_queue_size];

    int read_table_size = 0;
    int program_table_size = 0;
    int erase_table_size = 0;

    while(iteration_cnt < TRANSACTION_FLUSH_TH)
    {
        transaction_t trx_ = dram_controller->get_transaction();

        switch(trx_.type)
        {
            case NAND_PROGRAM:
                memcpy(&program_table[program_table_size++], &trx_, sizeof(transaction_t));
                break;
            case NAND_READ:
                memcpy(&read_table[read_table_size++], &trx_, sizeof(transaction_t));
                break;
            case NAND_ERASE:
                memcpy(&erase_table[erase_table_size++], &trx_, sizeof(transaction_t));
                break;
            default:
            break;
        }
        iteration_cnt++;
        dram_controller->push_transaction_queue(trx_);
    } 

    iteration_cnt = 0;

    if(read_table_size > 0)
    {
        trx = find_least_recent_transaction(read_table, read_table_size);
    }
    else if(program_table_size > 0)
    {
        trx = find_least_recent_transaction(program_table, program_table_size);
    }
    else if(erase_table_size > 0)
    {
        trx = find_least_recent_transaction(erase_table, erase_table_size);
    }
    else
    {
        trx.type = NAND_NONE;
    }

    return trx;
}

transaction_t ssdcontroller::find_least_recent_transaction(transaction_t *i_table, int i_size)
{
    transaction_t trx;

    int least_submit_tick = get_ticks();
    int least_submit_transaction_index = -1;

    for(int i = 0; i < i_size; i++)
    {
        if(i_table[i].submit_tick < least_submit_tick)
        {
            least_submit_transaction_index = i;
            least_submit_tick = i_table[i].submit_tick;
        }
    }

    trx = dram_controller->get_transaction_with_parameter(&i_table[least_submit_transaction_index]);

    return trx;
}

void ssdcontroller::write_to_buffer(lpa_t i_lpa, unit_t i_data)
{
    dram_controller->write_to_buffer(i_lpa, i_data);

    return;
}

void ssdcontroller::write_to_nand(ppa_t i_ppa, unit_t i_data)
{
	flash_controller->program_page(i_ppa, i_data);

    return;
}

bool ssdcontroller::execute()
{
    if(dram_controller->is_cmd_queue_empty()) {
        return false;
    }

    bool valid = true;
    host_command_t cmd = get_command(); 

    generate_transaction_from_command(cmd);

    if(dram_controller->get_transaction_queue_size() == TRANSACTION_FLUSH_TH)
    {
        schedule_transaction();
    }

	if(valid == false) failed_command++;
	else succeed_command++;

    return valid;
}

void ssdcontroller::show_valid_flash_pages()
{
    flash_controller->show_valid_flash_pages();

    return;
}

void ssdcontroller::log_flash_status()
{
	flash_controller->log_flash_status();
	
	return;
}

void ssdcontroller::log_table_status()
{
    dram_controller->log_table_status();
    
	return;
}

void ssdcontroller::show_execution_result()
{    
	uint64_t total_read_cnt = 0;
    uint64_t total_program_cnt = 0;
    uint64_t total_erase_cnt = flash_controller->get_total_erase_count();

    for(lpa_t lpa = 0; lpa < MAX_LOGICAL_ADDRESS; lpa++){
        total_read_cnt += history_table[lpa].read_cnt;
        total_program_cnt += history_table[lpa].program_cnt;
    }

    std::cout << "\ntotal read : " << total_read_cnt;
    std::cout << "\ntotal program : " << total_program_cnt;
    std::cout << "\ntotal erase of blocks : " << total_erase_cnt;

    std::cout << "\n\ntotal request : " << failed_command + succeed_command;
    std::cout << "\nsucceed request : " << succeed_command;
    std::cout << "\nfailed request : " << failed_command;
    std::cout << "\nrequest accuracy : " << (static_cast<double>(succeed_command) / static_cast<double>(failed_command + succeed_command)) * 100.0 << "%";
    std::cout << "\n\ntotal cycles : " << flash_controller->get_total_cycles();
    std::cout << "\n\n";
	
	return;
}

void ssdcontroller::show_stats()
{
#ifdef _LOG_
	log_flash_status();
    log_table_status();
#endif
    show_valid_flash_pages();
	show_execution_result();
    return;
}

