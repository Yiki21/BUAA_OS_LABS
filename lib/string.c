#include <types.h>

char* strcat(char* dest, const char* src) {
	char* p = dest;

    // 找到 dest 的末尾
    while (*p) {
        p++;
    }

    // 复制 src 到末尾
    while (*src) {
        *p++ = *src++;
    }

    *p = '\0';  // 添加字符串结束符

    return dest;
}

char *strncat(char *dest, const char *src, size_t n) {
	char* p = dest;

    // 找到 dest 的末尾
    while (*p) {
        p++;
    }

    // 复制最多 n 个字符
    while (n-- && *src) {
        *p++ = *src++;
    }

    *p = '\0';  // 添加字符串结束符

    return dest;
}

char * strtok(char *str, const char *delim) {
	static char* next = NULL;
    if (str != NULL) {
        next = str;
    }
    if (next == NULL) {
        return NULL;
    }

    // 跳过前缀的分隔符
    char* start = next;
    while (*start && strchr(delim, *start)) {
        start++;
    }

    if (*start == '\0') {
        next = NULL;
        return NULL;
    }

    // 找下一个分隔符
    char* token_end = start;
    while (*token_end && !strchr(delim, *token_end)) {
        token_end++;
    }

    if (*token_end) {
        *token_end = '\0';
        next = token_end + 1;
    } else {
        next = NULL;
    }

    return start;
}

void *memcpy(void *dst, const void *src, size_t n) {
	void *dstaddr = dst;
	void *max = dst + n;

	if (((u_long)src & 3) != ((u_long)dst & 3)) {
		while (dst < max) {
			*(char *)dst++ = *(char *)src++;
		}
		return dstaddr;
	}

	while (((u_long)dst & 3) && dst < max) {
		*(char *)dst++ = *(char *)src++;
	}

	// copy machine words while possible
	while (dst + 4 <= max) {
		*(uint32_t *)dst = *(uint32_t *)src;
		dst += 4;
		src += 4;
	}

	// finish the remaining 0-3 bytes
	while (dst < max) {
		*(char *)dst++ = *(char *)src++;
	}
	return dstaddr;
}

void *memset(void *dst, int c, size_t n) {
	void *dstaddr = dst;
	void *max = dst + n;
	u_char byte = c & 0xff;
	uint32_t word = byte | byte << 8 | byte << 16 | byte << 24;

	while (((u_long)dst & 3) && dst < max) {
		*(u_char *)dst++ = byte;
	}

	// fill machine words while possible
	while (dst + 4 <= max) {
		*(uint32_t *)dst = word;
		dst += 4;
	}

	// finish the remaining 0-3 bytes
	while (dst < max) {
		*(u_char *)dst++ = byte;
	}
	return dstaddr;
}

size_t strlen(const char *s) {
	int n;

	for (n = 0; *s; s++) {
		n++;
	}

	return n;
}

char *strcpy(char *dst, const char *src) {
	char *ret = dst;

	while ((*dst++ = *src++) != 0) {
	}

	return ret;
}

const char *strchr(const char *s, int c) {
	for (; *s; s++) {
		if (*s == c) {
			return s;
		}
	}
	return 0;
}

int strcmp(const char *p, const char *q) {
	while (*p && *p == *q) {
		p++, q++;
	}

	if ((u_int)*p < (u_int)*q) {
		return -1;
	}

	if ((u_int)*p > (u_int)*q) {
		return 1;
	}

	return 0;
}
