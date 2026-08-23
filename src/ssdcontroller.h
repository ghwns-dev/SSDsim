#include "dramcontroller.h"
#include "flashcontroller.h"

#define FREE_BLOCK_TH 2
#define ERASE_PERIOD 100
#define TRANSACTION_FLUSH_TH 16

class ssdcontroller {
public:
        ssdcontroller(int, SCHEDULING_POLICY);
        ~ssdcontroller();

        /***v1.3 Revision***/

        void initialize();

        void push_command(host_command_t);
        host_command_t get_command();
		bool is_cmd_queue_empty();

        void push_read_transaction(ppa_t);
        void push_program_transaction(ppa_t, unit_t);
        void push_erase_transaction(pba_t);

        table_entry_t get_mapping_table_entry(lpa_t);

        bool is_idle();

        bool execute();
        bool host_read(lpa_t, unit_t*);
        bool host_write(lpa_t, unit_t);

        bool schedule_transaction();
        transaction_t select_transaction();
        transaction_t find_least_recent_transaction(transaction_t *, int);

        bool garbage_collection_triggered();
        void garbage_collection();

        void write_to_buffer(lpa_t, unit_t);
        void write_to_nand(ppa_t, unit_t);

        void show_valid_flash_pages();
		void log_flash_status();
        void log_table_status();
        void show_execution_result();
		void show_stats();
private:
        dramcontroller_t *dram_controller;
        flashcontroller_t *flash_controller;

        history_t *history_table;        // index : PPA

		uint64_t failed_command;
		uint64_t succeed_command;

		// [FIX 2026/08/23] Per-request latency stats (queue wait + NAND op
		// time = completion_tick - submit_tick). total_cycles is a
		// per-channel sum and is structurally blind to processing order,
		// so this is what actually reveals FCFS vs Read-First scheduling
		// differences.
		uint64_t total_read_latency;
		uint64_t read_latency_count;
		uint64_t max_read_latency;

		uint64_t total_program_latency;
		uint64_t program_latency_count;
		uint64_t max_program_latency;

        SCHEDULING_POLICY m_scheduling_policy;
};

typedef ssdcontroller ssdcontroller_t;