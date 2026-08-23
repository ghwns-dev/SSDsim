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

void show_configuration(WORKLOAD_TYPE i_workload_type, SCHEDULING_POLICY i_scheduling_policy)
{
    static std::string banner = "\
***********************************************************\n\
* SSDsim: Simulator for a simple SSD Architecture         *\n\
* Developed by Hojun Kim                                  *\n\
* EEE, Yonsei University                                  *\n\
* Version: 2.2                                            *\n\
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
            std::cout << "\n\n* workload type : random workload";
            break;

        case WORKLOAD_TYPE::INFERENCE:
            std::cout << "\n\n* workload type : inference workload";
            break;

        case WORKLOAD_TYPE::TRAINING:
            std::cout << "\n\n* workload type : training workload";
            break;

        case WORKLOAD_TYPE::BURST:
            std::cout << "\n\n* workload type : burst workload";
            break;

        case WORKLOAD_TYPE::CHECKPOINT:
            std::cout << "\n\n* workload type : checkpoint workload";
            break;

        default:
            break;
    }

        switch(i_scheduling_policy)
    {
        case SCHEDULING_POLICY::FCFS:
            std::cout << "\n\n* scheduling policy : First-Come-First-Served";
            break;

        case SCHEDULING_POLICY::READ_FIRST:
            std::cout << "\n\n* scheduling policy : Read-Priority";
            break;

        default:
            break;
    }

    return;
}

#define DEFAULT_BUFFER_SIZE 32
#define DEFAULT_ITERATION 8192
#define DEFAULT_RANDOM_WORKLOAD 0
#define DEFAULT_SCHEDULING_POLICY_FCFS 0

int main(int argc, char* argv[]){
    srand(42);
 
    WORKLOAD_TYPE workload_type;
	int max_data_buffer_size;
    int iteration_count;
    SCHEDULING_POLICY scheduling_policy;
 
    if(argc == 5) {
        workload_type = static_cast<WORKLOAD_TYPE>(parse_argv(argv, 1));
        max_data_buffer_size = parse_argv(argv, 2);
        iteration_count = parse_argv(argv, 3);
        scheduling_policy = static_cast<SCHEDULING_POLICY>(parse_argv(argv, 4));
    }
    else {
        workload_type = static_cast<WORKLOAD_TYPE>(DEFAULT_RANDOM_WORKLOAD);
        max_data_buffer_size = DEFAULT_BUFFER_SIZE;
        iteration_count = DEFAULT_ITERATION;
        scheduling_policy = static_cast<SCHEDULING_POLICY>(DEFAULT_SCHEDULING_POLICY_FCFS);
    }
 
    show_configuration(workload_type, scheduling_policy);
 
    // [FIX 2026/08/23] workload_generator_t is now heap-allocated behind a
    // pointer instead of a stack object -- no functional reason once this
    // was just one constructor call again, but ~workload_generator() is
    // trivial and this keeps the explicit delete symmetric with ftl below.
    workload_generator_t *_workload = new workload_generator_t(workload_type, iteration_count);
 
    // [FIX 2026/08/23] WORKLOAD_TYPE now resolves to a trace file inside
    // workload_generator (see resolve_trace_path()) instead of a synthetic
    // pattern -- -workload=/-iteration= parsing above is unchanged.
    std::cout << "\n* trace entries loaded : " << _workload->get_total_command_count() << "\n";
 
	ssdcontroller_t *ftl = new ssdcontroller(max_data_buffer_size, scheduling_policy);
    ftl->initialize();
 
    // Simulator Start
    std::cout << "\n\n/*****SSDsim - simulation start*****/\n";
 
    // [FIX 2026/08/23] Previously the entire workload was pushed into the
    // command queue before the simulation loop even started (a plain
    // for-loop over iteration_count), so every run was fully backlogged
    // from tick 0 and total_cycles ended up dominated by raw throughput
    // rather than scheduling order (see the FCFS vs Read-First discussion).
    // Now that every WORKLOAD_TYPE is trace-driven (see
    // workload_generator::resolve_trace_path()), a command is only pushed
    // once its recorded arrival tick has actually been reached, so the
    // request-arrival pattern recorded in the trace is what actually paces
    // the simulation instead of everything landing at tick 0.
    while(!_workload->is_finished() || !ftl->is_idle())
    {
        while(!_workload->is_finished() && _workload->get_next_arrival_tick() <= get_ticks()) {
            host_command_t cmd = _workload->generate_command();
            ftl->push_command(cmd);
        }
 
        // [FIX 2026/08/23] get_ticks() only advances as a side effect of
        // actually processing a command/transaction -- there's no free-
        // running clock. If the FTL has drained everything it currently has
        // but the workload isn't finished, nothing will ever make
        // get_ticks() reach the next arrival on its own, so the simulated
        // clock has to be fast-forwarded explicitly to that next arrival
        // (this models the SSD sitting idle, powered on, waiting for the
        // next request). channel_busy[] values aren't touched by this --
        // they're absolute future ticks already recorded, and
        // max(get_ticks(), channel_busy[channel]) stays correct however far
        // get_ticks() jumps.
        if(ftl->is_idle() && !_workload->is_finished()) {
            uint64_t next_tick = _workload->get_next_arrival_tick();
            if(next_tick > get_ticks()) count_ticks(next_tick - get_ticks());
            continue;
        }
 
        ftl->execute();
    }
 
    std::cout << "\n/*****SSDsim - simulation done******/\n";
 
    ftl->show_stats();
 
    delete ftl;
    delete _workload;
    return 0;
}
