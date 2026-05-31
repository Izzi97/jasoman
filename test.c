#include <stdio.h>
#define JASOMAN_IMPL
#include "jasoman.h"

void indent(size_t i) {
	for (; i > 0; i--) putchar('\t');
}

void print_value(value_t *v, size_t i, bool from_object) {
	if (!from_object) indent(i);
	switch (v->type) {
		case VT_NULL:
			printf("null");
			break;
		case VT_BOOLEAN:
		     	printf("%s", v->data.boolean ? "true" : "false");
			break;
		case VT_NUMBER:
			printf("%f", v->data.number);
			break;
		case VT_STRING:
			printf("%s", v->data.string);
			break;
		case VT_ARRAY:
			putchar('[');
			for (
				array_element_t *ae = v->data.array->first;
				ae != NULL;
				ae = ae->next
			) {
				putchar('\n');
				indent(i);
				print_value(ae->value, i+1, false);
			}
			putchar('\n');
			indent(i);
			putchar(']');
			break;
		case VT_OBJECT:
			putchar('{');
			for (
				object_element_t *oe = v->data.object->first;
				oe != NULL;
				oe = oe->next
			) {
				putchar('\n');
				indent(i+1);
				printf("%s: ", oe->key);
				print_value(oe->value, i+1, true);
			}
			putchar('\n');
			indent(i);
			putchar('}');
			break;
		default:
			panic("invlid value type");
	}
}

int main() {
	char *input = "   {	\"foo\": -42.69e3,\n\"bar\": [ null, true, false ], \"baz\":{}, \"fred\": []}";
	
	parser_result_t pr = parse(input);
	if (pr.status == FAILURE)
		fprintf(stderr, "failed @%ld: %s", pr.position, pr.reason);
	else
		print_value(pr.value, 0, false);

	v_free(pr.value);
	return 0;
}

