#include <asmjit/asmjit.h>

#include "asmjit.h"

using namespace asmjit;

VALUE rb_mAsmjit;

static JitRuntime jit_runtime;

struct CodeHolderWrapper {
    CodeHolder *code;
};

void code_holder_free(void *data) {
    CodeHolderWrapper *wrapper = static_cast<CodeHolderWrapper *>(data);
    delete wrapper->code;
    xfree(wrapper);
}

static const rb_data_type_t code_holder_type = {
    .wrap_struct_name = "AsmJIT::CodeHolder",
    .function = {
        .dmark = NULL,
        .dfree = code_holder_free,
        .dsize = NULL,
    },
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY,
};

VALUE code_holder_alloc(VALUE self) {
    CodeHolderWrapper *wrapper = static_cast<CodeHolderWrapper *>(xmalloc(sizeof(CodeHolderWrapper)));
    wrapper->code = new CodeHolder();

    return TypedData_Wrap_Struct(self, &code_holder_type, wrapper);
}

VALUE code_holder_initialize(VALUE self) {
    CodeHolderWrapper *wrapper;
    TypedData_Get_Struct(self, CodeHolderWrapper, &code_holder_type, wrapper);

    CodeHolder *code = wrapper->code;

    code->init(jit_runtime.environment());

    //x86::Assembler a(code);

    //a.mov(x86::eax, 1);
    //a.ret();

    //int (*fn)(void);
    //Error err = jit_runtime.add(&fn, code);

    return self;
}

struct x86AssemblerWrapper {
    x86::Assembler *assembler;
    VALUE code_holder;
};

void x86_assembler_free(void *data) {
    x86AssemblerWrapper *wrapper = static_cast<x86AssemblerWrapper *>(data);
    delete wrapper->assembler;
    xfree(wrapper);
}

void x86_assembler_mark(void *data) {
    x86AssemblerWrapper *wrapper = static_cast<x86AssemblerWrapper *>(data);
    rb_gc_mark(wrapper->code_holder);
}

static const rb_data_type_t x86_assembler_type = {
    .wrap_struct_name = "AsmJIT::Assembler",
    .function = {
        .dmark = NULL,
        .dfree = x86_assembler_free,
        .dsize = NULL,
    },
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY,
};

VALUE x86_assembler_alloc(VALUE self) {
    x86AssemblerWrapper *wrapper = static_cast<x86AssemblerWrapper *>(xmalloc(sizeof(CodeHolderWrapper)));
    wrapper->assembler = NULL;
    wrapper->code_holder = Qnil;

    return TypedData_Wrap_Struct(self, &x86_assembler_type, wrapper);
}

VALUE x86_assembler_initialize(VALUE self, VALUE code_holder) {
    x86AssemblerWrapper *wrapper;
    TypedData_Get_Struct(self, x86AssemblerWrapper, &x86_assembler_type, wrapper);

    CodeHolderWrapper *code_wrapper;
    TypedData_Get_Struct(code_holder, CodeHolderWrapper, &code_holder_type, code_wrapper);

    wrapper->code_holder = code_holder;
    wrapper->assembler = new x86::Assembler(code_wrapper->code);

    return self;
}

extern "C" void
Init_asmjit(void)
{
    //jit_runtime = new JitRuntime();
    rb_mAsmjit = rb_define_module("AsmJIT");

	VALUE cCodeHolder = rb_define_class_under(rb_mAsmjit, "CodeHolder", rb_cObject);
	rb_define_alloc_func(cCodeHolder, code_holder_alloc);
	rb_define_method(cCodeHolder, "initialize", code_holder_initialize, 0);

	VALUE rb_mX86 = rb_define_module_under(rb_mAsmjit, "X86");

	VALUE cX86Assembler = rb_define_class_under(rb_mX86, "Assembler", rb_cObject);
	rb_define_alloc_func(cX86Assembler, x86_assembler_alloc);
	rb_define_method(cX86Assembler, "initialize", x86_assembler_initialize, 1);
}
