/*
 * This file is part of the Micro Python project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013, 2014 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>

#include "misc.h"
#include "mpconfig.h"
#include "qstr.h"
#include "obj.h"
#include "runtime.h"
#include "stream.h"
#include "file.h"
#include "ff.h"

typedef struct _pyb_file_obj_t {
    mp_obj_base_t base;
    FIL fp;
} pyb_file_obj_t;

void file_obj_print(void (*print)(void *env, const char *fmt, ...), void *env, mp_obj_t self_in, mp_print_kind_t kind) {
    printf("<io.FileIO %p>", self_in);
}

STATIC machine_int_t file_read(mp_obj_t self_in, void *buf, machine_uint_t size, int *errcode) {
    pyb_file_obj_t *self = self_in;
    UINT sz_out;
    *errcode = f_read(&self->fp, buf, size, &sz_out);
    return sz_out;
}

STATIC machine_int_t file_write(mp_obj_t self_in, const void *buf, machine_uint_t size, int *errcode) {
    pyb_file_obj_t *self = self_in;
    UINT sz_out;
    *errcode = f_write(&self->fp, buf, size, &sz_out);
    return sz_out;
}

mp_obj_t file_obj_close(mp_obj_t self_in) {
    pyb_file_obj_t *self = self_in;
    f_close(&self->fp);
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_1(file_obj_close_obj, file_obj_close);

mp_obj_t file_obj___exit__(uint n_args, const mp_obj_t *args) {
    return file_obj_close(args[0]);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(file_obj___exit___obj, 4, 4, file_obj___exit__);

// TODO gc hook to close the file if not already closed

STATIC const mp_map_elem_t file_locals_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR_read), (mp_obj_t)&mp_stream_read_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_readall), (mp_obj_t)&mp_stream_readall_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_readline), (mp_obj_t)&mp_stream_unbuffered_readline_obj},
    { MP_OBJ_NEW_QSTR(MP_QSTR_readlines), (mp_obj_t)&mp_stream_unbuffered_readlines_obj},
    { MP_OBJ_NEW_QSTR(MP_QSTR_write), (mp_obj_t)&mp_stream_write_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_close), (mp_obj_t)&file_obj_close_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR___del__), (mp_obj_t)&file_obj_close_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR___enter__), (mp_obj_t)&mp_identity_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR___exit__), (mp_obj_t)&file_obj___exit___obj },
};

STATIC MP_DEFINE_CONST_DICT(file_locals_dict, file_locals_dict_table);

STATIC mp_obj_t file_obj_make_new(mp_obj_t type_in, uint n_args, uint n_kw, const mp_obj_t *args);

STATIC const mp_stream_p_t file_obj_stream_p = {
    .read = file_read,
    .write = file_write,
};

STATIC const mp_obj_type_t file_obj_type = {
    { &mp_type_type },
    .name = MP_QSTR_FileIO,
    .make_new = file_obj_make_new,
    .print = file_obj_print,
    .getiter = mp_identity,
    .iternext = mp_stream_unbuffered_iter,
    .stream_p = &file_obj_stream_p,
    .locals_dict = (mp_obj_t)&file_locals_dict,
};

STATIC mp_obj_t file_obj_make_new(mp_obj_t type_in, uint n_args, uint n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 1, 2, false);
    const char *filename = mp_obj_str_get_str(args[0]);
    const char *mode = "r";
    if (n_args > 1) {
        mode = mp_obj_str_get_str(args[1]);
    }
    pyb_file_obj_t *self = m_new_obj_with_finaliser(pyb_file_obj_t);
    self->base.type = &file_obj_type;
    if (mode[0] == 'r') {
        // open for reading
        FRESULT res = f_open(&self->fp, filename, FA_READ);
        if (res != FR_OK) {
            printf("FileNotFoundError: [Errno 2] No such file or directory: '%s'\n", filename);
            return mp_const_none;
        }
    } else if (mode[0] == 'w') {
        // open for writing, truncate the file first
        FRESULT res = f_open(&self->fp, filename, FA_WRITE | FA_CREATE_ALWAYS);
        if (res != FR_OK) {
            printf("?FileError: could not create file: '%s'\n", filename);
            return mp_const_none;
        }
    } else {
        printf("ValueError: invalid mode: '%s'\n", mode);
        return mp_const_none;
    }
    return self;
}

// Factory function for I/O stream classes
STATIC mp_obj_t pyb_io_open(uint n_args, const mp_obj_t *args) {
    // TODO: analyze mode and buffering args and instantiate appropriate type
    return file_obj_make_new((mp_obj_t)&file_obj_type, n_args, 0, args);
}

MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_builtin_open_obj, 1, 2, pyb_io_open);
