#include <eosio/vm/backend.hpp>
#include <eosio/vm/error_codes.hpp>
#include <eosio/vm/host_function.hpp>
#include <eosio/vm/watchdog.hpp>

#include <iostream>
#include <string>

using namespace eosio;
using namespace eosio::vm;
using namespace std;



template <typename T, std::size_t Align = alignof(T)>
using legacy_span = eosio::vm::argument_proxy<eosio::vm::span<T>, Align>;

struct example_host_methods {
   void print_num(uint64_t n) { std::cout << "Number : " << n << "\n"; }
   // example of a host "method"
   void print_name(const char* nm) { std::cout << "Name : " << nm << " " << field << "\n"; }
   // example of another type of host function
   void set_blockchain_parameters_packed(legacy_span<const char>) {}
   uint32_t get_blockchain_parameters_packed(legacy_span<char>) { return 0; }
   int64_t set_proposed_producers(legacy_span<const char>) { return 0; }
   int32_t get_active_producers(legacy_span<uint64_t>) { return 0; }
   uint64_t current_time() { return 0; }
   void prints_l(legacy_span<const char> s) { std::cout << std::string(s.data(), s.size()); }
   std::string  field = "";

   void eosio_assert(bool test, const char* msg) {
      if (!test) {
         std::cout << msg << std::endl;
         throw 0;
      }
   }

   void print_span(span<const char> s) {
      std::cout << "Span : " << std::string{s.data(), s.size()} << "\n";
   }

   void print_string(const char* s) {
      std::cout << "print_string() was called\n" << s << endl;
   }
};

struct cnv : type_converter<example_host_methods> {
   using type_converter::type_converter;
   using type_converter::from_wasm;
   EOS_VM_FROM_WASM(bool, (uint32_t value)) { return value ? 1 : 0; }
   EOS_VM_FROM_WASM(char*, (void* ptr)) { return static_cast<char*>(ptr); }
   EOS_VM_FROM_WASM(const char*, (void* ptr)) { return static_cast<char*>(ptr); }
};

EOS_VM_PRECONDITION(test_name,
      EOS_VM_INVOKE_ON(const char*, [&](auto&& nm, auto&&... rest) {
         std::string s = nm;
         if (s == "eos-vm2")
            throw "failure";
   }))

eosio::vm::guarded_vector<uint8_t>* find_export_name(const module& mod, uint32_t idx) {
    if(mod.names && mod.names->function_names) {
        for(uint32_t i = 0; i < mod.names->function_names->size(); ++i) {
            auto test = (*mod.names->function_names)[i].idx;
            if((*mod.names->function_names)[i].idx == idx) {
                return &(*mod.names->function_names)[i].name;
            }
        }
    }
    for(uint32_t i = 0; i < mod.exports.size(); ++i) {
        if(mod.exports[i].index == idx && mod.exports[i].kind == eosio::vm::Function) {
            return &mod.exports[i].field_str;
        }
    }
    return nullptr;
}


std::unordered_map<int, std::string> function_names;
// std::vector<std::string> function_names;

struct stats_t {
   // key opcode, value count
   std::unordered_map<int, int> ops_count;
   // key index, value count (index of the function in function table)
   std::unordered_map<int, int> calls_count;

   void print(const module & mod) {
       opcode_utils u;
       cout << "\nStatistics of operations: \n";
       for(auto i: ops_count) {
            cout << "operation: " << u.opcode_map[i.first] << " opcode: " << i.first << " count: " << i.second << endl;
       }
       cout << "\nStatistics of function calls: \n";
       for(auto i: calls_count) {
           cout << "function index: " << i.first << " name: " << function_names[i.first] << " count: " << i.second << endl;
       }
   }
} stats;


#define DBG_CALL_VISIT(name, code)                                                                                     \
   void operator()(const EOS_VM_OPCODE_T(name)& op) {                                                                  \
      call_t call;                                                                                                     \
      std::cout << "Found " << #name << " at " << get_context().get_pc() << "\n";                                      \
      interpret_visitor<ExecutionCTX>::operator()(op);                                                                 \
      get_context().print_stack();                                                                                     \
      stats.ops_count[op.opcode] ++;                                                                                   \
      if(op.opcode == call.opcode)  {                                                                                  \
        curr_func = op.index;                                                                                          \
        stats.calls_count[op.index] ++;                                                                                \
      }                                                                                                                \
   }


#define DBG_VISIT(name, code)                                                                                          \
   void operator()(const EOS_VM_OPCODE_T(name)& op) {                                                                  \
      std::cout << "Found " << #name << " at " << get_context().get_pc() << "\n";                                      \
      interpret_visitor<ExecutionCTX>::operator()(op);                                                                 \
      get_context().print_stack();                                                                                     \
      stats.ops_count[op.opcode] ++;                                                                                   \
   }

// TODO: Indercept the ret command and recognize if the function use a tail call

#define DBG_RET_VISIT(name, code)                                                                                      \
   void operator()(const EOS_VM_OPCODE_T(name)& op) { std::cout << "Found " << #name << "\n"; }

template <typename ExecutionCTX>
struct my_visitor : public interpret_visitor<ExecutionCTX> {
   using interpret_visitor<ExecutionCTX>::operator();
   uint32_t curr_func = 0;
   my_visitor(ExecutionCTX& ctx) : interpret_visitor<ExecutionCTX>(ctx) {}
   ExecutionCTX& get_context() { return interpret_visitor<ExecutionCTX>::get_context(); }
   EOS_VM_CALL_OPS(DBG_CALL_VISIT)
   EOS_VM_MEMORY_OPS(DBG_VISIT)
//   EOS_VM_RETURN_OP(DBG_RET_VISIT)
};

template <typename HostFunctions = std::nullptr_t, typename Impl = interpreter, typename Options = default_options, typename DebugInfo = null_debug_info>
class my_backend {
   using host_t     = detail::host_type_t<HostFunctions>;
   using context_t  = typename Impl::template context<HostFunctions>;
   using parser_t   = typename Impl::template parser<HostFunctions, Options, DebugInfo>;

   inline my_backend& initialize(host_t* host=nullptr) {
      if (memory_alloc) {
         ctx.reset();
         ctx.execute_start(host, interpret_visitor(ctx));
      }
      return *this;
   }
   inline my_backend& initialize(host_t& host) {
      return initialize(&host);
   }
   void construct(host_t* host=nullptr) {
      mod.finalize();
      ctx.set_wasm_allocator(memory_alloc);
      if constexpr (!std::is_same_v<HostFunctions, std::nullptr_t>)
         HostFunctions::resolve(mod);
      // FIXME: should not hard code knowledge of null_backend here
      if constexpr (!std::is_same_v<Impl, null_backend>)
         initialize(host);
   }

 private:
   wasm_allocator* memory_alloc = nullptr; // non owning pointer
   module          mod;
   DebugInfo       debug;
   context_t       ctx;

 public:

   my_backend(wasm_code& code, host_t& host, wasm_allocator* alloc, const Options& options = Options{})
       : memory_alloc(alloc), ctx(parser_t{ mod.allocator, options }.parse_module(code, mod, debug), detail::get_max_call_depth(options)) {
      ctx.set_max_pages(detail::get_max_pages(options));
      construct(&host);
   }

   template <typename... Args>
   inline bool call(host_t& host, const std::string_view& m, const std::string_view& func, Args... args) {
      ctx.execute(&host, my_visitor(ctx), func, args...);
      return true;
   }

    inline module & get_module() { return mod; }

};

// Specific the backend with example_host_methods for host functions.
using rhf_t     = eosio::vm::registered_host_functions<example_host_methods, execution_interface, cnv>;
using my_backend_t = my_backend<rhf_t>;

// A visitor struct declares a set of methods which will be called in process of the wasm parsing
struct nm_debug_info {
    using builder = nm_debug_info;
    void on_code_start(const void* compiled_base, const void* wasm_code_start) {
        wasm_base = wasm_code_start;
    }
    void on_function_start(const void* code_addr, const void* wasm_addr) {
        function_offsets.push_back(static_cast<std::uint32_t>(reinterpret_cast<const char*>(wasm_addr) - reinterpret_cast<const char*>(wasm_base)));
    }
    void on_instr_start(const void* code_addr, const void* wasm_addr) {}
    void on_code_end(const void* code_addr, const void* wasm_addr) {}
    void set(nm_debug_info&& other) { *this = std::move(other); }
    void relocate(const void*) {}

    uint32_t get_function(std::uint32_t addr) {
        auto pos = std::lower_bound(function_offsets.begin(), function_offsets.end(), addr + 1);
        if(pos == function_offsets.begin()) return 0;
        return (pos - function_offsets.begin()) - 1;
    }
    const void* wasm_base;
    std::vector<uint32_t> function_offsets;
};

// Options of the parser.
struct nm_options {
    static constexpr bool parse_custom_section_name = true;
};



/**
 * Simple implementation of an interpreter using eos-vm.
 */
int main(int argc, char** argv) {
/*
   if (argc < 2) {
      std::cerr << "Please enter wasm file\n";
      return -1;
   }
*/
   // Thread specific `allocator` used for wasm linear memory.
   wasm_allocator wa;


   rhf_t::add<&example_host_methods::print_string, test_name>("env", "print_string");
   // register print_num
   rhf_t::add<&example_host_methods::print_num>("env", "print_num");
   // register eosio_assert
   rhf_t::add<&example_host_methods::eosio_assert>("env", "eosio_assert");
   // register print_name
   rhf_t::add<&example_host_methods::print_name, test_name>("env", "print_name");
   rhf_t::add<&example_host_methods::print_span>("env", "print_span");
   rhf_t::add<&example_host_methods::current_time>("env", "current_time");
   rhf_t::add<&example_host_methods::set_blockchain_parameters_packed>("env", "set_blockchain_parameters_packed");
   rhf_t::add<&example_host_methods::get_blockchain_parameters_packed>("env", "get_blockchain_parameters_packed");
   rhf_t::add<&example_host_methods::set_proposed_producers>("env", "set_proposed_producers");
   rhf_t::add<&example_host_methods::get_active_producers>("env", "get_active_producers");
   rhf_t::add<&example_host_methods::prints_l>("env", "prints_l");


    watchdog wd{std::chrono::seconds(3)};

    try {

        // Load the wasm code as a binary array
        std::cout << "loading test_wasm.wasm\n" << endl;
        auto code = read_wasm("./test_wasm.wasm");

        nm_debug_info info;
        module mod;
        binary_parser<null_writer, nm_options, nm_debug_info> parser(mod.allocator);
        // Parse the loaded code and create a list of names of finctions.
        parser.parse_module(code, mod, info);
        // Iterate through all functions
        for(std::size_t i = 0; i < info.function_offsets.size(); ++i) {
            // finction indices section goes right after the imports section
            auto idx = i + mod.get_imported_functions_size();
            // get a name of a function by its index
            if(guarded_vector<uint8_t>* name = find_export_name(mod, i + mod.get_imported_functions_size())) {
                auto n = (std::string(reinterpret_cast<const char*>(name->raw()), name->size()));
                // add the function index and the name to the map
                function_names[idx] = n;
            } else {
                // if no name for the index add the index instead of the name
                auto n = ("fn" + std::to_string( i + mod.get_imported_functions_size()));
                function_names[idx] = n;
            }
        }

        example_host_methods ehm;

        std::cout << "Instantiate a new backend using the wasm provided.\n" << endl;
        // Instantiate a new backend using the wasm provided.
        my_backend_t bkend( code, ehm, &wa );

        std::cout << "Execute apply.\n" << endl;
        // Instantiate a "host"
        ehm.field = "testing";
        // Execute apply.
        bkend.call(ehm, "env", "apply", (uint64_t)0, (uint64_t)0, (uint64_t)0);
        stats.print(bkend.get_module());
   }
   catch ( const eosio::vm::exception& ex ) {
      std::cerr << ex.what() << " : " << ex.detail() <<  "\n";
   }
   catch (const std::exception &e) {
      std::cerr << e.what();
   }
   catch (...) { std::cerr << "eos-vm interpreter error\n"; }
   return 0;
}
