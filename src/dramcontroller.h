#include "include/defs.h"
#include "include/command.h"
#include "include/transaction.h"
#include <fstream>

class dramcontroller {
public:
    dramcontroller(int);
    ~dramcontroller();

    void initialize();  // Load initial mapping table in way 0

    table_entry_t get_mapping_table_entry(lpa_t);

    void push_command_queue(host_command_t);
    host_command_t get_command();
    int get_command_queue_size();
	bool is_cmd_queue_empty();

    void push_transaction_queue(transaction_t);
    transaction_t get_transaction();
    transaction_t get_transaction_with_parameter(transaction_t*);
    int get_transaction_queue_size();
    bool is_transaction_queue_empty();

    void update_mapping_table(lpa_t, ppa_t, uint16_t);

    int get_write_buffer_size();
    int get_max_write_buffer_size();

    void write_to_buffer(lpa_t, unit_t);
    bool is_write_buffer_empty();
    buffer_entry_t get_front_buffer_entry();

	lpa_t get_lpa_from_mapping_table(ppa_t);

	bool is_gc_buffer_empty();
    void push_gc_buffer(lpa_t, pba_t, unit_t);
    garbage_collection_buffer_entry_t get_gc_buffer_entry();

    bool is_read_result_buffer_empty();
    void push_read_result_buffer(lpa_t, unit_t);
    buffer_entry_t get_read_result_buffer();

    void log_table_status();

private:
    std::queue<host_command_t> command_queue;
    std::queue<transaction_t> transaction_queue;

    std::queue<buffer_entry_t> write_buffer;
    std::queue<buffer_entry_t> read_result_buffer;
    std::queue<garbage_collection_buffer_entry_t> gc_buffer;

    table_entry_t *mapping_table;

    int max_buffer_size;
};

typedef dramcontroller dramcontroller_t;
