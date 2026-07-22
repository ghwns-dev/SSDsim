#include "defs.h"
#include "command.h"
#include "random.h"
#include "ctime"

enum WORKLOAD_TYPE
{
    RANDOM = 0,
    INFERENCE,
    TRAINING,
    BURST,
    CHECKPOINT
};

#define HOT_SPOT 256

class workload_generator
{
public:
    workload_generator(WORKLOAD_TYPE, int);
    ~workload_generator();

    host_command_t generate_command();
    host_command_t random_workload();
    host_command_t inference_workload();        // 80% read, 20% write, sequential read
    host_command_t training_workload();        // 80% write, 20% read, sequential write
    host_command_t burst_workload();            // 25% read -> 25% wrtie -> 25% read -> 25% write
    host_command_t checkpoint_workload();

private:
    WORKLOAD_TYPE workload_type;
    
    int total_command_count;
    int current_command_count;

    lpa_t sequential_address;
    lpa_t training_read_address;
    lpa_t training_write_address;
    lpa_t checkpoint_address;

    bool checkpoint_mode;
    int checkpoint_remaining;
};

typedef workload_generator workload_generator_t;