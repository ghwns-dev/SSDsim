#include "include/defs.h"
#include "include/command.h"
#include "include/random.h"

enum WORKLOAD_TYPE
{
    RANDOM = 0,
    READ_HEAVY,
    WRITE_HEAVY,
    READ_WRITE_BURST
};

class workload_generator
{
public:
    workload_generator(int);
    ~workload_generator();

    host_command_t generate_command(WORKLOAD_TYPE i_type);
    HOST_COMMAND_TYPE heavy_read_generator();
    HOST_COMMAND_TYPE heavy_write_generator();
    HOST_COMMAND_TYPE burst_generator();

private:
    int total_command_count;
};

typedef workload_generator workload_generator_t;