#include "dramcontroller.h"

dramcontroller::dramcontroller(int i_max_buffer_size)
{
    max_buffer_size = i_max_buffer_size;
}

dramcontroller::~dramcontroller()
{
    free(mapping_table);
}

void dramcontroller::initialize()
{
    
	mapping_table = (table_entry_t*)malloc(sizeof(table_entry_t) * MAX_LOGICAL_ADDRESS);

    for(int i = 0; i < MAX_LOGICAL_ADDRESS; i++){
        mapping_table[i].page_status = INIT;
        mapping_table[i].PPA = FAULT;
    }

    return;
}

table_entry_t dramcontroller::get_mapping_table_entry(lpa_t i_lpa)
{
    
	count_ticks(tDRAM);
   	table_entry_t entry = mapping_table[i_lpa];
    
	return entry;
}

lpa_t dramcontroller::get_lpa_from_mapping_table(ppa_t i_ppa)
{
	lpa_t lpa = FAULT;
	
	for(lpa_t _lpa = 0; _lpa < MAX_LOGICAL_ADDRESS; _lpa++){
		if(mapping_table[_lpa].PPA == i_ppa && mapping_table[_lpa].page_status == VALID){
			lpa = _lpa;
		}	
	}	

	return lpa;
}

void dramcontroller::push_command_queue(host_command_t i_cmd)
{
    
	count_ticks(tCMD + tDRAM);
    command_queue.push(i_cmd);
    
	return;
}

host_command_t dramcontroller::get_command()
{
	
	if(is_cmd_queue_empty()) return { HOST_NONE, 0, 0};

    count_ticks(tDRAM);
    host_command_t cmd = command_queue.front();
    
	command_queue.pop();
    
	return cmd;
}

int dramcontroller::get_command_queue_size()
{
	count_ticks(tDRAM);
    
	return command_queue.size();
}

bool dramcontroller::is_cmd_queue_empty()
{
	return command_queue.empty();
}

void dramcontroller::push_transaction_queue(transaction_t i_trx, bool i_is_scheduling)
{
    // Not Actual DRAM Traffic, no timing increment
	if(!i_is_scheduling) count_ticks(tCMD + tDRAM);
    transaction_queue.push(i_trx);
    
	return;
}

transaction_t dramcontroller::get_transaction(bool i_is_scheduling)
{
	
	if(is_transaction_queue_empty()) return { NAND_NONE, 0, 0};

	// Not Actual DRAM Traffic, no timing increment
    if(!i_is_scheduling) count_ticks(tDRAM);
    transaction_t trx = transaction_queue.front();
    
	transaction_queue.pop();
    
	return trx;
}

transaction_t dramcontroller::get_transaction_with_parameter(transaction_t *i_trx, bool i_is_scheduling)
{
	transaction_t trx;

	int total_iteration = get_transaction_queue_size(i_is_scheduling);
	int iteration = 0;

	while(iteration < total_iteration)
	{
		transaction_t cur_trx = transaction_queue.front();
		transaction_queue.pop();

		if(memcmp(&cur_trx, i_trx, sizeof(transaction_t)) == 0)
		{
			trx = cur_trx;
		}
		else
		{
			transaction_queue.push(cur_trx);
		}

		iteration++;
	}

	return trx;
}

int dramcontroller::get_transaction_queue_size(bool i_is_scheduling)
{
	// Not Actual DRAM Traffic, no timing increment
	if(!i_is_scheduling) count_ticks(tDRAM);
    
	return transaction_queue.size();
}


bool dramcontroller::is_transaction_queue_empty()
{
	return transaction_queue.empty();
}

void dramcontroller::update_mapping_table(lpa_t i_lpa, ppa_t i_ppa, uint16_t i_page_status)
{
	count_ticks(tDRAM);
    
	mapping_table[i_lpa].PPA = i_ppa;
    mapping_table[i_lpa].page_status = i_page_status;
    
	return;
}

int dramcontroller::get_write_buffer_size()
{
    count_ticks(tDRAM);
    
	return write_buffer.size();
}

int dramcontroller::get_max_write_buffer_size()
{
    count_ticks(tDRAM);
    
	return max_buffer_size;
}

void dramcontroller::write_to_buffer(lpa_t i_lpa, unit_t i_data)
{
	count_ticks(tDRAM);
    
	buffer_entry_t buffer_entry;
    buffer_entry.LPA = i_lpa;
    buffer_entry.data = i_data;
    
	write_buffer.push(buffer_entry);
    
	return;
}

bool dramcontroller::is_write_buffer_empty()
{
    count_ticks(tDRAM);
    
	return write_buffer.empty();
}

buffer_entry_t dramcontroller::get_front_buffer_entry()
{
	count_ticks(tDRAM);
    
	buffer_entry_t buffer_entry = write_buffer.front();
    write_buffer.pop();
    
	return buffer_entry;
}

bool dramcontroller::is_copy_data_buffer_empty()
{
	count_ticks(tDRAM);
	
	return copy_data_buffer.empty();
}

void dramcontroller::push_copy_data_buffer(unit_t i_data)
{
	count_ticks(tDRAM);
    copy_data_buffer.push(i_data);
    
	return;
}

unit_t dramcontroller::get_copy_data_buffer()
{
	if(copy_data_buffer.empty()) return NULL;
    
	count_ticks(tDRAM);
    
	unit_t data = copy_data_buffer.front();
	copy_data_buffer.pop();
    
	return data;
}

void dramcontroller::log_table_status()
{
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

