/* TODO:
	* error reporting and code to string conversion - half done: code-pair emission,
		see struct message_error
	* better detection of blank lines, see test 2
	* organisation of token array to struct layout for associative win
	* make inline message struct comparison
 */

#include <stdlib.h>
#include <stdio.h>

#include <string.h>
#include <errno.h>

/*-/-/-----------------------------------------------------------------\-\-*/

typedef struct buffer Buffer;

struct buffer {
	unsigned long len, cap;
	char *base;
};

int buffer_push (Buffer *bp, int c) {
	return (bp->len < bp->cap) ? 
		(unsigned char)(bp->base[bp->len++] = c)
		:EOF;
}

char *buffer_head (Buffer *b) {
	return (b->len) ? 
		b->base + (b->len - 1)
		:NULL;
}

void buffer_set_head (Buffer *b, int c) {
	char *p;
	if ((p = buffer_head(b))) {
		*p = c;
	}
}

void buffer_reset (Buffer *b) {
	b->len = 0;
}

typedef struct stream Stream;
typedef int (*stream_get_fn)(void *);

typedef int (*stream_error_fn)(void *);
typedef int (*stream_eof_fn)(void *);

struct stream {
	void *handle;
	stream_get_fn get;

	stream_error_fn error;
	stream_eof_fn eof;
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

#define ARGS_MAX 16

typedef struct message Message;
struct message {
	Buffer *b;
	size_t n;
	char *t[32];
};

enum BufferError {
	BUFFER_EOF,
	BUFFER_EMPTY
};

enum MessageErrorType {
	MESSAGE_ERROR_NONE_T,
	MESSAGE_ERROR_BUFFER_T,
	MESSAGE_ERROR_STREAM_T
};

typedef struct message_error MessageError;
struct message_error {
	int code;
	enum MessageErrorType type;
};

#define NO_ERROR (MessageError){0, MESSAGE_ERROR_NONE_T}

#define NEWLINE     '\n'
#define RETURN_FEED '\r'
#define SPACE       ' '
#define COLON       ':'

MessageError get_message (Stream *sp, Message *mp) {

#define BUFFER_ERROR(e) \
	(MessageError) {\
		(e), \
		MESSAGE_ERROR_BUFFER_T \
	}

# define PUSHC(b,c) do {\
	if (buffer_push((b), (c)) == EOF) \
		return BUFFER_ERROR(BUFFER_EOF); \
} while (0)

# define STREAM_ERROR(s) \
	(MessageError) { \
		stream_error((s)),\
		MESSAGE_ERROR_STREAM_T \
	}

# define GETC(c,s) do { \
	if ((c = stream_get(s)) == EOF) { \
		return STREAM_ERROR(s); \
	} \
} while (0)

	int c, l;

	l = 0;
	mp->n = 0;
	mp->t[0] = mp->b->base;

	for (;; l = c) {
		GETC(c,sp);

		if (c == NEWLINE && l == RETURN_FEED) {
			buffer_set_head(mp->b, '\0');
			goto RETURN;
		} else if (c == SPACE) {
			PUSHC(mp->b, '\0');
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

		if (c == NEWLINE && l == RETURN_FEED) {
			buffer_set_head(mp->b, '\0');
			break;
		}
		PUSHC(mp->b, c);
	}

RETURN:
	mp->n++;
	return NO_ERROR;

#undef BUFFER_ERROR
#undef PUSHC

#undef STREAM_ERROR
#undef GETC
}

/*-\-\-----------------------------------------------------------------/-/-*/

#define NELEMS(t) (sizeof t / sizeof *t)

struct mtest {
	char *input;
	Message output;
};

struct mtest tests[] = { 
	{ 
		":nick!user@irc.net COMMAND foo:bar :foo bar foo bar\r\n", 
		(Message){ NULL, 4, {
				":nick!user@irc.net", 
				"COMMAND",
				"foo:bar", 
				":foo bar foo bar"
			}
		}
	},
	{ 
		"\r\n", 
		(Message){ NULL, 1, {""}}
	}
};

size_t ntests = NELEMS(tests);

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

char buffer[BUFSIZ];

int main () {

	long unsigned nerrors;
	long unsigned i, j;
	
	Message m;
	Stream s;
	Buffer b;

	nerrors = 0;

	m.b = &b;

	b.base = buffer;
	b.cap = NELEMS(buffer);

	s.get = byte_stream_get;
	s.eof = byte_stream_eof;
	s.error = byte_stream_error;

	for (i = 0; i < ntests; i++) {
		s.handle = &tests[i].input;
		b.len = 0;
		memset(m.t, '\0', sizeof m.t);

		get_message(&s, &m);

		if (m.n != tests[i].output.n) {
			fprintf(stderr, "[%.2lu]: token count mismatch: got: %lu expected: %lu\n", 
				i, m.n, tests[i].output.n);
			nerrors++;
		} else {
			for (j = 0; j < m.n; j++) {
				if (strlen(tests[i].output.t[j]) != strlen(m.t[j])) {			
					fprintf(stderr, "[%.2lu]: token length mismatch: got: %s expected: %s\n",
						j, m.t[j], tests[i].output.t[j]);
					nerrors++;
				} else if ( strcmp(tests[i].output.t[j], m.t[j]) != 0) {
					fprintf(stderr, "[%.2lu]: token mismatch: got: %s expected: %s\n",
						j, m.t[j], tests[i].output.t[j]);
					nerrors++;
				}
			}
		}
	}

	if (nerrors > 0) {
		fprintf(stderr, "%lu errors total\n");
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}
