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
    .wrap_struct_name = "foo",
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

VALUE code_holder_m_initialize(VALUE self) {
    CodeHolderWrapper *wrapper;
    TypedData_Get_Struct(self, CodeHolderWrapper, &code_holder_type, wrapper);

    CodeHolder *code = wrapper->code;

    code->init(jit_runtime.environment());

    x86::Assembler a(code);

    a.mov(x86::eax, 1);
    a.ret();

    int (*fn)(void);
    Error err = jit_runtime.add(&fn, code);

    return self;
}

extern "C" void
Init_asmjit(void)
{
    //jit_runtime = new JitRuntime();
    rb_mAsmjit = rb_define_module("AsmJIT");


	VALUE cFoo = rb_define_class_under(rb_mAsmjit, "CodeHolder", rb_cObject);

	rb_define_alloc_func(cFoo, code_holder_alloc);
	rb_define_method(cFoo, "initialize", code_holder_m_initialize, 0);
}
