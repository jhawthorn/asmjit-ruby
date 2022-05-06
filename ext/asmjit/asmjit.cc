#include <asmjit/asmjit.h>

#include "asmjit.h"

using namespace asmjit;

VALUE rb_mAsmjit;

static JitRuntime *jit_runtime;

void foo_free(void *) {}

static const rb_data_type_t foo_type = {
    .wrap_struct_name = "foo",
    .function = {
        .dmark = NULL,
        .dfree = foo_free,
        .dsize = NULL,
    },
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY,
};

extern "C" void
Init_asmjit(void)
{
    jit_runtime = new JitRuntime();
    rb_mAsmjit = rb_define_module("AsmJIT");
}
