#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>

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

#ifdef JASOMAN_IMPL

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

#endif

