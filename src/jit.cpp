#include <fstream>
#include <memory>

#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/ExecutionEngine/RuntimeDyld.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/DynamicLibrary.h>

#include <impala/impala.h>
#include <impala/ast.h>
#include <thorin/world.h>
#include <thorin/transform/codegen_prepare.h>
#include <thorin/be/llvm/cpu.h>

#include "anydsl_runtime.h"

struct MemBuf : public std::streambuf {
    MemBuf(const char* string, uint32_t size) {
        auto begin = const_cast<char*>(string);
        auto end   = const_cast<char*>(string + size);
        setg(begin, begin, end);
    }
};

std::string get_runtime_src() {
    std::string runtime_src;
    auto load_file = [&](const std::string file) {
        std::ifstream str(file);
        runtime_src += std::string(std::istreambuf_iterator<char>(str), (std::istreambuf_iterator<char>()));
    };

    load_file(ANYDSL_RUNTIME_DIR "/platforms/intrinsics_amdgpu.impala");
    load_file(ANYDSL_RUNTIME_DIR "/platforms/intrinsics_cpu.impala");
    load_file(ANYDSL_RUNTIME_DIR "/platforms/intrinsics_cuda.impala");
    load_file(ANYDSL_RUNTIME_DIR "/platforms/intrinsics_hls.impala");
    load_file(ANYDSL_RUNTIME_DIR "/platforms/intrinsics.impala");
    load_file(ANYDSL_RUNTIME_DIR "/platforms/intrinsics_nvvm.impala");
    load_file(ANYDSL_RUNTIME_DIR "/platforms/intrinsics_opencl.impala");
    load_file(ANYDSL_RUNTIME_DIR "/platforms/intrinsics_thorin.impala");
    load_file(ANYDSL_RUNTIME_DIR "/src/runtime.impala");

    return runtime_src;
}

void anydsl_link(const char* lib) {
    llvm::sys::DynamicLibrary::LoadLibraryPermanently(lib);
}

void* anydsl_compile(const char* string, uint32_t size, const char* fn_name, uint32_t opt) {
    static constexpr auto module_name = "jit";
    static constexpr bool debug = false;
    assert(opt <= 3);

    std::string program = get_runtime_src() + string;
    MemBuf buf(program.c_str(), program.size());
    std::istream is(&buf);
    impala::init();
    impala::Items items;
    impala::parse(items, is, module_name);

    auto module = std::make_unique<const impala::Module>(module_name, std::move(items));
    impala::global_num_warnings = 0;
    impala::global_num_errors   = 0;
    std::unique_ptr<impala::TypeTable> typetable;
    impala::check(typetable, module.get(), false);
    if (impala::global_num_errors != 0)
        return nullptr;

    thorin::World world(module_name);
    impala::emit(world, module.get());
    world.cleanup();
    world.opt();
    world.cleanup();
    thorin::codegen_prepare(world);
    thorin::CPUCodeGen cg(world);
    auto& llvm_module = cg.emit(opt, debug, false);
    auto fn = llvm_module->getFunction(fn_name);
    if (!fn)
        return nullptr;

    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    auto engine = llvm::EngineBuilder(std::move(llvm_module))
        .setEngineKind(llvm::EngineKind::JIT)
        .setOptLevel(   opt == 0  ? llvm::CodeGenOpt::None    :
                        opt == 1  ? llvm::CodeGenOpt::Less    :
                        opt == 2  ? llvm::CodeGenOpt::Default :
                     /* opt == 3 */ llvm::CodeGenOpt::Aggressive)
        .create();
    if (!engine)
        return nullptr;

    engine->finalizeObject();
    return engine->getPointerToFunction(fn);
}
