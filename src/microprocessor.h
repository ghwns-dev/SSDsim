#include "dramcontroller.h"
#include "flashcontroller.h"

#define FREE_BLOCK_TH 2
#define ERASE_PERIOD 100

class microprocessor {
public:
        microprocessor(int);
        ~microprocessor();

        /***v1.3 Revision***/

        void initialize();

        void push_command(cmd_t);
        cmd_t get_command();
		bool is_cmd_queue_empty();

        table_entry_t get_mapping_table_entry(lpa_t);

        bool execute();
        bool read(lpa_t, unit_t*);
        bool program(lpa_t, unit_t);

        bool garbage_collection_triggered();
        void garbage_collection();

        void write_to_buffer(lpa_t, unit_t);
        void write_to_nand(ppa_t, unit_t);

        void show_valid_flash_pages();
		void log_flash_status();
        void log_flash_history();
        void log_table_status();
        void show_execution_result();
		void show_stats();
private:
        dramcontroller_t *dramcontroller;
        flashcontroller_t *flashcontroller;

        history_t *history_table;        // index : PPA

		uint64_t failed_command;
		uint64_t succeed_command;
};

typedef microprocessor microprocessor_t;

