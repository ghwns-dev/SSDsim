#include "workload_generator.h"
#include <fstream>
#include <sstream>
#include <climits>

// [FIX 2026/08/23] WORKLOAD_TYPE now selects a trace file (see
// resolve_trace_path()) instead of seeding a synthetic pattern -- the
// signature and every existing call site (main.cc's -workload=/-iteration=
// parsing) are unchanged.
workload_generator::workload_generator(WORKLOAD_TYPE i_type, int i_cmd_cnt)
{
    workload_type = i_type;
    current_command_count = 0;

    // vestigial -- only the now-unused synthetic *_workload() functions
    // read these; initialized so nothing is left undefined.
    sequential_address = 0;
    training_read_address = 0;
    training_write_address = MAX_LOGICAL_ADDRESS / 2;
    checkpoint_address = 0;
    checkpoint_mode = false;
    checkpoint_remaining = 0;

    trace_cursor = 0;
    load_trace(resolve_trace_path(i_type));

    // [FIX 2026/08/23] i_cmd_cnt (originally -iteration=, "how many
    // synthetic commands to generate") now caps how many of the trace's
    // entries actually get used, taken in trace order -- it can never
    // exceed what the file actually has.
    int trace_size = static_cast<int>(trace_entries.size());
    total_command_count = std::min(i_cmd_cnt, trace_size);

    if(i_cmd_cnt > trace_size)
    {
        std::cout << "\n[workload_generator] warning: -iteration=" << i_cmd_cnt
                   << " exceeds trace length (" << trace_size
                   << "); using " << total_command_count << " entries instead.\n";
    }
}

// [FIX 2026/08/23] Maps a WORKLOAD_TYPE to its trace file. All five
// workload types are driven by a matching offline-generated trace file
// under SSDsim_wsl/trace/ (tick,R/W,lpa -- see the trace-file generation
// discussion) instead of the synthetic *_workload() generators further
// below, which are kept only for reference.
std::string workload_generator::resolve_trace_path(WORKLOAD_TYPE i_type)
{
    switch(i_type)
    {
        case WORKLOAD_TYPE::RANDOM:     return "trace/random.trace";
        case WORKLOAD_TYPE::INFERENCE:  return "trace/inference.trace";
        case WORKLOAD_TYPE::TRAINING:   return "trace/training.trace";
        case WORKLOAD_TYPE::BURST:      return "trace/burst.trace";
        case WORKLOAD_TYPE::CHECKPOINT: return "trace/checkpoint.trace";
        default:                        return "";
    }
}

workload_generator::~workload_generator()
{

}

// [FIX 2026/08/23] Parses "tick,R/W,lpa" lines into trace_entries. Write
// commands get a random data payload synthesized here -- the trace format
// intentionally doesn't carry payload data (SSDsim never reads it back for
// correctness, only stores it in page_t::data), matching how the synthetic
// *_workload() functions already fill write data via
// double_words_data_generator().
void workload_generator::load_trace(std::string i_trace_path)
{
    std::ifstream trace_file(i_trace_path);

    if(!trace_file.is_open())
    {
        std::cout << "\nfailed to open trace file: " << i_trace_path << "\n";
        exit(-1);
    }

    std::string line;

    while(std::getline(trace_file, line))
    {
        if(line.empty()) continue;

        std::stringstream ss(line);
        std::string tick_str, rw_str, lpa_str;

        std::getline(ss, tick_str, ',');
        std::getline(ss, rw_str, ',');
        std::getline(ss, lpa_str, ',');

        trace_entry_t entry;
        entry.tick = std::stoull(tick_str);
        entry.cmd.type = (rw_str == "R") ? HOST_READ : HOST_WRITE;
        entry.cmd.LPA = static_cast<lpa_t>(std::stoul(lpa_str));

        if(entry.cmd.type == HOST_WRITE) entry.cmd.data = double_words_data_generator();

        trace_entries.push_back(entry);
    }

    trace_file.close();

    if(trace_entries.empty())
    {
        std::cout << "\ntrace file has no entries: " << i_trace_path << "\n";
        exit(-1);
    }
}

// [FIX 2026/08/23] Every WORKLOAD_TYPE is trace-driven now -- the switch is
// kept (rather than calling trace_workload() unconditionally) so it stays
// obvious at a glance which types are actually supported.
host_command_t workload_generator::generate_command()
{
    host_command_t command;

    switch(workload_type)
    {
        case WORKLOAD_TYPE::RANDOM:
        case WORKLOAD_TYPE::INFERENCE:
        case WORKLOAD_TYPE::TRAINING:
        case WORKLOAD_TYPE::BURST:
        case WORKLOAD_TYPE::CHECKPOINT:
            command = trace_workload();
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

// [FIX 2026/08/23] Returns the next trace entry's command in order and
// advances the cursor. Like the other *_workload() functions, this doesn't
// guard against being called past the end of its data -- callers must check
// is_finished() first.
host_command_t workload_generator::trace_workload()
{
    host_command_t command = trace_entries[trace_cursor].cmd;
    trace_cursor++;

    return command;
}

// [FIX 2026/08/23] Arrival tick of the next not-yet-generated command.
// main.cc uses this to decide whether it's time to push the next command
// yet, instead of pushing the whole workload up front (which is what made
// every run fully saturated from tick 0, regardless of scheduling policy --
// see the FCFS vs Read-First discussion). Once the trace is exhausted,
// returns ULLONG_MAX so a caller that (incorrectly) checks this before
// is_finished() never treats "no more entries" as "ready now".
uint64_t workload_generator::get_next_arrival_tick()
{
    if(trace_cursor >= static_cast<int>(trace_entries.size())) return ULLONG_MAX;

    return trace_entries[trace_cursor].tick;
}

// [FIX 2026/08/23] True once total_command_count commands have been
// generated (mirrors the current_command_count bookkeeping generate_command()
// already does for every workload type).
bool workload_generator::is_finished()
{
    return current_command_count >= total_command_count;
}

// [FIX 2026/08/23] For TRACE mode this is the number of lines actually
// parsed from the trace file (not a caller-supplied guess), so main.cc can
// drive its loop bound off the real trace length.
int workload_generator::get_total_command_count()
{
    return total_command_count;
}