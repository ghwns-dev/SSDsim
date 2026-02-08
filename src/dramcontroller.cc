#include "dramcontroller.h"

_dramcontroller::_dramcontroller(int i_max_buffer_size){
    max_buffer_size = i_max_buffer_size;
}

_dramcontroller::~_dramcontroller(){
    free(mapping_table);
}

void _dramcontroller::initialize(){
    
	mapping_table = (table_entry_t*)malloc(sizeof(table_entry_t) * MAX_LOGICAL_ADDRESS);

    for(int i = 0; i < MAX_LOGICAL_ADDRESS; i++){
        mapping_table[i].page_status = INIT;
        mapping_table[i].PPA = FAULT;
    }

    return;
}

table_entry_t _dramcontroller::get_mapping_table_entry(lpa_t i_lpa){
    
	count_ticks(tDRAM);
   	table_entry_t entry = mapping_table[i_lpa];
    
	return entry;
}

lpa_t _dramcontroller::get_lpa_from_mapping_table(ppa_t i_ppa){
	lpa_t lpa = FAULT;
	
	for(lpa_t _lpa = 0; _lpa < MAX_LOGICAL_ADDRESS; _lpa++){
		if(mapping_table[_lpa].PPA == i_ppa && mapping_table[_lpa].page_status == VALID){
			lpa = _lpa;
		}	
	}	

	return lpa;
}

void _dramcontroller::push_command_queue(cmd_t i_cmd){
    
	count_ticks(tCMD + tDRAM);
    command_queue.push(i_cmd);
    
	return;
}

cmd_t _dramcontroller::get_command(){
	
	if(is_cmd_queue_empty()) return { NONE, 0, 0};

    count_ticks(tDRAM);
    cmd_t cmd = command_queue.front();
    
	command_queue.pop();
    
	return cmd;
}

int _dramcontroller::get_command_queue_size(){
	count_ticks(tDRAM);
    
	return command_queue.size();
}

bool _dramcontroller::is_cmd_queue_empty(){
	return command_queue.empty();
}

void _dramcontroller::update_mapping_table(lpa_t i_lpa, ppa_t i_ppa, uint16_t i_page_status){
    
	count_ticks(tDRAM);
    
	mapping_table[i_lpa].PPA = i_ppa;
    mapping_table[i_lpa].page_status = i_page_status;
    
	return;
}

int _dramcontroller::get_write_buffer_size(){
    count_ticks(tDRAM);
    
	return write_buffer.size();
}

int _dramcontroller::get_max_write_buffer_size(){
    count_ticks(tDRAM);
    
	return max_buffer_size;
}

void _dramcontroller::write_to_buffer(lpa_t i_lpa, unit_t i_data){
    
	count_ticks(tDRAM);
    
	buffer_entry_t buffer_entry;
    buffer_entry.LPA = i_lpa;
    buffer_entry.data = i_data;
    
	write_buffer.push(buffer_entry);
    
	return;
}

bool _dramcontroller::is_write_buffer_empty(){
    count_ticks(tDRAM);
    
	return write_buffer.empty();
}

buffer_entry_t _dramcontroller::get_front_buffer_entry(){
    
	count_ticks(tDRAM);
    
	buffer_entry_t buffer_entry = write_buffer.front();
    write_buffer.pop();
    
	return buffer_entry;
}

bool _dramcontroller::is_copy_data_buffer_empty(){
	count_ticks(tDRAM);
	
	return copy_data_buffer.empty();
}

void _dramcontroller::push_copy_data_buffer(unit_t i_data){
    
	count_ticks(tDRAM);
    copy_data_buffer.push(i_data);
    
	return;
}

unit_t _dramcontroller::get_copy_data_buffer(){

	if(copy_data_buffer.empty()) return NULL;
    
	count_ticks(tDRAM);
    
	unit_t data = copy_data_buffer.front();
	copy_data_buffer.pop();
    
	return data;
}

void _dramcontroller::log_table_status(){
    
	std::ofstream status_file("./log/table_status");
    
	for(int i = 0; i < MAX_LOGICAL_ADDRESS; i++){
        status_file << "LPA : " << i << " | PPA : " << mapping_table[i].PPA << " | status : ";
        
		if(mapping_table[i].page_status == FREE)
            status_file << "FREE\n";
        else if(mapping_table[i].page_status == VALID)
            status_file << "VALID\n";
        else if(mapping_table[i].page_status == INVALID)
            status_file << "INVALID\n";
        else
            status_file << "INIT\n";
    }
    
	status_file.close();

	return;
}

