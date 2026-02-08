#include "./include/random.h"
#include "./include/defs.h"
#include <fstream>

class _flashcontroller {
public:
    _flashcontroller();
    ~_flashcontroller();

    void initialize();
    uint16_t get_block_address(ppa_t);
    uint16_t get_page_index(ppa_t);

    ppa_t find_free_page();
    void update_page_status(ppa_t, uint16_t);

    unit_t read_page(ppa_t);
    void program_page(ppa_t, unit_t);
    void erase_block(pba_t);

    uint16_t get_number_of_free_blocks();
    bool has_invalid_page();

    uint16_t find_free_block();
    uint16_t get_victim_block();

	void set_free_block_ptr();

    page_t get_page(pba_t, ppa_t);

	bool is_block_full(pba_t);

	double get_erase_variation();
    
	void show_valid_flash_pages();
	void log_flash_status();
	void log_flash_history();
	void log_gc(int);
private:
    block_t *nand;
    block_t *free_block_ptr;

    uint64_t erase_cnt;
	
	history_t *history_table;	
};

typedef _flashcontroller flashcontroller_t;

/* To-do List
1. Add Page Sensing function
*/
