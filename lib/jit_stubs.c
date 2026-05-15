#include <caml/mlvalues.h>
#include <caml/memory.h>
#include <caml/alloc.h>
#include <stdint.h>

typedef int64_t (*ardium_main_t)();

CAMLprim value ml_ardium_call_main(value v_ptr) {
    CAMLparam1(v_ptr);
    void *ptr = (void *)Nativeint_val(v_ptr);
    ardium_main_t main_fn = (ardium_main_t)ptr;
    int64_t res = main_fn();
    CAMLreturn(caml_copy_int64(res));
}
