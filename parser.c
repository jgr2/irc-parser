/* TODO:
	* error reporting and code to string conversion - half done: code-pair emission,
		see struct ERROR
	* better detection of blank lines, see test 2
	* organisation of token array to struct layout for associative win
	* make inline message struct comparison
	* add more message tests
 */

#include <stdlib.h>
#include <stdio.h>

#include <string.h>
#include <errno.h>

#define NELEMS(t) (sizeof t / sizeof t[0])

/*-/-/-----------------------------------------------------------------\-\-*/

typedef struct buffer Buffer;

struct buffer {
	unsigned long len, cap;
	char *base;
};

#define BUFFER_OVERFLOW(bp)  ((bp)->len >= (bp)->cap)

#define BUFFER_UNDERFLOW(bp) ((bp)->len <= 0)

#define BUFFER_PUSH(bp) ((unsigned char)((bp)->base[(bp)->len++] = c))

#define BUFFER_HEAD(bp) ((bp)->base + ((bp)->len - 1))

int buffer_push (Buffer *bp, int c) {
	return BUFFER_OVERFLOW(bp) ? EOF: BUFFER_PUSH(bp);
}

char *buffer_head (Buffer *bp) {
	return BUFFER_UNDERFLOW(bp) ? NULL: BUFFER_HEAD(bp);
}

void buffer_set_head (Buffer *bp, int c) {
	char *p;
	if ((p = buffer_head(bp))) {
		*p = c;
	}
}

void buffer_reset (Buffer *bp) {
	bp->len = 0;
}

/*-\-\-----------------------------------------------------------------/-/-*/
/*-/-/-----------------------------------------------------------------\-\-*/

typedef struct stream Stream;

typedef int (*stream_get_fn)(void *);
typedef int (*stream_eof_fn)(void *);
typedef int (*stream_error_fn)(void *);

struct stream {
	void *handle;
	stream_get_fn get;

	stream_eof_fn eof;
	stream_error_fn error;
};

int stream_get (Stream *sp) {
	return sp->get(sp->handle);
}

int stream_error (Stream *sp) {
	return sp->error(sp->handle);
}

int stream_eof (Stream *sp) {
	return sp->eof(sp->handle);
}

/*-\-\-----------------------------------------------------------------/-/-*/
/*-/-/-----------------------------------------------------------------\-\-*/

#define SUCCESS 0
#define NO_ERROR SUCCESS

typedef struct error Error;

enum ErrorType {
	ERROR_TYPE_NONE = NO_ERROR,
	ERROR_TYPE_BUFFER,
	ERROR_TYPE_STREAM,
	ERROR_TYPE_PARSE
};

struct error {
	enum ErrorType type;
	int code;
};

enum ParseError {
	PARSE_E_SUCCESS = SUCCESS,
	PARSE_E_T_MAX
};

enum BufferError {
	BUFFER_E_SUCCESS = SUCCESS,
	BUFFER_E_OVERFLOW,
	BUFFER_E_UNDERFLOW
};

enum StreamError {
	STREAM_E_SUCCESS = SUCCESS,
	STREAM_E_EOF
};

Error no_error = {ERROR_TYPE_NONE, SUCCESS};

/*-\-\-----------------------------------------------------------------/-/-*/
/*-/-/-----------------------------------------------------------------\-\-*/

typedef struct message Message;

#define T_MAX 32

struct message {
	Buffer *b;
	size_t n;
	char *t[T_MAX];
};

#define NL    '\n'
#define CR    '\r'
#define SPACE  ' '
#define COLON  ':'

Error get_message (Stream *sp, Message *mp) {

# define PUSHC(b,c) do {\
	if (buffer_push((b), (c)) == EOF) { \
		error.type = ERROR_TYPE_BUFFER; \
		error.code = BUFFER_E_OVERFLOW; \
		goto RETURN; \
	} \
} while (0)

# define GETC(c,s) do { \
	if ((c = stream_get(s)) == EOF) { \
		error.type = ERROR_TYPE_STREAM; \
		error.code = stream_error(s); \
		goto RETURN; \
	} \
} while (0)

	Error error = no_error;

	int c, l;

	l = 0;
	mp->n = 0;
	mp->t[0] = mp->b->base;

	for (;; l = c) {
		GETC(c,sp);

		if (c == NL && l == CR) {
			buffer_set_head(mp->b, '\0');
			goto RETURN;
		} else if (c == SPACE) {
			PUSHC(mp->b, '\0');
			if (mp->n >= T_MAX) {
				error.type = ERROR_TYPE_PARSE;
				error.code = PARSE_E_T_MAX;
				goto RETURN;
			}
			mp->n++;
			mp->t[mp->n] = buffer_head(mp->b) + 1;
			continue;
		}

		PUSHC(mp->b, c);
		
		if (c == COLON && l == SPACE) {
			break;
		}
	}

	for (;; l = c) {
		GETC(c, sp);

		if (c == NL && l == CR) {
			buffer_set_head(mp->b, '\0');
			break;
		}
		PUSHC(mp->b, c);
	}

RETURN:
	mp->n++;
	return error;

#undef PUSHC
#undef GETC
}

/*-\-\-----------------------------------------------------------------/-/-*/

int byte_stream_get (void *_p) {
	char **p = _p;
	char *s = *p;
	int c;

	c = (char)*s++;
	*p = s;
	
	return (c == '\0') ? EOF: c;
}

int byte_stream_eof (void *_p) {
	char **p = _p;
	return **p == '\0';
}

int byte_stream_error (void *_p) {
	return byte_stream_eof(_p);
}

struct mtest {
	char *input;
	Message output;
};

struct mtest tests[] = { 
	{
		":nick!user@irc.net COMMAND foo:bar :foo bar foo bar\r\n", 
		{ NULL, 4, {
				":nick!user@irc.net", 
				"COMMAND",
				"foo:bar", 
				":foo bar foo bar"
			}
		}
	},
	{"\r\n", {NULL, 1, {""}}},
	{":\r\n", {NULL, 1, {":"}}},
	{"COMMAND\r\n", {NULL, 1, {"COMMAND"}}}
};

size_t ntests = NELEMS(tests);

char buffer[BUFSIZ];

#include <assert.h>

int main () {

	long unsigned i, j;
	
	Message m;
	Stream s;
	Buffer b;

	m.b = &b;

	b.base = buffer;
	b.cap = NELEMS(buffer);

	s.get = byte_stream_get;
	s.eof = byte_stream_eof;
	s.error = byte_stream_error;

	for (i = 0; i < ntests; i++) {
		s.handle = &tests[i].input;
		buffer_reset(m.b);
		memset(m.t, '\0', sizeof m.t);

		get_message(&s, &m);

		assert(m.n == tests[i].output.n);
	
		for (j = 0; j < m.n; j++) {
			assert(strlen(tests[i].output.t[j]) == strlen(m.t[j]));
			assert(strcmp(tests[i].output.t[j], m.t[j]) == 0);
		}
	}

	exit(EXIT_SUCCESS);
}
