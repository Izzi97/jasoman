#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define STACK_INIT_CAP 64

void
panic(char *reason) {
	fprintf(stderr, reason);
	exit(EXIT_FAILURE);
}

void
panic_mem() {
	panic("memory allocation failed");
}

void*
safe_malloc(size_t s) {
	void *p = malloc(s);
	if (!p) panic_mem();
	return p;
}
#define malloc(s) safe_malloc(s)

void*
safe_calloc(size_t n, size_t s) {
	void *p = calloc(n, s);
	if (!p) panic_mem();
	return p;
}
#define calloc(n, s) safe_calloc(n, s)

typedef enum {
	FAILURE,
	EOI,
	SUCCESS,
	STATUS_COUNT
} status_t;

typedef enum {
	TT_EMPTY,
	TT_NULL,
	TT_BOOLEAN,
	TT_NUMBER,
	TT_STRING,
	TT_COMMA,
	TT_ARRAY_START,
	TT_ARRAY_END,
	TT_OBJECT_START,
	TT_COLON,
	TT_OBJECT_END,
	TT_COUNT
} token_type_t;

typedef struct {
	token_type_t	type;
	union {
		bool	boolean;
		double  number;
		char	*string;
	} value;
} token_t;

typedef struct {
	status_t status;
	size_t position;
	union {
		token_t token;
		char *reason;
	};
} tokenizer_result_t;

char * tt_name(token_type_t tt);
tokenizer_result_t next_token(char *input, size_t pos, size_t cap);



typedef enum {
	VT_NULL,
	VT_BOOLEAN,
	VT_NUMBER,
	VT_STRING,
	VT_ARRAY,
	VT_OBJECT,
	VT_COUNT
} value_type_t;

typedef struct array array_t;
typedef struct object object_t;

typedef struct {
	value_type_t type;
	union {
		bool 		boolean;
		double 		number;
		char 		*string;
		array_t 	*array;
		object_t 	*object;
	} data;
} value_t;

typedef struct array_element array_element_t;
typedef struct array_element {
	array_element_t *next;
	value_t *value;
} array_element_t;

typedef struct array {
	array_element_t *first;
	array_element_t *last;
} array_t;

typedef struct object_element object_element_t;
typedef struct object_element {
	object_element_t *next;
	char *key;
	value_t *value;
} object_element_t;

typedef struct object {
	object_element_t *first;
	object_element_t *last;
} object_t;

value_t*
value_new(value_type_t t) {
	value_t *v = malloc(sizeof(value_t));
	if (!v) panic_mem();
	v->type = t;
	return v;
}

value_t*
v_null_new() {
	return value_new(VT_NULL);
}

value_t*
v_bool_new(bool data) {
	value_t *v = value_new(VT_BOOLEAN);
	v->data.boolean = data;
	return v;
}

value_t*
v_number_new(double data) {
	value_t *v = value_new(VT_NUMBER);
	v->data.number = data;
	return v;
}

value_t*
v_string_new(char *data) {
	value_t *v = value_new(VT_STRING);
	v->data.string = data;
	return v;
}

value_t*
v_array_new() {
	value_t *v = value_new(VT_ARRAY);
	v->data.array = calloc(1, sizeof(array_t));
	if (!v->data.array) panic_mem();
	return v;
}

void
v_array_append(array_t *a, value_t *v) {
	if (!a || !v) return;

	array_element_t *ae = calloc(1, sizeof(array_element_t));
	ae->value = v;

	if (!a->last) a->first = a->last = ae;
	else a->last = a->last->next = ae;
}

value_t*
v_object_new() {
	value_t *v = value_new(VT_OBJECT);
	v->data.object = calloc(1, sizeof(object_t));
	if (!v->data.object) panic_mem();
	return v;
}

void
v_object_append(object_t *o) {
	if (!o) return;

	object_element_t *oe = calloc(1, sizeof(object_element_t));
	if (!o->last) o->first = o->last = oe;
	else o->last = o->last->next = oe;
}

void
v_free(value_t *v) {
	if (!v) return;
	switch (v->type) {
		case VT_STRING:
			free(v->data.string);
			break;
		case VT_ARRAY:
			array_t *a = v->data.array;
			array_element_t *ae_cur = a->first, *ae_next;
			while (ae_cur) {
				ae_next = ae_cur->next;
				v_free(ae_cur->value);
				free(ae_cur);
				ae_cur = ae_next;
			}
			free(a);
			break;
		case VT_OBJECT:
			object_t *o = v->data.object;
			object_element_t *oe_cur = o->first, *oe_next;
			while (oe_cur)  {
				oe_next = oe_cur->next;
				v_free(oe_cur->value);
				free(oe_cur);
				oe_cur = oe_next;
			}
			free(o);
			break;
		default:
	}
	free(v);
}



typedef struct {
	status_t status;
	size_t position;
	union {
		value_t *value;
		char *reason;
	};
} parser_result_t;

parser_result_t parse(char *input);



#ifdef JASOMAN_IMPL

// Tokenizer

char *
tt_name(token_type_t tt) {
	switch (tt) {
	case TT_EMPTY: return "''";
	case TT_NULL: return "null";
	case TT_BOOLEAN: return "bool";
	case TT_NUMBER: return "number";
	case TT_STRING: return "string";
	case TT_COMMA: return ",";
	case TT_ARRAY_START: return "[";
	case TT_ARRAY_END: return "]";
	case TT_OBJECT_START: return "{";
	case TT_COLON: return ":";
	case TT_OBJECT_END: return "}";
	default: return "invalid";
	}
}

tokenizer_result_t
t_res_succ(size_t pos, token_t tok) {
	tokenizer_result_t r = {0};
	r.status = SUCCESS;
	r.position = pos;
	r.token = tok;
	return r;
}

tokenizer_result_t
t_res_eoi(size_t pos, token_t tok) {
	tokenizer_result_t r = {0};
	r.status = EOI;
	r.position = pos;
	r.token = tok;
	return r;
}

tokenizer_result_t
t_res_fail(size_t pos, char *reason) {
	tokenizer_result_t r = {0};
	r.status = FAILURE;
	r.position = pos;
	r.reason = reason;
	return r;
}

bool
any_of(char c, char *cs) {
	for (size_t i = 0; cs[i] != 0; i++) {
		if (c == cs[i]) return true;
	}
	return false;
}

typedef bool (*tokenizer_predicate_t)(char);

tokenizer_result_t
consume_by_predicate(tokenizer_predicate_t pred, char *input, size_t pos, size_t cap) {
	if (!input) return t_res_fail(pos, "invalid input buffer");
	if (pos > cap) return t_res_fail(pos, "input buffer position exceeds capacity");
	if (pos == cap) return t_res_eoi(pos, (token_t){ .type = TT_EMPTY });
	if (!pred(input[pos])) return t_res_fail(pos, "character predicate mismatch");
	// hacky ...
	else return t_res_succ(pos+1, (token_t){ .type = TT_EMPTY });
}

tokenizer_result_t
consume_literal_char(char c, char *input, size_t pos, size_t cap) {
	if (!input) return t_res_fail(pos, "invalid input buffer");
	if (pos > cap) return t_res_fail(pos, "input buffer position exceeds capacity");
	if (pos == cap) return t_res_eoi(pos, (token_t){ .type = TT_EMPTY });
	if (input[pos] != c) return t_res_fail(pos, "unexpected input character");
	// hacky ...
	else return t_res_succ(pos+1, (token_t){ .type = TT_EMPTY });
}

tokenizer_result_t
consume_literal_string(char *string, char *input, size_t pos, size_t cap) {
	// input sanitation only on lowest level = consume_literal_char
	tokenizer_result_t r = {0};
	for (size_t i = 0; string[i] != 0; i++) {
		r = consume_literal_char(string[i], input, pos, cap);
		if (r.status != SUCCESS) return r;
		else pos = r.position;
	}
	return r;
}

tokenizer_result_t
consume_null(char *input, size_t pos, size_t cap) {
	tokenizer_result_t r = consume_literal_string("null", input, pos, cap);
	if (r.status == SUCCESS) r.token.type = TT_NULL;
	return r;
}

tokenizer_result_t
consume_true(char *input, size_t pos, size_t cap) {
	tokenizer_result_t r = consume_literal_string("true", input, pos, cap);
	if (r.status == SUCCESS) {
		r.token.type = TT_BOOLEAN;
		r.token.value.boolean = true;
	}
	return r;
}

tokenizer_result_t
consume_false(char *input, size_t pos, size_t cap) {
	tokenizer_result_t r = consume_literal_string("false", input, pos, cap);
	if (r.status == SUCCESS) {
		r.token.type = TT_BOOLEAN;
		r.token.value.boolean = false;
	}
	return r;
}

bool
is_hex_char(char c) {
	return ('0' <= c && c <= '9') || ('a' <= c && c <= 'f') || ('A' <= c && c <= 'F');
}

bool
is_escaped_char(char c) {
	return any_of(c, "\"\\/bfnrt");
}

bool
is_regular_char(char c) {
	return c != '"' && c != '\\';
}

tokenizer_result_t
consume_escape_sequence(char *input, size_t pos, size_t cap) {
	// backslash
	tokenizer_result_t r = consume_literal_char('\\', input, pos, cap);
	if (r.status != SUCCESS) return t_res_fail(r.position, "unexpected end of input");
	size_t after_backslash = r.position;

	// four hex chars
	for (size_t i = 0; i<4; i++) {
		r = consume_by_predicate(is_hex_char, input, r.position, cap);
		if (r.status == EOI) return t_res_fail(r.position, "unexpected end of input");
	}
	if (r.status == SUCCESS) return r;

	// control sequence
	return consume_by_predicate(is_escaped_char, input, after_backslash, cap);
}

tokenizer_result_t
consume_string(char *input, size_t pos, size_t cap) {
	size_t cursor = 0;
	tokenizer_result_t r = consume_literal_char('"', input, pos, cap);
	if (r.status != SUCCESS) return r;
	else cursor = r.position;

	size_t begin = cursor, end = 0;
	for (;;) {
		r = consume_literal_char('"', input, cursor, cap);
		if (r.status == SUCCESS) {
			end = cursor;
			break;
		}
		if (r.status == EOI) goto unexpected_eoi;

		r = consume_escape_sequence(input, cursor, cap);
		if (r.status == SUCCESS) cursor = r.position;
		if (r.status == EOI) goto unexpected_eoi;

		r = consume_by_predicate(is_regular_char, input, cursor, cap);
		if (r.status == SUCCESS) cursor = r.position;
		if (r.status == EOI) goto unexpected_eoi;
	}

	size_t length = end - begin;
	// possibly non-portable behavior when length == 0, cf. man malloc
	char *string = malloc(length);
	if (!string) {
		fprintf(stderr, "memory allocation for JSON string failed");
		exit(1);
	}
	for (size_t i = 0; i < length; i++) string[i] = input[begin+i];
	r.token = (token_t) {
		.type = TT_STRING,
		.value.string = string
	};
	return r;

unexpected_eoi:
	return t_res_fail(r.position, "unexpected end of input");
}

tokenizer_result_t
consume_number(char *input, size_t pos, size_t cap) {
	char *num_begin = input + pos;
	char *num_end = NULL;
	double num = strtod(num_begin, &num_end);
	if (num_end == num_begin) return t_res_fail(pos, "invalid number literal");
	if (num_end > input + cap) return t_res_fail(cap, "unexpected end of input");
	return t_res_succ(num_end - input, (token_t){ .type = TT_NUMBER, .value.number = num });
}

tokenizer_result_t
next_token(char *input, size_t pos, size_t cap) {
	if (!input) return t_res_fail(pos, "invalid input buffer");
	if (pos == cap) return t_res_eoi(pos, (token_t){ .type = TT_EMPTY });
	if (pos > cap) return t_res_fail(pos, "input buffer position exceeds capacity");

	token_t tok = {0};
	tokenizer_result_t res = {0};

	for (; any_of(input[pos], "\t\n\r "); pos++) {
		if (pos >= cap) {
			tok.type = TT_EMPTY;
			return t_res_eoi(pos, tok);
		}
	}

	switch (input[pos]) {
	case '[':
		tok.type = TT_ARRAY_START;
		res = t_res_succ(pos+1, tok);
		break;
	case ']':
		tok.type = TT_ARRAY_END;
		res = t_res_succ(pos+1, tok);
		break;
	case '{':
		tok.type = TT_OBJECT_START;
		res = t_res_succ(pos+1, tok);
		break;
	case '}':
		tok.type = TT_OBJECT_END;
		res = t_res_succ(pos+1, tok);
		break;
	case ',':
		tok.type = TT_COMMA;
		res = t_res_succ(pos+1, tok);
		break;
	case ':':
		tok.type = TT_COLON;
		res = t_res_succ(pos+1, tok);
		break;
	case 'n':
		res = consume_null(input, pos, cap);
		break;
	case 't':
		res = consume_true(input, pos, cap);
		break;
	case 'f':
		res = consume_false(input, pos, cap);
		break;
	case '"':
		res = consume_string(input, pos, cap);
		break;
	case '-':
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		res = consume_number(input, pos, cap);
		break;
	default:
		res = t_res_fail(pos, "invalid input character");
	}

	return res;
}


// Parser

typedef enum {
	S_INIT,
	S_ARR_OPEN,
	S_ARR_ITEM,
	S_ARR_COMMA,
	S_ARR_CLOSE,
	S_OBJ_OPEN,
	S_OBJ_KVP_LHS,
	S_OBJ_KVP_COLON,
	S_OBJ_KVP_RHS,
	S_OBJ_COMMA,
	S_OBJ_CLOSE,
	S_COUNT
} state_t;

typedef struct {
	state_t *data;
	size_t capacity;
	size_t count;
} state_stack_t;

state_stack_t
ss_new() {
	state_t *data = malloc(STACK_INIT_CAP * sizeof(state_t));
	if (!data) panic_mem();

	state_stack_t ss = {0};
	ss.data = data;
	ss.capacity = STACK_INIT_CAP;
	return ss;
}

state_t*
ss_peek(state_stack_t *ss) {
	if (!ss->count) return NULL;
	else return &(ss->data[ss->count-1]);
}

void
ss_push(state_stack_t *ss, state_t s){
	if (ss->count == ss->capacity) {
		state_t *bigger_data = realloc(ss->data, 2 * ss->capacity * sizeof(state_t));
		if (!bigger_data) panic_mem();
		ss->data = bigger_data;
	}

	ss->data[ss->count++] = s;
}

void
ss_pop(state_stack_t *ss) {
	if (ss->count == 0) return;
	else ss->count -= 1;
}

void
ss_free(state_stack_t *ss) {
	free(ss->data);
}

typedef struct {
	value_t **data;
	size_t count;
	size_t capacity;
} value_stack_t;

value_stack_t
vs_new() {
	value_t **data = malloc(STACK_INIT_CAP * sizeof(value_t*));
	if (!data) panic_mem();

	value_stack_t vs = {0};
	vs.data = data;
	vs.capacity = STACK_INIT_CAP;
	return vs;
}

value_t*
vs_peek(value_stack_t *vs) {
	if (!vs->count) return NULL;
	else return vs->data[vs->count-1];
}

void
vs_push(value_stack_t *vs, value_t *s){
	if (vs->count == vs->capacity) {
		value_t **bigger_data = realloc(vs->data, 2 * vs->capacity * sizeof(value_t*));
		if (!bigger_data) panic_mem();
		vs->data = bigger_data;
	}

	vs->data[vs->count++] = s;
}

void
vs_pop(value_stack_t *vs) {
	if (vs->count == 0) return;
	else vs->count -= 1;
}

void
vs_free(value_stack_t *vs) {
	free(vs->data);
}


parser_result_t
p_res_succ(value_t *v) {
	return (parser_result_t){
		.status = SUCCESS,
		.value = v
	};
}

typedef parser_result_t (*handler_t)(token_t t, state_stack_t *ss, value_stack_t *vs);

parser_result_t
invalid_transition(token_t t, state_stack_t *ss, value_stack_t *vs) {
	(void) t;
	(void) ss;
	(void) vs;
	return (parser_result_t) {
		.status = FAILURE,
		.reason = "unexpected token"
	};
}

parser_result_t
h_toplevel_null(token_t t, state_stack_t *ss, value_stack_t *vs) {
	(void)t;
	ss_pop(ss);

	value_t *v = v_null_new();
	vs_push(vs, v);
	return p_res_succ(v);
}

parser_result_t
h_toplevel_bool(token_t t, state_stack_t *ss, value_stack_t *vs) {
	ss_pop(ss);

	value_t *v = v_bool_new(t.value.boolean);
	vs_push(vs, v);
	return p_res_succ(v);
}

parser_result_t
h_toplevel_number(token_t t, state_stack_t *ss, value_stack_t *vs) {
	ss_pop(ss);

	value_t *v = v_number_new(t.value.number);
	vs_push(vs, v);
	return p_res_succ(v);
}

parser_result_t
h_toplevel_string(token_t t, state_stack_t *ss, value_stack_t *vs) {
	ss_pop(ss);

	value_t *v = v_string_new(t.value.string);
	vs_push(vs, v);
	return p_res_succ(v);
}

parser_result_t
h_toplevel_array(token_t t, state_stack_t *ss, value_stack_t *vs) {
	(void)t;
	ss_pop(ss);
	ss_push(ss, S_ARR_OPEN);

	value_t *v = v_array_new();
	vs_push(vs, v);
	return p_res_succ(v);
}

parser_result_t
h_toplevel_object(token_t t, state_stack_t *ss, value_stack_t *vs) {
	(void)t;
	ss_pop(ss);
	ss_push(ss, S_OBJ_OPEN);

	value_t *v = v_object_new();
	vs_push(vs, v);
	return p_res_succ(v);
}

parser_result_t
h_composite_close(token_t t, state_stack_t *ss, value_stack_t *vs) {
	(void)t;
	ss_pop(ss);
	state_t *s = ss_peek(ss);
	if (!s) return p_res_succ(vs_peek(vs));

	value_t *v;
	switch (*s) {
		case S_ARR_COMMA:
			ss_pop(ss);
			ss_push(ss, S_ARR_ITEM);
			v = vs_peek(vs);
			vs_pop(vs);
			value_t *va = vs_peek(vs);
			v_array_append(va->data.array, v);
			return p_res_succ(v);
		case S_OBJ_KVP_COLON:
			ss_pop(ss);
			ss_push(ss, S_OBJ_KVP_RHS);
			v = vs_peek(vs);
			vs_pop(vs);
			value_t *vo = vs_peek(vs);
			vo->data.object->last->value = v;
			return p_res_succ(v);
		default:
			return (parser_result_t){
				.status = FAILURE,
				.reason = "unexpected state"
			};
	}
}

parser_result_t
h_arrappend_null(token_t t, state_stack_t *ss, value_stack_t *vs) {
	(void)t;
	ss_pop(ss);
	ss_push(ss, S_ARR_ITEM);

	value_t *v = v_null_new();
	array_t *a = vs_peek(vs)->data.array;
	v_array_append(a, v);
	return p_res_succ(v);
}

parser_result_t
h_arrappend_boolean(token_t t, state_stack_t *ss, value_stack_t *vs) {
	ss_pop(ss);
	ss_push(ss, S_ARR_ITEM);

	value_t *v = v_bool_new(t.value.boolean);
	array_t *a = vs_peek(vs)->data.array;
	v_array_append(a, v);
	return p_res_succ(v);
}

parser_result_t
h_arrappend_number(token_t t, state_stack_t *ss, value_stack_t *vs) {
	ss_pop(ss);
	ss_push(ss, S_ARR_ITEM);

	value_t *v = v_number_new(t.value.number);
	array_t *a = vs_peek(vs)->data.array;
	v_array_append(a, v);
	return p_res_succ(v);
}

parser_result_t
h_arrappend_string(token_t t, state_stack_t *ss, value_stack_t *vs) {
	ss_pop(ss);
	ss_push(ss, S_ARR_ITEM);

	value_t *v = v_string_new(t.value.string);
	array_t *a = vs_peek(vs)->data.array;
	v_array_append(a, v);
	return p_res_succ(v);
}

parser_result_t
h_arrappend_arrstart(token_t t, state_stack_t *ss, value_stack_t *vs) {
	(void)t;
	ss_pop(ss);
	ss_push(ss, S_ARR_ITEM);
	ss_push(ss, S_ARR_OPEN);

	value_t *v = v_array_new();
	array_t *a = vs_peek(vs)->data.array;
	v_array_append(a, v);
	vs_push(vs, v);
	return p_res_succ(v);
}

parser_result_t
h_arrappend_objstart(token_t t, state_stack_t *ss, value_stack_t *vs) {
	(void)t;
	ss_pop(ss);
	ss_push(ss, S_ARR_ITEM);
	ss_push(ss, S_OBJ_OPEN);

	value_t *v = v_object_new();
	array_t *a = vs_peek(vs)->data.array;
	v_array_append(a, v);
	vs_push(vs, v);
	return p_res_succ(v);
}

parser_result_t
h_arritem_comma(token_t t, state_stack_t *ss, value_stack_t *vs) {
	(void)t;
	ss_pop(ss);
	ss_push(ss, S_ARR_COMMA);
	return p_res_succ(vs_peek(vs));
}

parser_result_t
h_kvp_key(token_t t, state_stack_t *ss, value_stack_t *vs) {
	ss_pop(ss);
	ss_push(ss, S_OBJ_KVP_LHS);
	value_t *v = vs_peek(vs);
	object_t *o = v->data.object;
	v_object_append(o);
	o->last->key = t.value.string;
	return p_res_succ(v);
}

parser_result_t
h_kvp_colon(token_t t, state_stack_t *ss, value_stack_t *vs) {
	(void)t;
	(void)vs;
	ss_pop(ss);
	ss_push(ss, S_OBJ_KVP_COLON);
	return p_res_succ(vs_peek(vs));
}

parser_result_t
h_kvp_valuenull(token_t t, state_stack_t *ss, value_stack_t *vs) {
	(void)t;
	ss_pop(ss);
	ss_push(ss, S_OBJ_KVP_RHS);
	value_t *v = vs_peek(vs);
	object_t *o = v->data.object;
	o->last->value = v_null_new();
	return p_res_succ(v);
}

parser_result_t
h_kvp_valueboolean(token_t t, state_stack_t *ss, value_stack_t *vs) {
	ss_pop(ss);
	ss_push(ss, S_OBJ_KVP_RHS);
	value_t *v = vs_peek(vs);
	object_t *o = v->data.object;
	o->last->value = v_bool_new(t.value.boolean);
	return p_res_succ(v);
}

parser_result_t
h_kvp_valuenumber(token_t t, state_stack_t *ss, value_stack_t *vs) {
	ss_pop(ss);
	ss_push(ss, S_OBJ_KVP_RHS);
	value_t *v = vs_peek(vs);
	object_t *o = v->data.object;
	o->last->value = v_number_new(t.value.number);
	return p_res_succ(v);
}

parser_result_t
h_kvp_valuestring(token_t t, state_stack_t *ss, value_stack_t *vs) {
	ss_pop(ss);
	ss_push(ss, S_OBJ_KVP_RHS);
	value_t *v = vs_peek(vs);
	object_t *o = v->data.object;
	o->last->value = v_string_new(t.value.string);
	return p_res_succ(v);
}

parser_result_t
h_kvp_valuearray(token_t t, state_stack_t *ss, value_stack_t *vs) {
	(void)t;
	ss_push(ss, S_ARR_OPEN);
	value_t *vo = vs_peek(vs);
	object_t *o = vo->data.object;
	value_t *va = v_array_new();
	o->last->value = va;
	vs_push(vs, va);
	return p_res_succ(va);
}

parser_result_t
h_kvp_valueobject(token_t t, state_stack_t *ss, value_stack_t *vs) {
	(void)t;
	ss_push(ss, S_OBJ_OPEN);
	value_t *vo = vs_peek(vs);
	object_t *o = vo->data.object;
	value_t *v = v_object_new();
	o->last->value = v;
	vs_push(vs, v);
	return p_res_succ(v);
}

parser_result_t
h_object_comma(token_t t, state_stack_t *ss, value_stack_t *vs) {
	(void)t;
	ss_pop(ss);
	ss_push(ss, S_OBJ_COMMA);
	return p_res_succ(vs_peek(vs));
}

static handler_t parse_table[S_COUNT][TT_COUNT];
static bool parse_table_initialized = false;

void
init_parse_table() {
	if (parse_table_initialized) return;

	for (size_t i = 0; i < S_COUNT; i++) {
		for (size_t j = 0; j < TT_COUNT; j++)
			parse_table[i][j] = invalid_transition;
	}

	parse_table[S_INIT][TT_NULL] = h_toplevel_null;
	parse_table[S_INIT][TT_BOOLEAN] = h_toplevel_bool;
	parse_table[S_INIT][TT_NUMBER] = h_toplevel_number;
	parse_table[S_INIT][TT_STRING] = h_toplevel_string;
	parse_table[S_INIT][TT_ARRAY_START] = h_toplevel_array;
	parse_table[S_INIT][TT_OBJECT_START] = h_toplevel_object;

	parse_table[S_ARR_OPEN][TT_ARRAY_END] = h_composite_close;
	parse_table[S_ARR_OPEN][TT_NULL] = h_arrappend_null;
	parse_table[S_ARR_OPEN][TT_BOOLEAN] = h_arrappend_boolean;
	parse_table[S_ARR_OPEN][TT_NUMBER] = h_arrappend_number;
	parse_table[S_ARR_OPEN][TT_STRING] = h_arrappend_string;
	parse_table[S_ARR_OPEN][TT_ARRAY_START] = h_arrappend_arrstart;
	parse_table[S_ARR_OPEN][TT_OBJECT_START] = h_arrappend_objstart;

	parse_table[S_ARR_ITEM][TT_ARRAY_END] = h_composite_close;
	parse_table[S_ARR_ITEM][TT_COMMA] = h_arritem_comma;

	parse_table[S_ARR_COMMA][TT_NULL] = h_arrappend_null;
	parse_table[S_ARR_COMMA][TT_BOOLEAN] = h_arrappend_boolean;
	parse_table[S_ARR_COMMA][TT_NUMBER] = h_arrappend_number;
	parse_table[S_ARR_COMMA][TT_STRING] = h_arrappend_string;
	parse_table[S_ARR_COMMA][TT_ARRAY_START] = h_arrappend_arrstart;
	parse_table[S_ARR_COMMA][TT_OBJECT_START] = h_arrappend_objstart;

	parse_table[S_OBJ_OPEN][TT_OBJECT_END] = h_composite_close;
	parse_table[S_OBJ_OPEN][TT_STRING] = h_kvp_key;

	parse_table[S_OBJ_KVP_LHS][TT_COLON] = h_kvp_colon;

	parse_table[S_OBJ_KVP_COLON][TT_NULL] = h_kvp_valuenull;
	parse_table[S_OBJ_KVP_COLON][TT_BOOLEAN] = h_kvp_valueboolean;
	parse_table[S_OBJ_KVP_COLON][TT_NUMBER] = h_kvp_valuenumber;
	parse_table[S_OBJ_KVP_COLON][TT_STRING] = h_kvp_valuestring;
	parse_table[S_OBJ_KVP_COLON][TT_ARRAY_START] = h_kvp_valuearray;
	parse_table[S_OBJ_KVP_COLON][TT_OBJECT_START] = h_kvp_valueobject;

	parse_table[S_OBJ_KVP_RHS][TT_OBJECT_END] = h_composite_close;
	parse_table[S_OBJ_KVP_RHS][TT_COMMA] = h_object_comma;

	parse_table[S_OBJ_COMMA][TT_STRING] = h_kvp_key;
}

parser_result_t
parse(char *input) {
	size_t cap = strlen(input);
	tokenizer_result_t tr = {0};

	state_stack_t ss = ss_new();
	ss_push(&ss, S_INIT);
	value_stack_t vs = vs_new();
	state_t s;
	handler_t h;
	parser_result_t pr = {0};

	init_parse_table();

	do {
		tr = next_token(input, tr.position, cap);
		switch (tr.status) {
			case SUCCESS:
				s = *ss_peek(&ss);
				h = parse_table[s][tr.token.type];
				pr = (*h)(tr.token, &ss, &vs);
				break;
			case EOI:
				pr.status = EOI;
				break;
			case FAILURE:
				pr.status = FAILURE;
				pr.reason = tr.reason;
				break;
			default:
				panic("invalid state reached");
		}
		pr.position = tr.position;
	} while (ss_peek(&ss) && pr.status == SUCCESS);

	if (ss_peek(&ss) && pr.status == EOI) {
		pr.status = FAILURE;
		pr.reason = "unexpected end of input";
	}
	return pr;
}


#endif

