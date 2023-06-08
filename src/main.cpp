#include <eosio/vm/backend.hpp>
#include <eosio/vm/error_codes.hpp>
#include <eosio/vm/host_function.hpp>
#include <eosio/vm/watchdog.hpp>

#include <iostream>
#include <string>
#include <fstream>
#include <iterator>

#include "static_analysis.hpp"

using namespace eosio;
using namespace eosio::vm;

// needed because CDT is stupid
template <typename T, std::size_t Align = alignof(T)>
using legacy_span = eosio::vm::argument_proxy<eosio::vm::span<T>, Align>;

struct base_host_functions {
   void set_blockchain_parameters_packed(legacy_span<const char>) {}
   uint32_t get_blockchain_parameters_packed(legacy_span<char>) { return 0; }
   int64_t set_proposed_producers(legacy_span<const char>) { return 0; }
   int32_t get_active_producers(legacy_span<uint64_t>) { return 0; }
   uint64_t current_time() { return 0; }
   void prints_l(legacy_span<const char> s) { std::cout << std::string(s.data(), s.size()); }
   void eosio_assert(bool test, const char* msg) {
      if (!test) {
         std::cout << msg << std::endl;
         throw 0;
      }
   }

};

struct host_functions : public base_host_functions {
   void print_hello() {
      std::cout << "Hello, world!" << std::endl;
   }

   void print_number(int32_t n) {
      std::cout << "Number: " << n << std::endl;
   }
};

struct cnv : type_converter<host_functions> {
   using type_converter::type_converter;
   using type_converter::from_wasm;
   EOS_VM_FROM_WASM(bool, (uint32_t value)) { return value ? 1 : 0; }
   EOS_VM_FROM_WASM(char*, (void* ptr)) { return static_cast<char*>(ptr); }
   EOS_VM_FROM_WASM(const char*, (void* ptr)) { return static_cast<char*>(ptr); }
};

int main(int argc, char** argv) {
   wasm_allocator wa;
   using rhf_t = eosio::vm::registered_host_functions<host_functions, execution_interface, cnv>;
   using backend_t = eosio::vm::backend<rhf_t>;

   rhf_t::add<&host_functions::print_hello>("env", "print_hello");
   rhf_t::add<&host_functions::print_number>("env", "print_number");

   // once again needed because of CDT
   rhf_t::add<&host_functions::eosio_assert>("env", "eosio_assert");
   rhf_t::add<&host_functions::current_time>("env", "current_time");
   rhf_t::add<&host_functions::set_blockchain_parameters_packed>("env", "set_blockchain_parameters_packed");
   rhf_t::add<&host_functions::get_blockchain_parameters_packed>("env", "get_blockchain_parameters_packed");
   rhf_t::add<&host_functions::set_proposed_producers>("env", "set_proposed_producers");
   rhf_t::add<&host_functions::get_active_producers>("env", "get_active_producers");
   rhf_t::add<&host_functions::prints_l>("env", "prints_l");

   if (argc < 2) {
      std::cerr << "Please provide a wasm file." << std::endl;
      return -1;
   }

   std::vector<uint8_t> wasm_bytes;

   std::ifstream wasm_file(argv[1], std::ios::binary);
   if (!wasm_file) {
      std::cerr << "Unable to open wasm file." << std::endl;
      return -1;
   }

   wasm_file.seekg(0, std::ios::end);
   std::size_t sz = wasm_file.tellg();
   wasm_file.seekg(0, std::ios::beg);

   wasm_bytes.resize(sz);
   wasm_file.read((char*)wasm_bytes.data(), sz);

   try {
      host_functions hf;
      backend_t bkend(wasm_bytes, hf, &wa);

      bkend(hf, "env", "apply", (uint64_t)0, (uint64_t)0, (uint64_t)0);

      // static analysis
      static_analysis sa;
      for(std::size_t i=0; i < bkend.get_module().code.size(); ++i) {
         const auto& body = bkend.get_module().code[i];

         for (std::size_t j=0; j < body.size; ++j) {
            visit(sa, body.code[j]);
         }
      }
      std::cout << "How many found ops: " << sa.get_count() << std::endl;
   } catch (const eosio::vm::exception& ex) {
      std::cerr << ex.detail() << std::endl;
      return -1;
   }

   return 0;
}
