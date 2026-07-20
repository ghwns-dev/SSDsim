#include "workload_generator.h"

workload_generator::workload_generator(int i_cmd_cnt) 
{
    total_command_count = i_cmd_cnt;
}

workload_generator::~workload_generator() 
{

}

host_command_t workload_generator::generate_command(WORKLOAD_TYPE i_type)
{
    host_command_t command;

    switch(i_type)
    {
        case WORKLOAD_TYPE::RANDOM:
        command.type = static_cast<HOST_COMMAND_TYPE>(cmd_type_generator());

        case WORKLOAD_TYPE::READ_HEAVY:
        command.type = heavy_read_generator();

    }

    command.LPA = logical_address_generator();

    if(command.type == HOST_COMMAND_TYPE::HOST_WRITE) command.data = double_words_data_generator();

    return command;
}