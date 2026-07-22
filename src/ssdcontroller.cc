#include "ssdcontroller.h"

ssdcontroller::ssdcontroller(int size) 
{
    init_ticks();
    dram_controller = new dramcontroller(size);
    flash_controller = new flashcontroller();

    gc_context.reset();
}

ssdcontroller::~ssdcontroller() 
{
    delete dram_controller;
    delete flash_controller;

    // delete_ticks();
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

void ssdcontroller::push_read_transaction(transaction_t i_trx)
{
    transaction_t trx;
    
    trx.type = NAND_READ;
    trx.lpa = i_trx.lpa;
    trx.ppa = i_trx.ppa;
    trx.old_ppa = i_trx.old_ppa;
    trx.pba = flash_controller->get_block_address(i_trx.ppa);
    trx.channel = flash_controller->get_channel(trx.pba);
    trx.submit_tick = get_ticks();
    trx.is_gc_transaction = i_trx.is_gc_transaction;

    dram_controller->push_transaction_queue(trx);

    return;
}

void ssdcontroller::push_program_transaction(transaction_t i_trx)
{
    transaction_t trx;
    
    trx.type = NAND_PROGRAM;
    trx.lpa = i_trx.lpa;
    trx.old_ppa = i_trx.old_ppa;
    trx.ppa = i_trx.ppa;
    trx.pba = flash_controller->get_block_address(i_trx.ppa);
    trx.channel = flash_controller->get_channel(trx.pba);
    trx.data = i_trx.data;
    trx.submit_tick = get_ticks();
    trx.is_gc_transaction = i_trx.is_gc_transaction;

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
    if(gc_context.running) return;

    if (!flash_controller->has_invalid_page())
        return;

    pba_t victim = flash_controller->get_victim_block();

    if (victim == FAULT)
        return;

    gc_context.running = true;
    gc_context.victim_block = victim;

    for(uint16_t page = 0; page < PAGE_PER_BLOCK; page++)
    {
        page_t page_entry = flash_controller->get_page(victim, page);

        if(page_entry.page_status != VALID)
            continue;

        ppa_t ppa = victim * PAGE_PER_BLOCK + page;

        lpa_t lpa = dram_controller->get_lpa_from_mapping_table(ppa);

        transaction_t trx_;
        trx_.lpa = lpa;
        trx_.ppa = ppa;
        trx_.old_ppa = ppa;
        trx_.is_gc_transaction = true;

        push_read_transaction(trx_);

        gc_context.pending_read++;
    }

    gc_context.pending_program = 0;
}

bool ssdcontroller::host_read(lpa_t i_lpa)
{
    table_entry_t table_entry = get_mapping_table_entry(i_lpa);

    if(table_entry.page_status != VALID){
        return false;
    }

    transaction_t trx_;
    trx_.lpa = i_lpa;
    trx_.ppa = table_entry.PPA;
    trx_.is_gc_transaction = false;

    push_read_transaction(trx_);
   
    history_table[i_lpa].read_cnt++;
    
	return true;
}

bool ssdcontroller::host_write(lpa_t i_lpa, unit_t i_data)
{
	history_table[i_lpa].program_cnt++;
	
	if(dram_controller->get_write_buffer_size() < dram_controller->get_max_write_buffer_size()) 
    {
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
			
			// dram_controller->update_mapping_table(lpa, ppa, VALID);
			
			// write_to_nand(ppa, buffer_entry.data);	// segfault, ppa == 8192
            // flash_controller->update_page_status(ppa, VALID);
            transaction_t trx_;
            trx_.lpa = lpa;
            trx_.old_ppa = -1;
            trx_.ppa = ppa;
            trx_.data = buffer_entry.data;
            trx_.is_gc_transaction = false;
            push_program_transaction(trx_);
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
					exit(-1);
				}
			}	
            
			// dram_controller->update_mapping_table(lpa, ppa, VALID); 
			
            // write_to_nand(ppa, buffer_entry.data); 
            // flash_controller->update_page_status(ppa, VALID);
            transaction_t trx_;
            trx_.lpa = lpa;
            trx_.ppa = ppa;
            trx_.old_ppa = prev_ppa;
            trx_.is_gc_transaction = false;
            trx_.data = buffer_entry.data;

            push_program_transaction(trx_);
			// if(table_entry.page_status == VALID) flash_controller->update_page_status(prev_ppa, INVALID);
		}
	}

    return true;
}

bool ssdcontroller::schedule_transaction()
{
    while(!dram_controller->is_transaction_queue_empty())
    {
        transaction_t trx = select_transaction();
        // transaction_t trx = dram_controller->get_transaction();

        if(trx.type == NAND_NONE) return false;

        unit_t data;
        bool read_success = true;

        pba_t pba = flash_controller->get_block_address(trx.ppa);

        switch(trx.type)
        {
            case NAND_PROGRAM:
                flash_controller->program_page(trx.ppa, trx.data);

                if(trx.is_gc_transaction) 
                {
                    gc_context.pending_program--;
                }

                flash_controller->update_page_status(trx.ppa, VALID);
                dram_controller->update_mapping_table(trx.lpa, trx.ppa, VALID);

                if(trx.old_ppa >= 0) 
                {
                    flash_controller->update_page_status(trx.old_ppa, INVALID);
                }
                break;

            case NAND_READ:
                read_success = flash_controller->read_page(trx.ppa, &data);
                if(read_success) 
                {
                    if(trx.is_gc_transaction)
                    {
                        dram_controller->push_gc_buffer(trx.lpa, trx.old_ppa, data);
                        gc_context.pending_read--;
                    }
                    else
                    {
                        dram_controller->push_read_result_buffer(trx.lpa, data);
                    }
                }
                break;

            case NAND_ERASE:
                flash_controller->erase_block(pba);
                gc_context.reset();
                break;
        }
    }

    return true;
}

transaction_t ssdcontroller::select_transaction()
{
    transaction_t trx;

    int transaction_queue_size = dram_controller->get_transaction_queue_size();

    int iteration_cnt = 0;

    transaction_t read_table[transaction_queue_size];
    transaction_t program_table[transaction_queue_size];
    transaction_t erase_table[transaction_queue_size];

    int read_table_size = 0;
    int program_table_size = 0;
    int erase_table_size = 0;

    while(iteration_cnt < transaction_queue_size)
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
    bool valid = true;

    if(!dram_controller->is_cmd_queue_empty()) 
    {   
        host_command_t cmd = get_command(); 

        switch(cmd.type)
        {
            case HOST_COMMAND_TYPE::HOST_READ:
                valid = host_read(cmd.LPA);
                break;

            case HOST_COMMAND_TYPE::HOST_WRITE:
                valid = host_write(cmd.LPA, cmd.data);
                break;

            default:
                break;
        }

        if(dram_controller->get_transaction_queue_size() >= TRANSACTION_FLUSH_TH)
        {
            schedule_transaction();
        }

        if(valid == false) failed_command++;
    	else succeed_command++;
    }
    else
    {
        schedule_transaction();
    }

    while(!dram_controller->is_gc_buffer_empty())
    {
        garbage_collection_buffer_entry_t gc_buffer_entry = dram_controller->get_gc_buffer_entry();

        ppa_t new_ppa = flash_controller->find_free_page();

        if(new_ppa != FAULT) 
        {
            transaction_t trx_;
            trx_.lpa = gc_buffer_entry.LPA;
            trx_.ppa = new_ppa;
            trx_.old_ppa = gc_buffer_entry.old_pba;
            trx_.data = gc_buffer_entry.data;
            trx_.is_gc_transaction = true;

            push_program_transaction(trx_);
            gc_context.pending_program++;
        }
    }

    check_garbage_collection_finish();

    return valid;
}

void ssdcontroller::check_garbage_collection_finish()
{
    if(!gc_context.running) return;

    if(gc_context.pending_read != 0 || gc_context.pending_program != 0) return;

    if(gc_context.erase_issued) return;

    push_erase_transaction(gc_context.victim_block);

    gc_context.erase_issued = true;

    return;
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