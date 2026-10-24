// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 Koen De Vleeschauwer

// mui module: u8g2 mui menus, driven from python.
//
// forms (fds) are bytes, built with form(), label(), xy(), ...
// fields are Field objects. each field owns its value; mui edits it in place.
// on_change(value) callbacks run after next()/prev()/select()/... return,
// outside of mui.

#include <string.h>
#include "py/runtime.h"
#include "modu8g2.h"
#include "mui.h"
#include "mui_u8g2.h"

/******************************************************************************/
// widgets

enum {
    KIND_NONE,      // no data
    KIND_U8,        // uint8_t variable
    KIND_U8MM,      // uint8_t min max
    KIND_S8MM,      // int8_t min max
    KIND_U8MMS,     // uint8_t min max step (bar)
    KIND_U16LIST,   // uint16_t index into a list of strings
    KIND_FONT,      // font style
};

#define SEL MUIF_CFLAG_IS_CURSOR_SELECTABLE

// W(python name, field function, kind, cflags)
#define WIDGETS \
    W(LABEL, mui_u8g2_draw_text, KIND_NONE, 0) \
    W(HLINE, mui_hline, KIND_NONE, 0) \
    W(FONT_STYLE, mui_u8g2_set_font_style_function, KIND_FONT, 0) \
    W(BTN_GOTO_WM_FI, mui_u8g2_btn_goto_wm_fi, KIND_NONE, SEL) \
    W(BTN_GOTO_WM_IF, mui_u8g2_btn_goto_wm_if, KIND_NONE, SEL) \
    W(BTN_GOTO_W2_FI, mui_u8g2_btn_goto_w2_fi, KIND_NONE, SEL) \
    W(BTN_GOTO_W2_IF, mui_u8g2_btn_goto_w2_if, KIND_NONE, SEL) \
    W(BTN_GOTO_W1_PI, mui_u8g2_btn_goto_w1_pi, KIND_NONE, SEL) \
    W(BTN_GOTO_W1_FI, mui_u8g2_btn_goto_w1_fi, KIND_NONE, SEL) \
    W(BTN_BACK_WM_FI, mui_u8g2_btn_back_wm_fi, KIND_NONE, SEL) \
    W(BTN_BACK_WM_IF, mui_u8g2_btn_back_wm_if, KIND_NONE, SEL) \
    W(BTN_BACK_W2_FI, mui_u8g2_btn_back_w2_fi, KIND_NONE, SEL) \
    W(BTN_BACK_W2_IF, mui_u8g2_btn_back_w2_if, KIND_NONE, SEL) \
    W(BTN_BACK_W1_PI, mui_u8g2_btn_back_w1_pi, KIND_NONE, SEL) \
    W(BTN_BACK_W1_FI, mui_u8g2_btn_back_w1_fi, KIND_NONE, SEL) \
    W(BTN_EXIT_WM_FI, mui_u8g2_btn_exit_wm_fi, KIND_NONE, SEL) \
    W(BTN_EXIT_WM_IF, mui_u8g2_btn_exit_wm_if, KIND_NONE, SEL) \
    W(BTN_EXIT_W2_FI, mui_u8g2_btn_exit_w2_fi, KIND_NONE, SEL) \
    W(BTN_EXIT_W2_IF, mui_u8g2_btn_exit_w2_if, KIND_NONE, SEL) \
    W(BTN_EXIT_W1_PI, mui_u8g2_btn_exit_w1_pi, KIND_NONE, SEL) \
    W(BTN_EXIT_W1_FI, mui_u8g2_btn_exit_w1_fi, KIND_NONE, SEL) \
    W(GOTO_DATA, mui_u8g2_goto_data, KIND_NONE, 0) \
    W(GOTO_FORM_W1_PI, mui_u8g2_goto_form_w1_pi, KIND_NONE, SEL) \
    W(GOTO_FORM_W1_PF, mui_u8g2_goto_form_w1_pf, KIND_NONE, SEL) \
    W(U8_CHKBOX_WM_PI, mui_u8g2_u8_chkbox_wm_pi, KIND_U8, SEL) \
    W(U8_RADIO_WM_PI, mui_u8g2_u8_radio_wm_pi, KIND_U8, SEL) \
    W(U8_OPT_LINE_WA_MSE_PI, mui_u8g2_u8_opt_line_wa_mse_pi, KIND_U8, SEL) \
    W(U8_OPT_LINE_WA_MSE_PF, mui_u8g2_u8_opt_line_wa_mse_pf, KIND_U8, SEL) \
    W(U8_OPT_LINE_WA_MUD_PI, mui_u8g2_u8_opt_line_wa_mud_pi, KIND_U8, SEL) \
    W(U8_OPT_LINE_WA_MUD_PF, mui_u8g2_u8_opt_line_wa_mud_pf, KIND_U8, SEL) \
    W(U8_OPT_PARENT_WM_PI, mui_u8g2_u8_opt_parent_wm_pi, KIND_U8, SEL) \
    W(U8_OPT_RADIO_CHILD_WM_PI, mui_u8g2_u8_opt_radio_child_wm_pi, KIND_U8, SEL) \
    W(U8_OPT_RADIO_CHILD_W1_PI, mui_u8g2_u8_opt_radio_child_w1_pi, KIND_U8, SEL) \
    W(U8_OPT_CHILD_WM_PI, mui_u8g2_u8_opt_child_wm_pi, KIND_U8, SEL) \
    W(U8_CHAR_WM_MUD_PI, mui_u8g2_u8_char_wm_mud_pi, KIND_U8, SEL) \
    W(U8_MIN_MAX_WM_MSE_PI, mui_u8g2_u8_min_max_wm_mse_pi, KIND_U8MM, SEL) \
    W(U8_MIN_MAX_WM_MUD_PI, mui_u8g2_u8_min_max_wm_mud_pi, KIND_U8MM, SEL) \
    W(U8_MIN_MAX_WM_MSE_PF, mui_u8g2_u8_min_max_wm_mse_pf, KIND_U8MM, SEL) \
    W(U8_MIN_MAX_WM_MUD_PF, mui_u8g2_u8_min_max_wm_mud_pf, KIND_U8MM, SEL) \
    W(X8_MIN_MAX_WM_MSE_PI, mui_u8g2_x8_min_max_wm_mse_pi, KIND_U8MM, SEL) \
    W(X8_MIN_MAX_WM_MUD_PI, mui_u8g2_x8_min_max_wm_mud_pi, KIND_U8MM, SEL) \
    W(X8_MIN_MAX_WM_MSE_PF, mui_u8g2_x8_min_max_wm_mse_pf, KIND_U8MM, SEL) \
    W(X8_MIN_MAX_WM_MUD_PF, mui_u8g2_x8_min_max_wm_mud_pf, KIND_U8MM, SEL) \
    W(S8_MIN_MAX_WM_MSE_PI, mui_u8g2_s8_min_max_wm_mse_pi, KIND_S8MM, SEL) \
    W(S8_MIN_MAX_WM_MUD_PI, mui_u8g2_s8_min_max_wm_mud_pi, KIND_S8MM, SEL) \
    W(S8_MIN_MAX_WM_MSE_PF, mui_u8g2_s8_min_max_wm_mse_pf, KIND_S8MM, SEL) \
    W(S8_MIN_MAX_WM_MUD_PF, mui_u8g2_s8_min_max_wm_mud_pf, KIND_S8MM, SEL) \
    W(U8_BAR_WM_MSE_PI, mui_u8g2_u8_bar_wm_mse_pi, KIND_U8MMS, SEL) \
    W(U8_BAR_WM_MUD_PI, mui_u8g2_u8_bar_wm_mud_pi, KIND_U8MMS, SEL) \
    W(U8_BAR_WM_MSE_PF, mui_u8g2_u8_bar_wm_mse_pf, KIND_U8MMS, SEL) \
    W(U8_BAR_WM_MUD_PF, mui_u8g2_u8_bar_wm_mud_pf, KIND_U8MMS, SEL) \
    W(U8_FIXED_WIDTH_BAR_WM_MSE_PI, mui_u8g2_u8_fixed_width_bar_wm_mse_pi, KIND_U8MMS, SEL) \
    W(U8_FIXED_WIDTH_BAR_WM_MUD_PI, mui_u8g2_u8_fixed_width_bar_wm_mud_pi, KIND_U8MMS, SEL) \
    W(U8_FIXED_WIDTH_BAR_WM_MSE_PF, mui_u8g2_u8_fixed_width_bar_wm_mse_pf, KIND_U8MMS, SEL) \
    W(U8_FIXED_WIDTH_BAR_WM_MUD_PF, mui_u8g2_u8_fixed_width_bar_wm_mud_pf, KIND_U8MMS, SEL) \
    W(U16_LIST_LINE_WA_MSE_PI, mui_u8g2_u16_list_line_wa_mse_pi, KIND_U16LIST, SEL) \
    W(U16_LIST_LINE_WA_MUD_PI, mui_u8g2_u16_list_line_wa_mud_pi, KIND_U16LIST, SEL) \
    W(U16_LIST_PARENT_WM_PI, mui_u8g2_u16_list_parent_wm_pi, KIND_U16LIST, SEL) \
    W(U16_LIST_CHILD_W1_PI, mui_u8g2_u16_list_child_w1_pi, KIND_U16LIST, SEL) \
    W(U16_LIST_GOTO_W1_PI, mui_u8g2_u16_list_goto_w1_pi, KIND_U16LIST, SEL)

enum {
    #define W(name, cb, kind, cflags) WIDGET_##name,
    WIDGETS
    #undef W
    WIDGET_COUNT
};

typedef struct {
    muif_cb cb;
    uint8_t kind;
    uint8_t cflags;
} mui_widget_t;

static const mui_widget_t mui_widgets[WIDGET_COUNT] = {
    #define W(name, cb, kind, cflags) { cb, kind, cflags },
    WIDGETS
    #undef W
};

/******************************************************************************/
// Field

typedef struct _mui_field_obj_t {
    mp_obj_base_t base;
    uint8_t id0, id1;
    uint8_t widget;
    bool exec_on_select;        // MUIF_CFLAG_IS_EXECUTE_ON_SELECT
    union {
        uint8_t u8;
        int8_t s8;
        uint16_t u16;
    } v;
    mp_int_t last;              // value seen by the last on_change check
    union {
        struct mui_u8g2_u8_min_max_struct u8mm;
        struct mui_u8g2_s8_min_max_struct s8mm;
        struct mui_u8g2_u8_min_max_step_struct u8mms;
        struct mui_u8g2_list_struct list;
    } d;
    mp_obj_t items;             // tuple of str, KIND_U16LIST
    mp_obj_t font;              // KIND_FONT
    mp_obj_t on_change;
} mui_field_obj_t;

static uint8_t mui_field_kind(const mui_field_obj_t *self) {
    return mui_widgets[self->widget].kind;
}

static mp_int_t mui_field_get(const mui_field_obj_t *self) {
    switch (mui_field_kind(self)) {
        case KIND_U8:
        case KIND_U8MM:
        case KIND_U8MMS:
            return self->v.u8;
        case KIND_S8MM:
            return self->v.s8;
        case KIND_U16LIST:
            return self->v.u16;
        default:
            return 0;
    }
}

static void mui_field_set(mui_field_obj_t *self, mp_obj_t value_in) {
    mp_int_t v = mp_obj_get_int(value_in);
    mp_int_t lo = 0, hi = 0;
    switch (mui_field_kind(self)) {
        case KIND_U8:
            lo = 0;
            hi = 255;
            break;
        case KIND_U8MM:
            lo = self->d.u8mm.min;
            hi = self->d.u8mm.max;
            break;
        case KIND_S8MM:
            lo = self->d.s8mm.min;
            hi = self->d.s8mm.max;
            break;
        case KIND_U8MMS:
            lo = self->d.u8mms.min;
            hi = self->d.u8mms.max;
            break;
        case KIND_U16LIST:
            hi = (mp_int_t)MP_OBJ_SMALL_INT_VALUE(mp_obj_len(self->items)) - 1;
            break;
        default:
            mp_raise_TypeError(MP_ERROR_TEXT("field has no value"));
    }
    if (v < lo || v > hi) {
        mp_raise_msg_varg(&mp_type_ValueError, MP_ERROR_TEXT("value %d..%d"), (int)lo, (int)hi);
    }
    switch (mui_field_kind(self)) {
        case KIND_S8MM:
            self->v.s8 = v;
            break;
        case KIND_U16LIST:
            self->v.u16 = v;
            break;
        default:
            self->v.u8 = v;
            break;
    }
    self->last = v;
}

// u16 list callbacks, data is the field
static const char *mui_list_element(void *data, uint16_t index) {
    mui_field_obj_t *self = data;
    size_t len;
    mp_obj_t *items;
    mp_obj_tuple_get(self->items, &len, &items);
    return index < len ? mp_obj_str_get_str(items[index]) : "";
}

static uint16_t mui_list_count(void *data) {
    mui_field_obj_t *self = data;
    return MP_OBJ_SMALL_INT_VALUE(mp_obj_len(self->items));
}

static mp_int_t mui_arg_range(mp_obj_t arg, mp_int_t lo, mp_int_t hi, qstr name) {
    mp_int_t v = mp_obj_get_int(arg);
    if (v < lo || v > hi) {
        mp_raise_msg_varg(&mp_type_ValueError, MP_ERROR_TEXT("%q %d..%d"), name, (int)lo, (int)hi);
    }
    return v;
}

static void mui_check_min_max(mp_int_t min, mp_int_t max) {
    if (min > max) {
        mp_raise_ValueError(MP_ERROR_TEXT("min > max"));
    }
}

// Field(id, widget, *, value=None, min=, max=, step=1, flags=0, width=0, items=None, font=None,
//       exec_on_select=False, on_change=None)
// value defaults to min for the min/max widgets, else to 0.
static mp_obj_t mui_field_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    enum { ARG_id, ARG_widget, ARG_value, ARG_min, ARG_max, ARG_step, ARG_flags, ARG_width, ARG_items, ARG_font, ARG_exec_on_select, ARG_on_change };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_id, MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_widget, MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_value, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_min, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_max, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_step, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 1} },
        { MP_QSTR_flags, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_width, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_items, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_font, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_exec_on_select, MP_ARG_KW_ONLY | MP_ARG_BOOL, {.u_bool = false} },
        { MP_QSTR_on_change, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none} },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    size_t id_len;
    const char *id = mp_obj_str_get_data(args[ARG_id].u_obj, &id_len);
    if (id_len != 2) {
        mp_raise_ValueError(MP_ERROR_TEXT("id is 2 chars"));
    }
    mp_int_t widget = args[ARG_widget].u_int;
    if (widget < 0 || widget >= WIDGET_COUNT) {
        mp_raise_ValueError(MP_ERROR_TEXT("unknown widget"));
    }

    if (args[ARG_exec_on_select].u_bool && !(mui_widgets[widget].cflags & SEL)) {
        mp_raise_ValueError(MP_ERROR_TEXT("exec_on_select: widget is not selectable"));
    }

    mui_field_obj_t *self = mp_obj_malloc(mui_field_obj_t, type);
    self->id0 = id[0];
    self->id1 = id[1];
    self->widget = widget;
    self->exec_on_select = args[ARG_exec_on_select].u_bool;
    self->v.u16 = 0;
    self->items = mp_const_none;
    self->font = mp_const_none;
    self->on_change = args[ARG_on_change].u_obj;
    memset(&self->d, 0, sizeof(self->d));

    mp_obj_t min_in = args[ARG_min].u_obj;
    mp_obj_t max_in = args[ARG_max].u_obj;
    switch (mui_widgets[widget].kind) {
        case KIND_U8MM:
            self->d.u8mm.value = &self->v.u8;
            self->d.u8mm.min = min_in == mp_const_none ? 0 : mui_arg_range(min_in, 0, 255, MP_QSTR_min);
            self->d.u8mm.max = max_in == mp_const_none ? 255 : mui_arg_range(max_in, 0, 255, MP_QSTR_max);
            mui_check_min_max(self->d.u8mm.min, self->d.u8mm.max);
            break;
        case KIND_S8MM:
            self->d.s8mm.value = &self->v.s8;
            self->d.s8mm.min = min_in == mp_const_none ? -128 : mui_arg_range(min_in, -128, 127, MP_QSTR_min);
            self->d.s8mm.max = max_in == mp_const_none ? 127 : mui_arg_range(max_in, -128, 127, MP_QSTR_max);
            mui_check_min_max(self->d.s8mm.min, self->d.s8mm.max);
            break;
        case KIND_U8MMS:
            self->d.u8mms.value = &self->v.u8;
            self->d.u8mms.min = min_in == mp_const_none ? 0 : mui_arg_range(min_in, 0, 255, MP_QSTR_min);
            self->d.u8mms.max = max_in == mp_const_none ? 255 : mui_arg_range(max_in, 0, 255, MP_QSTR_max);
            mui_check_min_max(self->d.u8mms.min, self->d.u8mms.max);
            self->d.u8mms.step = mui_arg_range(MP_OBJ_NEW_SMALL_INT(args[ARG_step].u_int), 1, 255, MP_QSTR_step);
            self->d.u8mms.flags = mui_arg_range(MP_OBJ_NEW_SMALL_INT(args[ARG_flags].u_int), 0, 255, MP_QSTR_flags);
            self->d.u8mms.width = mui_arg_range(MP_OBJ_NEW_SMALL_INT(args[ARG_width].u_int), 0, 255, MP_QSTR_width);
            break;
        case KIND_U16LIST: {
            if (args[ARG_items].u_obj == mp_const_none) {
                mp_raise_TypeError(MP_ERROR_TEXT("items required"));
            }
            // keep a private tuple of str
            mp_obj_t items = mp_call_function_1(MP_OBJ_FROM_PTR(&mp_type_tuple), args[ARG_items].u_obj);
            size_t len;
            mp_obj_t *elems;
            mp_obj_tuple_get(items, &len, &elems);
            if (len == 0 || len > 0xffff) {
                mp_raise_ValueError(MP_ERROR_TEXT("items: 1..65535 strings"));
            }
            for (size_t i = 0; i < len; i++) {
                if (!mp_obj_is_str(elems[i])) {
                    mp_raise_TypeError(MP_ERROR_TEXT("items must be str"));
                }
            }
            self->items = items;
            self->d.list.selection = &self->v.u16;
            self->d.list.data = self;
            self->d.list.get_list_element = mui_list_element;
            self->d.list.get_list_count = mui_list_count;
            break;
        }
        case KIND_FONT:
            u8g2_font_get(args[ARG_font].u_obj); // type check
            self->font = args[ARG_font].u_obj;
            break;
        default:
            break;
    }

    mp_obj_t value_in = args[ARG_value].u_obj;
    if (mui_widgets[widget].kind == KIND_NONE || mui_widgets[widget].kind == KIND_FONT) {
        self->last = 0;
    } else {
        if (value_in == mp_const_none) {
            // default: the lowest value
            switch (mui_widgets[widget].kind) {
                case KIND_U8MM:
                    value_in = MP_OBJ_NEW_SMALL_INT(self->d.u8mm.min);
                    break;
                case KIND_S8MM:
                    value_in = MP_OBJ_NEW_SMALL_INT(self->d.s8mm.min);
                    break;
                case KIND_U8MMS:
                    value_in = MP_OBJ_NEW_SMALL_INT(self->d.u8mms.min);
                    break;
                default:
                    value_in = MP_OBJ_NEW_SMALL_INT(0);
                    break;
            }
        }
        mui_field_set(self, value_in);
    }
    return MP_OBJ_FROM_PTR(self);
}

static void mui_field_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    mui_field_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_printf(print, "<Field '%c%c' value=%d>", self->id0, self->id1, (int)mui_field_get(self));
}

// field.value
static void mui_field_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    mui_field_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (attr != MP_QSTR_value) {
        dest[1] = MP_OBJ_SENTINEL; // not found
        return;
    }
    if (dest[0] == MP_OBJ_NULL) {
        dest[0] = MP_OBJ_NEW_SMALL_INT(mui_field_get(self));
    } else if (dest[1] != MP_OBJ_NULL) {
        mui_field_set(self, dest[1]);
        dest[0] = MP_OBJ_NULL; // stored
    }
}

static MP_DEFINE_CONST_OBJ_TYPE(
    mui_field_type,
    MP_QSTR_Field,
    MP_TYPE_FLAG_NONE,
    make_new, mui_field_make_new,
    print, mui_field_print,
    attr, mui_field_attr
    );

/******************************************************************************/
// the menu

static mui_t mui_ui;
static bool mui_ready;

MP_REGISTER_ROOT_POINTER(mp_obj_t mui_fds);
MP_REGISTER_ROOT_POINTER(mp_obj_t mui_fields);
MP_REGISTER_ROOT_POINTER(void *mui_muif);

extern const mp_obj_module_t mui_user_cmodule;

// set by init(). like u8g2: ready means flag set, mui still in sys.modules
// (no soft reset since init()) and u8g2 ready.
static bool mui_is_ready(void) {
    if (!mui_ready || !u8g2_is_ready()) {
        return false;
    }
    mp_map_elem_t *elem = mp_map_lookup(&MP_STATE_VM(mp_loaded_modules_dict).map, MP_OBJ_NEW_QSTR(MP_QSTR_mui), MP_MAP_LOOKUP);
    return elem != NULL && elem->value == MP_OBJ_FROM_PTR(&mui_user_cmodule);
}

static void mui_check_ready(void) {
    if (!mui_is_ready()) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("call mui.init() first"));
    }
}

// call on_change for fields whose value changed. runs outside of mui.
static void mui_dispatch_changes(void) {
    size_t len;
    mp_obj_t *fields;
    mp_obj_tuple_get(MP_STATE_VM(mui_fields), &len, &fields);
    if (len == 0) {
        return;
    }
    // update all first: an exception in a callback must not refire the others
    mp_obj_t pending[len];
    for (size_t i = 0; i < len; i++) {
        mui_field_obj_t *f = MP_OBJ_TO_PTR(fields[i]);
        mp_int_t v = mui_field_get(f);
        pending[i] = MP_OBJ_NULL;
        if (v != f->last) {
            f->last = v;
            if (f->on_change != mp_const_none) {
                pending[i] = fields[i];
            }
        }
    }
    for (size_t i = 0; i < len; i++) {
        if (pending[i] != MP_OBJ_NULL) {
            mui_field_obj_t *f = MP_OBJ_TO_PTR(pending[i]);
            mp_call_function_1(f->on_change, MP_OBJ_NEW_SMALL_INT(f->last));
        }
    }
}

// init(fds, fields): fds is bytes, fields a sequence of Field
static mp_obj_t mui_init(mp_obj_t fds_in, mp_obj_t fields_in) {
    if (!u8g2_is_ready()) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("call u8g2.init() first"));
    }
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(fds_in, &bufinfo, MP_BUFFER_READ);

    mp_obj_t fields = mp_call_function_1(MP_OBJ_FROM_PTR(&mp_type_tuple), fields_in);
    size_t len;
    mp_obj_t *elems;
    mp_obj_tuple_get(fields, &len, &elems);

    // private copy of fds, with the terminating zero mui expects
    vstr_t vstr;
    vstr_init_len(&vstr, bufinfo.len + 1);
    memcpy(vstr.buf, bufinfo.buf, bufinfo.len);
    vstr.buf[bufinfo.len] = 0;
    mp_obj_t fds = mp_obj_new_bytes_from_vstr(&vstr);
    const char *fds_data = mp_obj_str_get_data(fds, &bufinfo.len);

    struct muif_struct *muif = m_new(struct muif_struct, len);
    for (size_t i = 0; i < len; i++) {
        if (!mp_obj_is_type(elems[i], &mui_field_type)) {
            mp_raise_TypeError(MP_ERROR_TEXT("expecting Field"));
        }
        mui_field_obj_t *f = MP_OBJ_TO_PTR(elems[i]);
        const mui_widget_t *w = &mui_widgets[f->widget];
        muif[i].id0 = f->id0;
        muif[i].id1 = f->id1;
        muif[i].cflags = w->cflags | (f->exec_on_select ? MUIF_CFLAG_IS_EXECUTE_ON_SELECT : 0);
        muif[i].extra = 0;
        muif[i].cb = w->cb;
        switch (w->kind) {
            case KIND_U8:
                muif[i].data = &f->v.u8;
                break;
            case KIND_U8MM:
            case KIND_S8MM:
            case KIND_U8MMS:
            case KIND_U16LIST:
                muif[i].data = &f->d;
                break;
            case KIND_FONT:
                muif[i].data = (void *)u8g2_font_get(f->font);
                break;
            default:
                muif[i].data = NULL;
                break;
        }
        f->last = mui_field_get(f);
    }

    mui_ready = false;
    MP_STATE_VM(mui_fds) = fds;
    MP_STATE_VM(mui_fields) = fields;
    MP_STATE_VM(mui_muif) = muif;
    memset(&mui_ui, 0, sizeof(mui_ui));
    mui_Init(&mui_ui, &mp_u8g2, (fds_t *)fds_data, muif, len);
    mp_obj_dict_store(MP_OBJ_FROM_PTR(&MP_STATE_VM(mp_loaded_modules_dict)),
        MP_OBJ_NEW_QSTR(MP_QSTR_mui), MP_OBJ_FROM_PTR(&mui_user_cmodule));
    mui_ready = true;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(mui_init_obj, mui_init);

// goto_form(form, cursor=0) -> bool
static mp_obj_t mui_goto_form(size_t n_args, const mp_obj_t *args) {
    mui_check_ready();
    uint8_t form = mui_arg_range(args[0], 0, 255, MP_QSTR_form);
    uint8_t cursor = n_args > 1 ? mui_arg_range(args[1], 0, 255, MP_QSTR_cursor) : 0;
    uint8_t ok = mui_GotoForm(&mui_ui, form, cursor);
    mui_dispatch_changes();
    return mp_obj_new_bool(ok);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mui_goto_form_obj, 1, 2, mui_goto_form);

#define DEF_EVENT(pyname, cfunc) \
    static mp_obj_t mui_##pyname(void) { \
        mui_check_ready(); \
        cfunc(&mui_ui); \
        mui_dispatch_changes(); \
        return mp_const_none; \
    } \
    static MP_DEFINE_CONST_FUN_OBJ_0(mui_##pyname##_obj, mui_##pyname);

DEF_EVENT(next, mui_NextField)
DEF_EVENT(prev, mui_PrevField)
DEF_EVENT(select, mui_SendSelect)
// select_exec(): like select(), but a field made with exec_on_select=True
// is selected instead, wherever the cursor is
DEF_EVENT(select_exec, mui_SendSelectWithExecuteOnSelectFieldSearch)
DEF_EVENT(inc, mui_SendValueIncrement)
DEF_EVENT(dec, mui_SendValueDecrement)
DEF_EVENT(leave, mui_LeaveForm)

// cursor(): position of the cursor in the current form: n-th selectable field, from 0
static mp_obj_t mui_cursor(void) {
    mui_check_ready();
    return MP_OBJ_NEW_SMALL_INT(mui_GetCurrentCursorFocusPosition(&mui_ui));
}
static MP_DEFINE_CONST_FUN_OBJ_0(mui_cursor_obj, mui_cursor);

// save_form(cursor=None): remember current form and cursor, for restore_form()
// and the BTN_BACK widgets. with cursor: remember that cursor position instead.
static mp_obj_t mui_save_form(size_t n_args, const mp_obj_t *args) {
    mui_check_ready();
    if (n_args == 0 || args[0] == mp_const_none) {
        mui_SaveForm(&mui_ui);
    } else {
        mui_SaveFormWithCursorPosition(&mui_ui, mui_arg_range(args[0], 0, 255, MP_QSTR_cursor));
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mui_save_form_obj, 0, 1, mui_save_form);

// restore_form() -> bool: back to the form saved with save_form()
static mp_obj_t mui_restore_form(void) {
    mui_check_ready();
    uint8_t ok = mui_RestoreForm(&mui_ui);
    mui_dispatch_changes();
    return mp_obj_new_bool(ok);
}
static MP_DEFINE_CONST_FUN_OBJ_0(mui_restore_form_obj, mui_restore_form);

// save_cursor(pos): remember cursor position pos for the current form,
// used by goto_form_auto() and the goto buttons
static mp_obj_t mui_save_cursor(mp_obj_t pos_in) {
    mui_check_ready();
    if (!mui_IsFormActive(&mui_ui)) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("no active form"));
    }
    mui_SaveCursorPosition(&mui_ui, mui_arg_range(pos_in, 0, 255, MP_QSTR_cursor));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mui_save_cursor_obj, mui_save_cursor);

// goto_form_auto(form) -> bool: go to form, cursor at its remembered position
static mp_obj_t mui_goto_form_auto(mp_obj_t form_in) {
    mui_check_ready();
    uint8_t ok = mui_GotoFormAutoCursorPosition(&mui_ui, mui_arg_range(form_in, 0, 255, MP_QSTR_form));
    mui_dispatch_changes();
    return mp_obj_new_bool(ok);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mui_goto_form_auto_obj, mui_goto_form_auto);

// draw(): render the current form into the u8g2 buffer. does not clear or send.
static mp_obj_t mui_draw(void) {
    mui_check_ready();
    mui_Draw(&mui_ui);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mui_draw_obj, mui_draw);

// form_id(): current form, -1 if no form is active
static mp_obj_t mui_form_id(void) {
    if (!mui_is_ready()) {
        return MP_OBJ_NEW_SMALL_INT(-1);
    }
    return MP_OBJ_NEW_SMALL_INT(mui_GetCurrentFormId(&mui_ui));
}
static MP_DEFINE_CONST_FUN_OBJ_0(mui_form_id_obj, mui_form_id);

static mp_obj_t mui_is_active(void) {
    return mp_obj_new_bool(mui_is_ready() && mui_IsFormActive(&mui_ui));
}
static MP_DEFINE_CONST_FUN_OBJ_0(mui_is_active_obj, mui_is_active);

/******************************************************************************/
// fds builders: return bytes, concatenate with +

static uint8_t mui_byte(mp_obj_t arg) {
    return mui_arg_range(arg, 0, 255, MP_QSTR_byte);
}

static void mui_add_id(vstr_t *vstr, mp_obj_t id_in) {
    size_t len;
    const char *id = mp_obj_str_get_data(id_in, &len);
    if (len != 2) {
        mp_raise_ValueError(MP_ERROR_TEXT("id is 2 chars"));
    }
    vstr_add_strn(vstr, id, 2);
}

// text between 0xff delimiters. str is utf-8, which never contains 0xff.
static void mui_add_text(vstr_t *vstr, mp_obj_t text_in) {
    if (!mp_obj_is_str(text_in)) {
        mp_raise_TypeError(MP_ERROR_TEXT("text must be str"));
    }
    size_t len;
    const char *text = mp_obj_str_get_data(text_in, &len);
    // mui keeps at most MUI_MAX_TEXT_LEN bytes of the text, and of each '|' separated token
    size_t token_len = 0;
    for (size_t i = 0; i < len; i++) {
        token_len = text[i] == '|' ? 0 : token_len + 1;
        if (token_len > MUI_MAX_TEXT_LEN) {
            mp_raise_msg_varg(&mp_type_ValueError, MP_ERROR_TEXT("text: at most %d bytes"), (int)MUI_MAX_TEXT_LEN);
        }
    }
    vstr_add_byte(vstr, 0xff);
    vstr_add_strn(vstr, text, len);
    vstr_add_byte(vstr, 0xff);
}

static mp_obj_t mui_fds_done(vstr_t *vstr) {
    return mp_obj_new_bytes_from_vstr(vstr);
}

// form(n): MUI_FORM(n)
static mp_obj_t mui_fds_form(mp_obj_t n) {
    vstr_t vstr;
    vstr_init(&vstr, 2);
    vstr_add_byte(&vstr, 'U');
    vstr_add_byte(&vstr, mui_byte(n));
    return mui_fds_done(&vstr);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mui_fds_form_obj, mui_fds_form);

// style(n): MUI_STYLE(n), n = 0..9
static mp_obj_t mui_fds_style(mp_obj_t n) {
    vstr_t vstr;
    vstr_init(&vstr, 2);
    vstr_add_byte(&vstr, 'S');
    vstr_add_byte(&vstr, '0' + mui_arg_range(n, 0, 9, MP_QSTR_style));
    return mui_fds_done(&vstr);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mui_fds_style_obj, mui_fds_style);

// aux(id): MUI_AUX(id)
static mp_obj_t mui_fds_aux(mp_obj_t id) {
    vstr_t vstr;
    vstr_init(&vstr, 3);
    vstr_add_byte(&vstr, 'Z');
    mui_add_id(&vstr, id);
    return mui_fds_done(&vstr);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mui_fds_aux_obj, mui_fds_aux);

// data(id, text): MUI_DATA(id, text)
static mp_obj_t mui_fds_data(mp_obj_t id, mp_obj_t text) {
    vstr_t vstr;
    vstr_init(&vstr, 16);
    vstr_add_byte(&vstr, 'D');
    mui_add_id(&vstr, id);
    mui_add_text(&vstr, text);
    return mui_fds_done(&vstr);
}
static MP_DEFINE_CONST_FUN_OBJ_2(mui_fds_data_obj, mui_fds_data);

// xy(id, x, y): MUI_XY(id, x, y)
static mp_obj_t mui_fds_xy(mp_obj_t id, mp_obj_t x, mp_obj_t y) {
    vstr_t vstr;
    vstr_init(&vstr, 5);
    vstr_add_byte(&vstr, 'F');
    mui_add_id(&vstr, id);
    vstr_add_byte(&vstr, mui_byte(x));
    vstr_add_byte(&vstr, mui_byte(y));
    return mui_fds_done(&vstr);
}
static MP_DEFINE_CONST_FUN_OBJ_3(mui_fds_xy_obj, mui_fds_xy);

// xyt(id, x, y, text): MUI_XYT(id, x, y, text)
static mp_obj_t mui_fds_xyt(size_t n_args, const mp_obj_t *args) {
    vstr_t vstr;
    vstr_init(&vstr, 16);
    vstr_add_byte(&vstr, 'B');
    mui_add_id(&vstr, args[0]);
    vstr_add_byte(&vstr, mui_byte(args[1]));
    vstr_add_byte(&vstr, mui_byte(args[2]));
    mui_add_text(&vstr, args[3]);
    return mui_fds_done(&vstr);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mui_fds_xyt_obj, 4, 4, mui_fds_xyt);

// xya(id, x, y, a): MUI_XYA(id, x, y, a)
static mp_obj_t mui_fds_xya(size_t n_args, const mp_obj_t *args) {
    vstr_t vstr;
    vstr_init(&vstr, 6);
    vstr_add_byte(&vstr, 'A');
    mui_add_id(&vstr, args[0]);
    vstr_add_byte(&vstr, mui_byte(args[1]));
    vstr_add_byte(&vstr, mui_byte(args[2]));
    vstr_add_byte(&vstr, mui_byte(args[3]));
    return mui_fds_done(&vstr);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mui_fds_xya_obj, 4, 4, mui_fds_xya);

// xyat(id, x, y, a, text): MUI_XYAT(id, x, y, a, text)
static mp_obj_t mui_fds_xyat(size_t n_args, const mp_obj_t *args) {
    vstr_t vstr;
    vstr_init(&vstr, 16);
    vstr_add_byte(&vstr, 'T');
    mui_add_id(&vstr, args[0]);
    vstr_add_byte(&vstr, mui_byte(args[1]));
    vstr_add_byte(&vstr, mui_byte(args[2]));
    vstr_add_byte(&vstr, mui_byte(args[3]));
    mui_add_text(&vstr, args[4]);
    return mui_fds_done(&vstr);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mui_fds_xyat_obj, 5, 5, mui_fds_xyat);

// label(x, y, text): MUI_LABEL(x, y, text)
static mp_obj_t mui_fds_label(mp_obj_t x, mp_obj_t y, mp_obj_t text) {
    vstr_t vstr;
    vstr_init(&vstr, 16);
    vstr_add_byte(&vstr, 'L');
    vstr_add_byte(&vstr, mui_byte(x));
    vstr_add_byte(&vstr, mui_byte(y));
    mui_add_text(&vstr, text);
    return mui_fds_done(&vstr);
}
static MP_DEFINE_CONST_FUN_OBJ_3(mui_fds_label_obj, mui_fds_label);

// goto(x, y, form, text, alt=False): MUI_GOTO(x, y, form, text), alt=True: MUI_goto.
// like MUI_GOTO, but its field has the id ".g" instead of ".G", so a form can
// have two kinds of goto buttons.
static mp_obj_t mui_fds_goto(size_t n_args, const mp_obj_t *args) {
    vstr_t vstr;
    vstr_init(&vstr, 16);
    vstr_add_byte(&vstr, n_args > 4 && mp_obj_is_true(args[4]) ? 'g' : 'G');
    vstr_add_byte(&vstr, mui_byte(args[0]));
    vstr_add_byte(&vstr, mui_byte(args[1]));
    vstr_add_byte(&vstr, mui_byte(args[2]));
    mui_add_text(&vstr, args[3]);
    return mui_fds_done(&vstr);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mui_fds_goto_obj, 4, 5, mui_fds_goto);

/******************************************************************************/

static const mp_rom_map_elem_t mui_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_mui) },
    { MP_ROM_QSTR(MP_QSTR_Field), MP_ROM_PTR(&mui_field_type) },

    // menu
    { MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&mui_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_goto_form), MP_ROM_PTR(&mui_goto_form_obj) },
    { MP_ROM_QSTR(MP_QSTR_next), MP_ROM_PTR(&mui_next_obj) },
    { MP_ROM_QSTR(MP_QSTR_prev), MP_ROM_PTR(&mui_prev_obj) },
    { MP_ROM_QSTR(MP_QSTR_select), MP_ROM_PTR(&mui_select_obj) },
    { MP_ROM_QSTR(MP_QSTR_select_exec), MP_ROM_PTR(&mui_select_exec_obj) },
    { MP_ROM_QSTR(MP_QSTR_inc), MP_ROM_PTR(&mui_inc_obj) },
    { MP_ROM_QSTR(MP_QSTR_dec), MP_ROM_PTR(&mui_dec_obj) },
    { MP_ROM_QSTR(MP_QSTR_leave), MP_ROM_PTR(&mui_leave_obj) },
    { MP_ROM_QSTR(MP_QSTR_cursor), MP_ROM_PTR(&mui_cursor_obj) },
    { MP_ROM_QSTR(MP_QSTR_save_form), MP_ROM_PTR(&mui_save_form_obj) },
    { MP_ROM_QSTR(MP_QSTR_restore_form), MP_ROM_PTR(&mui_restore_form_obj) },
    { MP_ROM_QSTR(MP_QSTR_save_cursor), MP_ROM_PTR(&mui_save_cursor_obj) },
    { MP_ROM_QSTR(MP_QSTR_goto_form_auto), MP_ROM_PTR(&mui_goto_form_auto_obj) },
    { MP_ROM_QSTR(MP_QSTR_draw), MP_ROM_PTR(&mui_draw_obj) },
    { MP_ROM_QSTR(MP_QSTR_form_id), MP_ROM_PTR(&mui_form_id_obj) },
    { MP_ROM_QSTR(MP_QSTR_is_active), MP_ROM_PTR(&mui_is_active_obj) },

    // fds builders
    { MP_ROM_QSTR(MP_QSTR_form), MP_ROM_PTR(&mui_fds_form_obj) },
    { MP_ROM_QSTR(MP_QSTR_style), MP_ROM_PTR(&mui_fds_style_obj) },
    { MP_ROM_QSTR(MP_QSTR_aux), MP_ROM_PTR(&mui_fds_aux_obj) },
    { MP_ROM_QSTR(MP_QSTR_data), MP_ROM_PTR(&mui_fds_data_obj) },
    { MP_ROM_QSTR(MP_QSTR_xy), MP_ROM_PTR(&mui_fds_xy_obj) },
    { MP_ROM_QSTR(MP_QSTR_xyt), MP_ROM_PTR(&mui_fds_xyt_obj) },
    { MP_ROM_QSTR(MP_QSTR_xya), MP_ROM_PTR(&mui_fds_xya_obj) },
    { MP_ROM_QSTR(MP_QSTR_xyat), MP_ROM_PTR(&mui_fds_xyat_obj) },
    { MP_ROM_QSTR(MP_QSTR_label), MP_ROM_PTR(&mui_fds_label_obj) },
    { MP_ROM_QSTR(MP_QSTR_goto), MP_ROM_PTR(&mui_fds_goto_obj) },

    // bar flags
    { MP_ROM_QSTR(MP_QSTR_MMS_2X_BAR), MP_ROM_INT(MUI_MMS_2X_BAR) },
    { MP_ROM_QSTR(MP_QSTR_MMS_4X_BAR), MP_ROM_INT(MUI_MMS_4X_BAR) },
    { MP_ROM_QSTR(MP_QSTR_MMS_SHOW_VALUE), MP_ROM_INT(MUI_MMS_SHOW_VALUE) },
    { MP_ROM_QSTR(MP_QSTR_MMS_NO_WRAP), MP_ROM_INT(MUI_MMS_NO_WRAP) },

    // widgets
    #define W(name, cb, kind, cflags) { MP_ROM_QSTR(MP_QSTR_##name), MP_ROM_INT(WIDGET_##name) },
    WIDGETS
    #undef W
};
static MP_DEFINE_CONST_DICT(mui_module_globals, mui_module_globals_table);

const mp_obj_module_t mui_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&mui_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_mui, mui_user_cmodule);
