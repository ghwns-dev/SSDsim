#include "./microprocessor.h"

int parse_argv(char* argv_[], int index){
    std::string str = argv_[index];
    char target = '=';

    int start = str.find(target);

    std::string num_str = str.substr(start+1);

    int ret = stoi(num_str);
    return ret;
}

#define DEFAULT_BUFFER 32
#define DEFAULT_ITERATION 8192

static std::string banner = "\
***********************************************************\n\
* SSDsim: Simulator for a simple SSD Architecture         *\n\
* Developed by Hojun Kim                                  *\n\
* EEE, Yonsei University                                  *\n\
* Version: 1.9                                            *\n\
***********************************************************\n\
";

int main(int argc, char* argv[]){
   	
	std::cout << banner << '\n';
	 
	int max_data_buffer_size;
    int iter_cnt;

    if(argc > 1) {
        max_data_buffer_size = parse_argv(argv, 1);
        iter_cnt = parse_argv(argv, 2);
    }
    else {
        max_data_buffer_size = DEFAULT_BUFFER;
        iter_cnt = DEFAULT_ITERATION;
    }
	std::cout << "* SSD configuration for SSDsim\n";
	std::cout << "\n* SSD size : " << TOTAL_SSD_SIZE << " bytes";
	std::cout << "\n* page size : " << PAGE_SIZE << " bytes";
	std::cout << "\n* max logical address : " << MAX_LOGICAL_ADDRESS; 
	std::cout << "\n* max page address : " << MAX_PAGE_ADDRESS;		
	std::cout << "\n* number of pages per block : " << PAGE_PER_BLOCK;
	std::cout << "\n* number of blocks per nand : " << BLOCK_PER_NAND;

	microprocessor_t *ftl = new microprocessor(max_data_buffer_size);
    ftl->initialize();

    // Simulator Start
    std::cout << "\n\n/*****SSDsim - simulation start*****/\n";
    
	for(int i = 0; i < iter_cnt; i++) {
        cmd_t cmd;

        cmd.type = cmd_type_generator();
        cmd.LPA = logical_address_generator();
        // cmd.LPA = 64;
		if(cmd.type == CMD::PROGRAM) cmd.data = double_words_data_generator();

        ftl->push_command(cmd);
    }

	while(!ftl->is_cmd_queue_empty()){
        ftl->execute();
    }

    std::cout << "\n\n/*****SSDsim - simulation done******/\n";

    ftl->show_stats();

    delete ftl;
    return 0;
}

