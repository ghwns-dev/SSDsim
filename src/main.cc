#include "ssdcontroller.h"

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

void show_configuration()
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

    return;
}

cmd_t generate_command(int i_benchmark)
{
    cmd_t command;

    switch(i_benchmark)
    {
        case BENCHMARK_RANDOM:
            command.type = cmd_type_generator();
            break;
        case BENCHMARK_PROGRAM:
            command.type = CMD::PROGRAM;
            break;
        case BENCHMARK_READ:
            command.type = CMD::READ;
            break;
        default:
            break;
    }
   
    command.LPA = logical_address_generator();

	if(command.type == CMD::PROGRAM) command.data = double_words_data_generator();

    return command;
}

#define DEFAULT_BUFFER_SIZE 32
#define DEFAULT_ITERATION 8192


int main(int argc, char* argv[]){
   	 
    show_configuration();

    int benchmark;
	int max_data_buffer_size;
    int iter_cnt;

    if(argc == 4) {
        benchmark = parse_argv(argv, 1);
        max_data_buffer_size = parse_argv(argv, 2);
        iter_cnt = parse_argv(argv, 3);
    }
    else {
        max_data_buffer_size = DEFAULT_BUFFER_SIZE;
        iter_cnt = DEFAULT_ITERATION;
    }
	
	ssdcontroller_t *ftl = new ssdcontroller(max_data_buffer_size);
    ftl->initialize();

    // Simulator Start
    std::cout << "\n\n/*****SSDsim - simulation start*****/\n";
    
	for(int i = 0; i < iter_cnt; i++) {
        cmd_t cmd = generate_command(benchmark);
        ftl->push_command(cmd);
    }

	while(!ftl->is_cmd_queue_empty()){
        ftl->execute();
    }

    std::cout << "\n/*****SSDsim - simulation done******/\n";

    ftl->show_stats();

    delete ftl;
    return 0;
}

