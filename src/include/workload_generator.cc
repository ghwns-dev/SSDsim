#include "workload_generator.h"

workload_generator::workload_generator(WORKLOAD_TYPE i_type, int i_cmd_cnt) 
{
    workload_type = i_type;
    total_command_count = i_cmd_cnt;
    current_command_count = 0;

    sequential_address = logical_address_generator();
    training_read_address = 0;
    training_write_address = MAX_LOGICAL_ADDRESS / 2;
    checkpoint_address = logical_address_generator();

    checkpoint_mode = false;
    checkpoint_remaining = 0;
}

workload_generator::~workload_generator() 
{

}

host_command_t workload_generator::generate_command()
{
    host_command_t command;

    switch(workload_type) 
    {
        case WORKLOAD_TYPE::RANDOM:
            command = random_workload();
            break;

        case WORKLOAD_TYPE::INFERENCE:
            command = inference_workload();
            break;

        case WORKLOAD_TYPE::TRAINING:
            command = training_workload();
            break;

        case WORKLOAD_TYPE::BURST:
            command = burst_workload();
            break;

        case WORKLOAD_TYPE::CHECKPOINT:
            command = checkpoint_workload();
            break;

        default:
            break; 
    }

    current_command_count++;

    return command;
}

host_command_t workload_generator::random_workload()
{
    host_command_t command;

    command.type = static_cast<HOST_COMMAND_TYPE>(cmd_type_generator());
    command.LPA = logical_address_generator();

    if(command.type == HOST_COMMAND_TYPE::HOST_WRITE) command.data = double_words_data_generator();

    return command;
}

host_command_t workload_generator::inference_workload()
{
    // 90% Random Read (Hot Spot), 10% Random Write 

    host_command_t command;

    if(rand()%100 < 90) command.type = HOST_READ;
    else command.type = HOST_WRITE;

    if(rand()%100 < 80) command.LPA = rand() % HOT_SPOT;
    else command.LPA = rand() % MAX_LOGICAL_ADDRESS;

    if(command.type == HOST_WRITE) command.data = double_words_data_generator();

    return command;
}

host_command_t workload_generator::training_workload()
{
    // 60% Sequential Read, 30% Sequential Write, 10% Random Write
    host_command_t command;

    int p = rand() % 100;

    if(p < 60)
    {
        command.type = HOST_READ;
        command.LPA = training_read_address++;

        if(training_read_address == MAX_LOGICAL_ADDRESS) training_read_address = 0;
    }
    else if (p < 90)
    {
        command.type = HOST_WRITE;
        command.LPA = training_write_address++;
        command.data = double_words_data_generator();

        if(training_write_address == MAX_LOGICAL_ADDRESS) training_write_address = 0;
    }
    else
    {
        command.type = HOST_WRITE;
        command.LPA = logical_address_generator();
        command.data = double_words_data_generator();
    }

    return command;
}

host_command_t workload_generator::burst_workload()
{
    int burst_phase = (current_command_count / 256) % 2;

    host_command_t command;

    if(burst_phase == 0)
    {
        command.type = HOST_READ;
        command.LPA = logical_address_generator();
    }
    else
    {
        command.type = HOST_WRITE;
        command.LPA = sequential_address++;
        command.data = double_words_data_generator();

        if(sequential_address == MAX_LOGICAL_ADDRESS) sequential_address = 0;
    }

    return command;
}

host_command_t workload_generator::checkpoint_workload()
{
    host_command_t command;

    current_command_count++;

    if(current_command_count % 5000 == 0)
    {
        checkpoint_mode = true;
        checkpoint_remaining = 512;
    }

    if(checkpoint_mode)
    {
        command.type = HOST_WRITE;
        command.LPA = checkpoint_address++;
        command.data = double_words_data_generator();

        checkpoint_remaining--;

        if(checkpoint_remaining == 0) checkpoint_mode = false;

        if(checkpoint_address == MAX_LOGICAL_ADDRESS) checkpoint_address = 0;

        return command;
    }

    command = training_workload();

    return command;
}