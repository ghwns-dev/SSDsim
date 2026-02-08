#include "random.h"

uint8_t cmd_type_generator(){
 	std::random_device rd;                          // �~U~X�~S~\�~[��~V� �~B~\�~H~X
    std::mt19937 gen(rd());                         // Mersenne Twister �~W~T�~D
    std::uniform_int_distribution<uint32_t> dist(0, 0x1);  // 32bit �~T�~\~D

    uint8_t random_type = dist(gen);
    random_type = random_type & 0b1;
    return random_type;
}

uint16_t logical_address_generator(){
    std::random_device rd;                          // 하드웨어 난수
    std::mt19937 gen(rd());                         // Mersenne Twister 엔진
    std::uniform_int_distribution<uint16_t> dist(0, 0b1111111111);  // 32bit 범위
    
    uint16_t random_addr = dist(gen);
	random_addr = random_addr & 0b1111111111;
    return random_addr;
}

uint64_t double_words_data_generator(){
    std::random_device rd;                          // 하드웨어 난수
    std::mt19937 gen(rd());                         // Mersenne Twister 엔진
    std::uniform_int_distribution<uint32_t> dist(1, 0xFFF);  // 32bit 범위
    
    uint64_t random_data = dist(gen) + 1;
    return random_data;
}
