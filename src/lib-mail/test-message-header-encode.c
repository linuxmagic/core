/* Copyright (c) 2009-2018 Dovecot authors, see the included COPYING file */

#include "lib.h"
#include "base64.h"
#include "buffer.h"
#include "str.h"
#include "message-header-encode.h"
#include "test-common.h"

static bool verify_q(const char *str, unsigned int i, bool starts_with_a)
{
	unsigned int line_start = i, char_count = 0;

	if (str_begins(str+i, "\n\t")) {
		i += 2;
		line_start = i - 1;
	}

	for (;;) {
		if (!str_begins(str+i, "=?utf-8?q?"))
			return FALSE;
		i += 10;

		if (starts_with_a) {
			if (str[i] != 'a')
				return FALSE;
			starts_with_a = FALSE;
			i++;
		}
		while (!str_begins(str+i, "?=")) {
			if (!str_begins(str+i, "=C3=A4"))
				return FALSE;
			i += 6;
			char_count++;
		}
		i += 2;
		if (i - line_start > 76)
			return FALSE;

		if (str[i] == '\0')
			break;
		if (!str_begins(str+i, "\n\t"))
			return FALSE;
		i += 2;
		line_start = i - 1;
	}
	return char_count == 40;
}

static void test_message_header_encode_q(void)
{
	string_t *input = t_str_new(100);
	string_t *str = t_str_new(512);
	unsigned int i, j, skip;

	test_begin("message header encode q");

	str_append_c(input, 'a');
	for (i = 0; i < 40; i++)
		str_append(input, "\xC3\xA4");
	for (i = 0; i < 80; i++) {
		for (skip = 0; skip < 2; skip++) {
			str_truncate(str, 0);
			for (j = 1; j < i; j++)
				str_append_c(str, 'X');
			if (i != 0)
				str_append_c(str, ' ');

			message_header_encode_q(str_data(input) + skip,
						str_len(input) - skip, str,
						i == 0 ? 0 : i+1);
			test_assert(verify_q(str_c(str), i, skip == 0));
		}
	}
	test_end();
}

static bool verify_b(const char *str, unsigned int i, bool starts_with_a)
{
	unsigned int line_start = i, start, j, char_count = 0;
	char bufdata[1000];
	buffer_t buf;

	buffer_create_from_data(&buf, bufdata, sizeof(bufdata));
	if (str_begins(str+i, "\n\t")) {
		i += 2;
		line_start = i - 1;
	}

	for (;;) {
		if (!str_begins(str+i, "=?utf-8?b?"))
			return FALSE;
		i += 10;

		start = i;
		for (; str[i] != '?'; i++) {
			if (str[i] == '\0')
				return FALSE;
		}
		buffer_set_used_size(&buf, 0);
		if (base64_decode(str+start, i-start, &buf) < 0)
			return FALSE;
		i++;

		if (!starts_with_a)
			j = 0;
		else {
			if (bufdata[0] != 'a')
				return FALSE;
			starts_with_a = FALSE;
			j = 1;
		}
		for (; j < buf.used; j += 2) {
			if (bufdata[j] != '\xc3' || bufdata[j+1] != '\xa4')
				return FALSE;
			char_count++;
		}
		if (j != buf.used)
			return FALSE;

		if (str[i++] != '=')
			return FALSE;

		if (i - line_start > 76)
			return FALSE;

		if (str[i] == '\0')
			break;
		if (!str_begins(str+i, "\n\t"))
			return FALSE;
		i += 2;
		line_start = i - 1;
	}
	return char_count == 40;
}

static void test_message_header_encode_b(void)
{
	string_t *input = t_str_new(100);
	string_t *str = t_str_new(512);
	unsigned int i, j, skip;

	test_begin("message header encode b");

	str_append_c(input, 'a');
	for (i = 0; i < 40; i++)
		str_append(input, "\xC3\xA4");
	for (i = 0; i < 80; i++) {
		for (skip = 0; skip < 2; skip++) {
			str_truncate(str, 0);
			for (j = 1; j < i; j++)
				str_append_c(str, 'X');
			if (i != 0)
				str_append_c(str, ' ');

			message_header_encode_b(str_data(input) + skip,
						str_len(input) - skip, str,
						i == 0 ? 0 : i+1);
			test_assert(verify_b(str_c(str), i, skip == 0));
		}
	}
	test_end();
}

static void test_message_header_encode(void)
{
	const char *data[] = {
		"a b", "a b",
		"a bc\xC3\xA4""de f", "a =?utf-8?q?bc=C3=A4de?= f",
		"a \xC3\xA4\xC3\xA4 \xC3\xA4 b", "a =?utf-8?b?w6TDpCDDpA==?= b",
		"\xC3\xA4 a \xC3\xA4", "=?utf-8?q?=C3=A4_a_=C3=A4?=",
		"\xC3\xA4\xC3\xA4 a \xC3\xA4", "=?utf-8?b?w6TDpCBhIMOk?=",
		"=", "=",
		"?", "?",
		"a=?", "a=?",
		"=?", "=?utf-8?q?=3D=3F?=",
		"=?x", "=?utf-8?q?=3D=3Fx?=",
		"a\n=?", "a\n\t=?utf-8?q?=3D=3F?=",
		"a\t=?", "a\t=?utf-8?q?=3D=3F?=",
		"a =?", "a =?utf-8?q?=3D=3F?=",
		"foo\001bar", "=?utf-8?q?foo=01bar?=",
		"\x01\x02\x03\x04\x05\x06\x07\x08", "=?utf-8?b?AQIDBAUGBwg=?=",
#define TEXT30 "123456789012345678901234567890"
		TEXT30 " \xc3\xa4 " TEXT30 "\xc3\xa4 stuff",
			TEXT30 " =?utf-8?q?=C3=A4_12345678901234567890123456?=\n"
			"\t=?utf-8?q?7890=C3=A4?= stuff",

		"a\r\n b", "a\r\n b",
		"a\r\n\tb", "a\r\n\tb",
		"a\r\nb", "a\r\n\tb",
		"a\n b", "a\n b",
		"a\n  b", "a\n  b",
		"a\nb", "a\n\tb",
		"a\r\n", "a",
		"a\n", "a",
		"foo\n \001bar", "foo\n =?utf-8?q?=01bar?=",
		"foo\001\n bar", "=?utf-8?q?foo=01?=\n bar",
		"\xC3\xA4\xC3\xA4\xC3\xA4\xC3\xA4\xC3\xA4\xC3\xA4\xC3\xA4"
		"\xC3\xA4\xC3\xA4\xC3\xA4\xC3\xA4\xC3\xA4\xC3\xA4\xC3\xA4",
			"=?utf-8?b?w6TDpMOkw6TDpMOkw6TDpMOkw6TDpMOkw6TDpA==?=",
		/* Bad UTF-8 */
		"foofoo-\x80\x80\x80\x80\x80\x80\x80-barbar",
			"=?utf-8?q?foofoo-=EF=BF=BD-barbar?=",
		"foobarfoobar-\x80\x80\x80\x80\x80\x80\x80",
			"=?utf-8?q?foobarfoobar-=EF=BF=BD?=",
		"\x80\x80\x80\x80\x80\x80\x80-foobarfoobar",
			"=?utf-8?q?=EF=BF=BD-foobarfoobar?=",
		"foofoo-\x80\x80\x80\x80\x80\x80\x80-barbarbarbar-"
		"\x81\x82\x83\x84\x85\x86\x87-bazbaz",
			"=?utf-8?q?foofoo-=EF=BF=BD-barbarbarbar-"
			"=EF=BF=BD-bazbaz?=",
		"foobarfoobarfoobar-\x80\x80\x80\x80\x80\x80\x80-"
		"\x81\x82\x83\x84\x85\x86\x87-bazbaz",
			"=?utf-8?q?foobarfoobarfoobar-=EF=BF=BD-"
			"=EF=BF=BD-bazbaz?=",
		"\x80\x80\x80\x80\x80\x80\x80-foobarfoobarfoobar-"
		"\x81\x82\x83\x84\x85\x86\x87-bazbaz",
			"=?utf-8?q?=EF=BF=BD-foobarfoobarfoobar-"
			"=EF=BF=BD-bazbaz?=",
		"foofoo-\xC3-barbar",
			"=?utf-8?q?foofoo-=EF=BF=BD-barbar?=",
		"foobarfoobar-\xC3",
			"=?utf-8?q?foobarfoobar-=EF=BF=BD?=",
		"\xC3-foobarfoobar",
			"=?utf-8?q?=EF=BF=BD-foobarfoobar?=",
		"f-\x80\x80\x80\x80\x80\x80\x80-b", "=?utf-8?b?Zi3vv70tYg==?=",
		"fb-\x80\x80\x80\x80\x80\x80\x80", "=?utf-8?b?ZmIt77+9?=",
		"\x80\x80\x80\x80\x80\x80\x80-fb", "=?utf-8?b?77+9LWZi?=",
		"ff-\x80\x80\x80\x80\x80\x80\x80-bb-"
		"\x81\x82\x83\x84\x85\x86\x87-zz",
			"=?utf-8?b?ZmYt77+9LWJiLe+/vS16eg==?=",
		"fbfb-\x80\x80\x80\x80\x80\x80\x80-"
		"\x81\x82\x83\x84\x85\x86\x87-zz",
			"=?utf-8?b?ZmJmYi3vv70t77+9LXp6?=",
		"\x80\x80\x80\x80\x80\x80\x80-ff-"
		"\x81\x82\x83\x84\x85\x86\x87-zz",
			"=?utf-8?b?77+9LWZmLe+/vS16eg==?=",
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80",
			"=?utf-8?b?77+9?=",
		"-\xC3-\xC3-\xC3-\xC3-\xC3-\xC3-",
			"=?utf-8?q?-=EF=BF=BD-=EF=BF=BD-"
			"=EF=BF=BD-=EF=BF=BD-=EF=BF=BD-=EF=BF=BD-?=",
		"\xC3--\xC3-\xC3-\xC3-\xC3--\xC3",
			"=?utf-8?q?=EF=BF=BD--=EF=BF=BD-"
			"=EF=BF=BD-=EF=BF=BD-=EF=BF=BD--=EF=BF=BD?=",
		"-\xC3\xC3-\xC3\xC3-\xC3\xC3-\xC3\xC3-\xC3\xC3-\xC3\xC3-",
			"=?utf-8?b?Le+/vS3vv70t77+9Le+/vS3vv70t77+9LQ==?=",
		"-\xC3\xC3\xC3-\xC3\xC3\xC3-\xC3\xC3\xC3-"
		"\xC3\xC3\xC3-\xC3\xC3\xC3-\xC3\xC3\xC3-",
			"=?utf-8?b?Le+/vS3vv70t77+9Le+/vS3vv70t77+9LQ==?=",
		"\xC3\xC3\xC3\xC3\xC3\xC3\xC3\xC3\xC3\xC3\xC3\xC3\xC3\xC3",
			"=?utf-8?b?77+9?=",
		"-\xC3\xA4\xC3\xC3-\xC3\xC3\xA4\xC3-\xC3\xC3\xC3\xA4-"
		"\xC3\xC3\xC3\xA4-\xC3\xC3\xA4\xC3-\xC3\xA4\xC3\xC3-",
			"=?utf-8?b?LcOk77+9Le+/vcOk77+9Le+/"
			"vcOkLe+/vcOkLe+/vcOk77+9LcOk77+9LQ==?=",
	};                          
	string_t *str = t_str_new(128);
	unsigned int i;

	test_begin("message header encode");
	for (i = 0; i < N_ELEMENTS(data); i += 2) {
		str_truncate(str, 0);
		message_header_encode(data[i], str);
		test_assert_strcmp(str_c(str), data[i+1]);
	}
	test_end();
}

static void test_message_header_encode_data(void)
{
	string_t *str = t_str_new(128);
	static unsigned char nuls[10] = { 0, };

	test_begin("message header encode data");
	message_header_encode_data(nuls, 1, str);
	test_assert_strcmp(str_c(str), "=?utf-8?q?=00?=");

	str_truncate(str, 0);
	message_header_encode_data(nuls, sizeof(nuls), str);
	test_assert_strcmp(str_c(str), "=?utf-8?b?AAAAAAAAAAAAAA==?=");
	test_end();
}

int main(void)
{
	static void (*const test_functions[])(void) = {
		test_message_header_encode_q,
		test_message_header_encode_b,
		test_message_header_encode,
		test_message_header_encode_data,
		NULL
	};
	return test_run(test_functions);
}
