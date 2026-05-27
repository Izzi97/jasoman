#include <stdio.h>
#define JASOMAN_IMPL
#include "jasoman.h"

int main() {
	char input[] = "   {	\"foo\": -42.69e3,\n\"bar\": [ null, true, false ]}";
	size_t pos = 0, cap = sizeof(input)-1; // ignore trailing null byte
	tokenizer_result_t tr;
	
	while ((tr = next_token(input, pos, cap)).status == SUCCESS) {
		switch (tr.token.type) {
			case TT_BOOLEAN: {
				printf("%s\n", tr.token.value.boolean ? "true" : "false");
				break;
			}
			case TT_NUMBER: {
				printf("%f\n", tr.token.value.number);
				break;
			}
			case TT_STRING: {
				printf("\"%s\"\n", tr.token.value.string);
				break;
			}
			default: printf("%s\n", tt_name(tr.token.type));
		}
		pos = tr.position;
	}

	switch (tr.status) {
		case SUCCESS: {
			printf("success\n");
			break;
		}
		case FAILURE: {
			printf("failure: %s\n", tr.reason);
			break;
		}
		case EOI: {
			printf("eoi@%ld", tr.position);
			break;
		}
		default: {
			printf("invalid result status: %d", tr.status);
			break;
		}
	}

	return 0;
}

