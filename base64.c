#include <stdlib.h>
#include <string.h>

#include "base64.h"

static const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void
base64_generate_decode_table()
{
	int inv[80];

	memset(inv, -1, sizeof(inv));
	for (size_t i = 0; i < sizeof(base64_chars) - 1; i++) {
		inv[base64_chars[i]-43] = i;
	}
}

static int
base64_is_validchar(char c)
{
	if (c >= '0' && c <= '9') {
		return 1;
    }
	if (c >= 'A' && c <= 'Z') {
		return 1;
    }
	if (c >= 'a' && c <= 'z') {
		return 1;
    }
	if (c == '+' || c == '/' || c == '=') {
		return 1;
    }

	return 0;
}

size_t
base64_decoded_size(const char *in)
{
	if (in == NULL) {
		return 0;
    }

	size_t len = strlen(in);
	size_t ret = len / 4 * 3;

	for (size_t i = len; i-- > 0;) {
		if (in[i] == '=') {
			ret--;
		} else {
			break;
		}
	}

	return ret;
}

static const int base64_invs[] = {
    62, -1, -1, -1, 63, 52, 53, 54, 55, 56, 57, 58,
	59, 60, 61, -1, -1, -1, -1, -1, -1, -1, 0, 1, 2, 3, 4, 5,
	6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
	21, 22, 23, 24, 25, -1, -1, -1, -1, -1, -1, 26, 27, 28,
	29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42,
	43, 44, 45, 46, 47, 48, 49, 50, 51
};

static size_t
base64_encoded_size(size_t in_len)
{
	size_t ret = in_len;
	if (in_len % 3 != 0) {
		ret += 3 - (in_len % 3);
    }

	ret /= 3;
	ret *= 4;

	return ret;
}

char*
base64_encode(const unsigned char *in, size_t len)
{
	if (in == NULL || len == 0) {
        return NULL;
    }

	size_t elen = base64_encoded_size(len);
	char *out = malloc(elen+1);
	out[elen] = '\0';

    size_t i, j;
	size_t v;
	for (i = 0, j = 0; i < len; i += 3, j += 4) {
		v = in[i];
		v = i+1 < len ? v << 8 | in[i+1] : v << 8;
		v = i+2 < len ? v << 8 | in[i+2] : v << 8;

		out[j] = base64_chars[(v >> 18) & 0x3F];
		out[j+1] = base64_chars[(v >> 12) & 0x3F];

		if (i+1 < len) {
			out[j+2] = base64_chars[(v >> 6) & 0x3F];
		} else {
			out[j+2] = '=';
		}
		if (i+2 < len) {
			out[j+3] = base64_chars[v & 0x3F];
		} else {
			out[j+3] = '=';
		}
	}

	return out;
}

int
base64_decode(const char *in, unsigned char *out, size_t out_len)
{
	if (in == NULL || out == NULL) {
		return 0;
    }

	size_t len = strlen(in);
	if (out_len < base64_decoded_size(in) || len % 4 != 0) {
		return 0;
    }

    size_t i;
	for (i = 0; i < len; i++) {
		if (!base64_is_validchar(in[i])) {
			return 0;
		}
	}

    
	size_t j;
	int v;

	for (i = 0, j = 0; i < len; i += 4, j += 3) {
		v = base64_invs[in[i]-43];
		v = (v << 6) | base64_invs[in[i+1]-43];
		v = in[i+2]=='=' ? v << 6 : (v << 6) | base64_invs[in[i+2]-43];
		v = in[i+3]=='=' ? v << 6 : (v << 6) | base64_invs[in[i+3]-43];

		out[j] = (v >> 16) & 0xFF;
		if (in[i+2] != '=') {
			out[j+1] = (v >> 8) & 0xFF;
        }
		if (in[i+3] != '=') {
			out[j+2] = v & 0xFF;
        }
	}

	return 1;
}
