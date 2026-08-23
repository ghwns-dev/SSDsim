#include "defs.h"
#include "command.h"
#include "random.h"
#include "ctime"
#include <string>
#include <vector>

enum WORKLOAD_TYPE
{
    RANDOM = 0,
    INFERENCE,
    TRAINING,
    BURST,
    CHECKPOINT
};

#define HOT_SPOT 256

// [FIX 2026/08/23] One parsed line of a "tick,R/W,lpa" trace file -- tick is
// the request's real arrival time, so main.cc can gate pushing it into the
// command queue until that tick is reached instead of pushing the whole
// workload up front. See load_trace().
typedef struct trace_entry {
    uint64_t tick;
    host_command_t cmd;
} trace_entry_t;

class workload_generator
{
public:
    // [FIX 2026/08/23] WORKLOAD_TYPE now determines which trace file gets
    // loaded (see resolve_trace_path()/load_trace()) -- the constructor
    // signature and call sites (main.cc's -workload=/-iteration= parsing)
    // are unchanged, only what happens inside changed. i_cmd_cnt caps how
    // many of the trace's entries are used (see the .cc for details).
    workload_generator(WORKLOAD_TYPE, int);
    ~workload_generator();

    host_command_t generate_command();

    // [FIX 2026/08/23] No longer called from generate_command() -- every
    // WORKLOAD_TYPE is trace-driven now (see trace_workload()). Left in
    // place as the reference implementation the trace files were modeled
    // on, in case synthetic generation is ever needed again.
    host_command_t random_workload();
    host_command_t inference_workload();        // 80% read, 20% write, sequential read
    host_command_t training_workload();        // 80% write, 20% read, sequential write
    host_command_t burst_workload();            // 25% read -> 25% wrtie -> 25% read -> 25% write
    host_command_t checkpoint_workload();

    // [FIX 2026/08/23] get_next_arrival_tick() is what lets main.cc's loop
    // decide whether it's time to push the next command yet, instead of
    // dumping the entire workload into the command queue before the
    // simulation starts (which is what made every workload run fully
    // saturated from tick 0, regardless of scheduling policy -- see the
    // FCFS vs Read-First discussion).
    uint64_t get_next_arrival_tick();
    bool is_finished();
    int get_total_command_count();

private:
    WORKLOAD_TYPE workload_type;

    int total_command_count;
    int current_command_count;

    // [FIX 2026/08/23] vestigial -- only used by the now-unused synthetic
    // *_workload() functions above, kept initialized so nothing is left
    // undefined.
    lpa_t sequential_address;
    lpa_t training_read_address;
    lpa_t training_write_address;
    lpa_t checkpoint_address;

    bool checkpoint_mode;
    int checkpoint_remaining;

    // [FIX 2026/08/23] trace-mode state
    std::vector<trace_entry_t> trace_entries;
    int trace_cursor;
    std::string resolve_trace_path(WORKLOAD_TYPE);
    void load_trace(std::string i_trace_path);
    host_command_t trace_workload();
};

typedef workload_generator workload_generator_t;