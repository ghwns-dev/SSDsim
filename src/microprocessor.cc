#include "microprocessor.h"

microprocessor::microprocessor(int size) {
    init_ticks();
    dramcontroller = new _dramcontroller(size);
    flashcontroller = new _flashcontroller();
}

microprocessor::~microprocessor() {
    delete dramcontroller;
    delete flashcontroller;

    delete_ticks();
    free(history_table);
}

void microprocessor::initialize(){

    dramcontroller->initialize();
    flashcontroller->initialize();

    history_table = (history_t*)malloc(sizeof(history_t) * MAX_LOGICAL_ADDRESS);

    for(lpa_t lpa = 0; lpa < MAX_LOGICAL_ADDRESS; lpa++){
        history_table[lpa].read_cnt = 0;
        history_table[lpa].erase_cnt = 0;
        history_table[lpa].program_cnt = 0;
    }

	uint64_t failed_command = 0;
	uint64_t succeed_command = 0;

    return;
}

table_entry_t microprocessor::get_mapping_table_entry(lpa_t i_lpa){
    table_entry_t table_entry = dramcontroller->get_mapping_table_entry(i_lpa);

    return table_entry;
}

void microprocessor::push_command(cmd_t i_cmd){
    dramcontroller->push_command_queue(i_cmd);
    return;
}

cmd_t microprocessor::get_command(){
	
	if(is_cmd_queue_empty()) return { NONE, NULL, NULL };
	
    cmd_t cmd = dramcontroller->get_command();
    return cmd;
}

bool microprocessor::is_cmd_queue_empty(){
	return dramcontroller->is_cmd_queue_empty();
}

bool microprocessor::garbage_collection_triggered(){

    if(flashcontroller->find_free_page() == FAULT) return true;

    return false;
}

int gc_cnt = 0;

void microprocessor::garbage_collection(){
    pba_t free_block = flashcontroller->find_free_block();
	std::cout << "\n\n================\n\ngc start, free block : " << free_block << '\n';
	
	if(free_block == FAULT) {
		std::cout << "system shutdown\n";
		exit(-1);
	}
   
	uint64_t total_valid_pages = 0;
	uint64_t acc_valid_pages = 0;
 
	uint16_t page_offset = 0;
	
	while(flashcontroller->has_invalid_page()){

        pba_t victim_block = flashcontroller->get_victim_block();
        
		std::queue<lpa_t> lpa_buffer;

        for(uint16_t page = 0; page < PAGE_PER_BLOCK; page++){
            page_t page_entry = flashcontroller->get_page(victim_block, page);

            ppa_t ppa = victim_block * PAGE_PER_BLOCK + page;

            lpa_t lpa = dramcontroller->get_lpa_from_mapping_table(ppa); // v1.9 revision. get lpa whose ppa is valid

            if(page_entry.page_status == VALID) {	
				
				history_table[lpa].erase_cnt++;
                 
				lpa_buffer.push(lpa);
				
				dramcontroller->push_copy_data_buffer(page_entry.data);
				
				total_valid_pages++;
			}
            // v1.8 revision. all ppa at victim block should be mapped again.
        }

        flashcontroller->erase_block(victim_block);
        std::cout << "\nvictim block : " << victim_block << '\n';
		std::cout << "free block : " << free_block << "\n";
		uint64_t valid_pages = 0;
		
		while(!lpa_buffer.empty() && !dramcontroller->is_copy_data_buffer_empty()){
            lpa_t lpa = lpa_buffer.front();
            lpa_buffer.pop();

            unit_t data = dramcontroller->get_copy_data_buffer();

            ppa_t ppa = free_block * PAGE_PER_BLOCK + page_offset;

			dramcontroller->update_mapping_table(lpa, ppa, VALID);
			// 2025/09/17 added line. 

			write_to_nand(ppa, data);			// 2025/09/16 Omitted at version 1.71

            page_offset++;
			valid_pages++;
		
			if(flashcontroller->is_block_full(free_block)) {
				page_offset = 0;
				free_block = flashcontroller->find_free_block();
				std::cout << "new free block : " << free_block << '\n';
			}
        }
		acc_valid_pages += valid_pages;
		std::cout << "number of valid pages for victim block " << victim_block << " is " << valid_pages << '\n'; 
		std::cout << "accumulated valid pages for victim block " << acc_valid_pages << '\n'; 
    }
	std::cout << "\ntotal_valid_pages : " << total_valid_pages << '\n';
	std::cout << "\n================\n";

	gc_cnt++;
	flashcontroller->log_gc(gc_cnt);
    return;
}

bool microprocessor::read(lpa_t i_lpa, unit_t *i_data_ptr){
    table_entry_t table_entry = get_mapping_table_entry(i_lpa);

    if(table_entry.page_status != VALID){
        std::memset(i_data_ptr, 0x00, sizeof(unit_t));
        return false;
    }

    unit_t data = flashcontroller->read_page(table_entry.PPA);

    std::memcpy(i_data_ptr, &data, sizeof(unit_t));

    history_table[i_lpa].read_cnt++;
    
	return true;
}

bool microprocessor::program(lpa_t i_lpa, unit_t i_data){
	history_table[i_lpa].program_cnt++;
	
	if(dramcontroller->get_write_buffer_size() < dramcontroller->get_max_write_buffer_size()) {
		write_to_buffer(i_lpa, i_data);
		return true;
	}

	while(!dramcontroller->is_write_buffer_empty()){
		
		buffer_entry_t buffer_entry = dramcontroller->get_front_buffer_entry();

		table_entry_t table_entry = dramcontroller->get_mapping_table_entry(buffer_entry.LPA);

		lpa_t lpa = buffer_entry.LPA;
	
		if(table_entry.page_status == INIT || table_entry.page_status == FREE){
			ppa_t ppa = flashcontroller->find_free_page();

			dramcontroller->update_mapping_table(lpa, ppa, VALID);
			
			write_to_nand(ppa, buffer_entry.data);
		}
		else {	
			// VALID or INVALID
			// There is no case that table entry is set as FREE status
			ppa_t prev_ppa = table_entry.PPA;
			
			if(garbage_collection_triggered()){
				garbage_collection();		
			}
			ppa_t ppa = flashcontroller->find_free_page();
			
			dramcontroller->update_mapping_table(lpa, ppa, VALID); 
			
			if(table_entry.page_status == VALID) flashcontroller->update_page_status(prev_ppa, INVALID);
			
			write_to_nand(ppa, buffer_entry.data); 
		}
	}
    return true;
}

void microprocessor::write_to_buffer(lpa_t i_lpa, unit_t i_data){
    dramcontroller->write_to_buffer(i_lpa, i_data);

    return;
}

void microprocessor::write_to_nand(ppa_t i_ppa, unit_t i_data){
	flashcontroller->program_page(i_ppa, i_data);

    return;
}

bool microprocessor::execute(){

    if(dramcontroller->is_cmd_queue_empty()) {
        return false;
    }

    bool valid = true;
    cmd_t cmd = get_command();

    switch(cmd.type){
        case CMD::READ:
            unit_t data;
            valid = read(cmd.LPA, &data);
            break;
        case CMD::PROGRAM:
			valid = program(cmd.LPA, cmd.data);
            break;
    }

	if(valid == false) failed_command++;
	else succeed_command++;

    return valid;
}

void microprocessor::show_valid_flash_pages(){
    flashcontroller->show_valid_flash_pages();

    return;
}

void microprocessor::log_flash_status(){
	flashcontroller->log_flash_status();
	
	return;
}

void microprocessor::log_flash_history(){
	flashcontroller->log_flash_history();	
    
	return;
}

void microprocessor::log_table_status(){
    dramcontroller->log_table_status();
    
	return;
}

void microprocessor::show_execution_result(){
    
	uint64_t total_read_cnt = 0;
    uint64_t total_program_cnt = 0;
    uint64_t total_erase_cnt = 0;

    for(lpa_t lpa = 0; lpa < MAX_LOGICAL_ADDRESS; lpa++){
        total_read_cnt += history_table[lpa].read_cnt;
        total_program_cnt += history_table[lpa].program_cnt;
        total_erase_cnt += history_table[lpa].erase_cnt;
    }

    std::cout << "\ntotal read : " << total_read_cnt;
    std::cout << "\ntotal program : " << total_program_cnt;
    std::cout << "\ntotal erase : " << total_erase_cnt;

    std::cout << "\n\ntotal request : " << failed_command + succeed_command;
    std::cout << "\nsucceed request : " << succeed_command;
    std::cout << "\nfailed request : " << failed_command;
    std::cout << "\nrequest accuracy : " << (static_cast<double>(succeed_command) / static_cast<double>(failed_command + succeed_command)) * 100.0 << "%";
    std::cout << "\n\ntotal cycles : " << get_ticks();
    std::cout << "\n\n";
    return;
}

void microprocessor::show_stats(){
    show_valid_flash_pages();
#ifdef _LOG_
	log_flash_status();
    log_flash_history();
    log_table_status();
#endif
	show_execution_result();

    return;
}

