// Code used to generate this wasm
// compile with eosio-cpp -o hello.wasm hello.cpp
#include <eosio/eosio.hpp>
#include <eosio/print.hpp>
#include <cstring>
#include <cstdint>

extern "C" {

   [[eosio::wasm_import]] void print_string(const char*);

   void apply(uint64_t a, uint64_t b, uint64_t c) {
       const char* test_str = "Test string printed\n";
       std::size_t length = std::strlen(test_str);
       print_string(test_str);
   }
}

/*
extern "C" {
   [[eosio::wasm_import]] void print_name(const char*);
   [[eosio::wasm_import]] void print_num(uint64_t);
   [[eosio::wasm_import]] void print_span(const char*, std::size_t);

   void apply(uint64_t a, uint64_t b, uint64_t c) {
    const char* test_str = "hellohellohello";
    print_num(a);
    print_num(b);
    print_num(c);
    eosio::check(b == c, "Failure B != C");
    for (uint64_t i = 0; i < a; i++)
        print_name("eos-vm");
    print_span(test_str, 5);
    print_span(test_str, 10);
    print_name("eos-vm");
   }
}
*/
