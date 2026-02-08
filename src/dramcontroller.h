#include "./include/defs.h"
#include <fstream>

class _dramcontroller {
public:
    _dramcontroller(int);
    ~_dramcontroller();

    void initialize();  // Load initial mapping table in way 0

    table_entry_t get_mapping_table_entry(lpa_t);

    void push_command_queue(cmd_t);
    cmd_t get_command();
    int get_command_queue_size();
	bool is_cmd_queue_empty();

    void update_mapping_table(lpa_t, ppa_t, uint16_t);

    int get_write_buffer_size();
    int get_max_write_buffer_size();

    void write_to_buffer(lpa_t, unit_t);
    bool is_write_buffer_empty();
    buffer_entry_t get_front_buffer_entry();

	lpa_t get_lpa_from_mapping_table(ppa_t);

	bool is_copy_data_buffer_empty();
    void push_copy_data_buffer(unit_t);
    unit_t get_copy_data_buffer();

    void log_table_status();

private:
    std::queue<cmd_t> command_queue;
    std::queue<buffer_entry_t> write_buffer;
    std::queue<unit_t> copy_data_buffer;

    table_entry_t *mapping_table;

    int max_buffer_size;
};

typedef _dramcontroller dramcontroller_t;
