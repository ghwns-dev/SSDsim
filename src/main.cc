#include "ssdcontroller.h"
#include "include/workload_generator.h"

#define BENCHMARK_RANDOM 0
#define BENCHMARK_PROGRAM 1
#define BENCHMARK_READ 2

int parse_argv(char* argv_[], int index){
    std::string str = argv_[index];
    char target = '=';

    int start = str.find(target);

    std::string num_str = str.substr(start+1);

    int ret = stoi(num_str);
    return ret;
}

void show_configuration(int i_workload_type)
{
    static std::string banner = "\
***********************************************************\n\
* SSDsim: Simulator for a simple SSD Architecture         *\n\
* Developed by Hojun Kim                                  *\n\
* EEE, Yonsei University                                  *\n\
* Version: 2.0                                            *\n\
***********************************************************\n\
";

    std::cout << banner << '\n';

    std::cout << "* SSD configuration for SSDsim\n";
	std::cout << "\n* SSD size : " << TOTAL_SSD_SIZE << " bytes";
	std::cout << "\n* page size : " << PAGE_SIZE << " bytes";
	std::cout << "\n* max logical address : " << MAX_LOGICAL_ADDRESS; 
	std::cout << "\n* max page address : " << MAX_PAGE_ADDRESS;		
	std::cout << "\n* number of pages per block : " << PAGE_PER_BLOCK;
	std::cout << "\n* number of blocks per nand : " << BLOCK_PER_NAND;

    switch(i_workload_type)
    {
        case WORKLOAD_TYPE::RANDOM:
            std::cout << "\n\n* random workload";
            break;

        case WORKLOAD_TYPE::INFERENCE:
            std::cout << "\n\n* inference workload";
            break;

        case WORKLOAD_TYPE::TRAINING:
            std::cout << "\n\n* training workload";
            break;

        case WORKLOAD_TYPE::BURST:
            std::cout << "\n\n* burst workload";
            break;

        case WORKLOAD_TYPE::CHECKPOINT:
            std::cout << "\n\n* checkpoint workload";
            break;

        default:
            break;
    }

    return;
}

#define DEFAULT_BUFFER_SIZE 32
#define DEFAULT_ITERATION 8192
#define DEFAULT_RANDOM_WORKLOAD 0

int main(int argc, char* argv[]){
    srand(42);

    int workload_type;
	int max_data_buffer_size;
    int iteration_count;

    if(argc == 4) {
        workload_type = parse_argv(argv, 1);
        max_data_buffer_size = parse_argv(argv, 2);
        iteration_count = parse_argv(argv, 3);
    }
    else {
        workload_type = DEFAULT_RANDOM_WORKLOAD;
        max_data_buffer_size = DEFAULT_BUFFER_SIZE;
        iteration_count = DEFAULT_ITERATION;
    }

    show_configuration(workload_type);
	
	ssdcontroller_t *ftl = new ssdcontroller(max_data_buffer_size);
    ftl->initialize();

    workload_generator_t _workload = workload_generator(WORKLOAD_TYPE(workload_type), iteration_count);

    // Simulator Start
    std::cout << "\n\n/*****SSDsim - simulation start*****/\n";
    
	for(int i = 0; i < iteration_count; i++) {
        host_command_t cmd = _workload.generate_command();
        ftl->push_command(cmd);
    }

    while(!ftl->is_idle())
    {
        // std::cout << "main loop\n";
        ftl->execute();
    }

    std::cout << "\n/*****SSDsim - simulation done******/\n";

    ftl->show_stats();

    delete ftl;
    return 0;
}

