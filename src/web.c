/* web.c
 * Github: https:\\github.com\tommojphillips
 *
 * HTTP/1.1 + WebSocket server for the Altair 8800.
 *
 * Winsock is used directly so the emulator keeps zero third party
 * dependencies. The interesting parts are:
 *
 *   - ws frame encode/decode (RFC 6455) including the client masking rule,
 *   - a binary-safe text terminal fed from the SIO output callback,
 *   - a client registry guarded by a single SRWLOCK,
 *   - an emulator thread that paces itself against the wall clock.
 */

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "web.h"
#include "settings.h"
#include "altair8800.h"
#include "88_sio.h"
#include "88_dcdd.h"
#include "i8080.h"

#define WEB_MAX_CLIENTS 32
#define WEB_MAX_WORKERS 4

/* Minimum bytes the guest must produce before the emulator thread publishes
 * a frame. Bounds websocket traffic while keeping interactive echo snappy. */
#define WEB_FLUSH_INTERVAL_CYCLES 20000

#define WEB_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

/* ---- global state -------------------------------------------------------- */

static SOCKET s_listen = INVALID_SOCKET;
static CRITICAL_SECTION s_clients_lock;
static int s_running = 0;
static int s_paused = 0;
static int s_initialised = 0;
static int s_clients_lock_ready = 0;

typedef struct
{
	SOCKET sock;
	int used;
	unsigned char in[4096]; /* partial frame buffer */
	int in_len;
} WEB_CLIENT;

static WEB_CLIENT s_clients[WEB_MAX_CLIENTS];
static SRWLOCK s_term_lock = SRWLOCK_INIT;
static char s_term[WEB_TERM_BYTES];
static int s_term_head = 0; /* next write position */
static int s_term_len = 0;	/* valid bytes */
static int s_term_sent[WEB_MAX_CLIENTS];
static char s_term_frame[WEB_TERM_BYTES * 2];
static int s_term_frame_len = 0;

/* Guest input queue, filled by the web UI and drained by the SIO. */
static CRITICAL_SECTION s_input_lock;
static int s_input_lock_ready = 0;
static char s_input[8192];
static int s_input_head = 0;
static int s_input_len = 0;

/* Scratch buffers for the HTTP worker threads. Each worker owns a slice,
 * indexed by its worker id, so no allocation happens per request. */
static char s_http_req[WEB_MAX_WORKERS][16384];
static char s_resp_buf[WEB_MAX_WORKERS][WEB_TERM_BYTES + 16384];

/* ---- helpers ------------------------------------------------------------- */

static int64_t web_now_ms(void)
{
	return (int64_t)GetTickCount64();
}

static void web_sleep_ms(int ms)
{
	if (ms <= 0)
		return;
	Sleep((DWORD)ms);
}

/* ---- base64 (for the WebSocket handshake and file downloads) ------------- */

static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void base64_encode(const unsigned char *in, int len, char *out)
{
	int i = 0, o = 0;
	while (i + 2 < len)
	{
		uint32_t v = ((uint32_t)in[i] << 16) | ((uint32_t)in[i + 1] << 8) | in[i + 2];
		out[o++] = B64[(v >> 18) & 63];
		out[o++] = B64[(v >> 12) & 63];
		out[o++] = B64[(v >> 6) & 63];
		out[o++] = B64[v & 63];
		i += 3;
	}
	if (i + 1 == len)
	{
		uint32_t v = (uint32_t)in[i] << 16;
		out[o++] = B64[(v >> 18) & 63];
		out[o++] = B64[(v >> 12) & 63];
		out[o++] = '=';
		out[o++] = '=';
	}
	else if (i + 2 == len)
	{
		uint32_t v = ((uint32_t)in[i] << 16) | ((uint32_t)in[i + 1] << 8);
		out[o++] = B64[(v >> 18) & 63];
		out[o++] = B64[(v >> 12) & 63];
		out[o++] = B64[(v >> 6) & 63];
		out[o++] = '=';
	}
	out[o] = '\0';
}

/* ---- SHA-1 (handshake only; not used for anything security sensitive) ----- */

typedef struct
{
	uint32_t h[5];
	uint64_t len;
	unsigned char buf[64];
	int buf_len;
} SHA1_CTX;

static uint32_t rol32(uint32_t v, int n) { return (v << n) | (v >> (32 - n)); }

static void sha1_block(SHA1_CTX *c, const unsigned char *p)
{
	uint32_t w[80];
	for (int i = 0; i < 16; ++i)
	{
		w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) |
			   ((uint32_t)p[i * 4 + 2] << 8) | (uint32_t)p[i * 4 + 3];
	}
	for (int i = 16; i < 80; ++i)
	{
		w[i] = rol32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
	}
	uint32_t a = c->h[0], b = c->h[1], cc = c->h[2], d = c->h[3], e = c->h[4];
	for (int i = 0; i < 80; ++i)
	{
		uint32_t f, k;
		if (i < 20)
		{
			f = (b & cc) | ((~b) & d);
			k = 0x5A827999;
		}
		else if (i < 40)
		{
			f = b ^ cc ^ d;
			k = 0x6ED9EBA1;
		}
		else if (i < 60)
		{
			f = (b & cc) | (b & d) | (cc & d);
			k = 0x8F1BBCDC;
		}
		else
		{
			f = b ^ cc ^ d;
			k = 0xCA62C1D6;
		}
		uint32_t t = rol32(a, 5) + f + e + k + w[i];
		e = d;
		d = cc;
		cc = rol32(b, 30);
		b = a;
		a = t;
	}
	c->h[0] += a;
	c->h[1] += b;
	c->h[2] += cc;
	c->h[3] += d;
	c->h[4] += e;
}

static void sha1_init(SHA1_CTX *c)
{
	c->h[0] = 0x67452301;
	c->h[1] = 0xEFCDAB89;
	c->h[2] = 0x98BADCFE;
	c->h[3] = 0x10325476;
	c->h[4] = 0xC3D2E1F0;
	c->len = 0;
	c->buf_len = 0;
}
static void sha1_update(SHA1_CTX *c, const unsigned char *data, int len)
{
	c->len += (uint64_t)len;
	while (len > 0)
	{
		int take = 64 - c->buf_len;
		if (take > len)
			take = len;
		memcpy(c->buf + c->buf_len, data, take);
		c->buf_len += take;
		data += take;
		len -= take;
		if (c->buf_len == 64)
		{
			sha1_block(c, c->buf);
			c->buf_len = 0;
		}
	}
}
static void sha1_final(SHA1_CTX *c, unsigned char *out)
{
	uint64_t bits = c->len * 8;
	unsigned char pad = 0x80;
	sha1_update(c, &pad, 1);
	unsigned char zero = 0;
	while (c->buf_len != 56)
		sha1_update(c, &zero, 1);
	unsigned char lenbuf[8];
	for (int i = 0; i < 8; ++i)
		lenbuf[i] = (unsigned char)(bits >> (56 - i * 8));
	sha1_update(c, lenbuf, 8);
	for (int i = 0; i < 5; ++i)
	{
		out[i * 4] = (unsigned char)(c->h[i] >> 24);
		out[i * 4 + 1] = (unsigned char)(c->h[i] >> 16);
		out[i * 4 + 2] = (unsigned char)(c->h[i] >> 8);
		out[i * 4 + 3] = (unsigned char)(c->h[i]);
	}
}

/* ---- client registry ------------------------------------------------------ */

static void clients_broadcast(const char *data, int len);

static int client_add(SOCKET s)
{
	EnterCriticalSection(&s_clients_lock);
	for (int i = 0; i < WEB_MAX_CLIENTS; ++i)
	{
		if (!s_clients[i].used)
		{
			s_clients[i].used = 1;
			s_clients[i].sock = s;
			s_clients[i].in_len = 0;
			/* New client starts at the tail of the history. */
			s_term_sent[i] = 0;
			LeaveCriticalSection(&s_clients_lock);
			return i;
		}
	}
	LeaveCriticalSection(&s_clients_lock);
	return -1;
}

static void client_remove(int index)
{
	if (index < 0 || index >= WEB_MAX_CLIENTS)
		return;
	EnterCriticalSection(&s_clients_lock);
	if (s_clients[index].used)
	{
		closesocket(s_clients[index].sock);
		s_clients[index].used = 0;
		s_clients[index].sock = INVALID_SOCKET;
	}
	LeaveCriticalSection(&s_clients_lock);
}

static int client_count(void)
{
	int n = 0;
	EnterCriticalSection(&s_clients_lock);
	for (int i = 0; i < WEB_MAX_CLIENTS; ++i)
		if (s_clients[i].used)
			++n;
	LeaveCriticalSection(&s_clients_lock);
	return n;
}

/* Sends a whole buffer, tolerating short writes. */
static int sock_send_all(SOCKET s, const char *data, int len)
{
	int sent = 0;
	while (sent < len)
	{
		int n = send(s, data + sent, len - sent, 0);
		if (n == SOCKET_ERROR || n == 0)
			return 0;
		sent += n;
	}
	return 1;
}

/* ---- WebSocket framing ---------------------------------------------------- */

static int ws_send_text(SOCKET s, const char *data, int len)
{
	unsigned char header[10];
	int hlen = 0;
	header[0] = 0x81; /* FIN + text */
	if (len < 126)
	{
		header[1] = (unsigned char)len;
		hlen = 2;
	}
	else if (len < 65536)
	{
		header[1] = 126;
		header[2] = (unsigned char)(len >> 8);
		header[3] = (unsigned char)(len & 0xFF);
		hlen = 4;
	}
	else
	{
		header[1] = 127;
		for (int i = 0; i < 8; ++i)
			header[2 + i] = (unsigned char)((uint64_t)len >> (56 - i * 8));
		hlen = 10;
	}
	if (!sock_send_all(s, (const char *)header, hlen))
		return 0;
	if (len > 0)
	{
		if (!sock_send_all(s, data, len))
			return 0;
	}
	return 1;
}

/* Close frame, then a graceful shutdown. */
static void ws_close(SOCKET s, int code)
{
	unsigned char frame[4];
	frame[0] = 0x88;
	frame[1] = 0x02;
	frame[2] = (unsigned char)(code >> 8);
	frame[3] = (unsigned char)(code & 0xFF);
	sock_send_all(s, (const char *)frame, 4);
	shutdown(s, SD_SEND);
}

/* Pulls one complete frame out of `buf`, returning bytes consumed or 0. */
static int ws_parse_frame(unsigned char *buf, int len, unsigned char *opcode,
						  unsigned char *payload, int *payload_len)
{
	if (len < 2)
		return 0;

	unsigned char b0 = buf[0];
	unsigned char b1 = buf[1];
	*opcode = b0 & 0x0F;
	int masked = (b1 & 0x80) != 0;
	uint64_t plen = b1 & 0x7F;
	int pos = 2;

	if (plen == 126)
	{
		if (len < 4)
			return 0;
		plen = ((uint64_t)buf[2] << 8) | buf[3];
		pos = 4;
	}
	else if (plen == 127)
	{
		if (len < 10)
			return 0;
		plen = 0;
		for (int i = 0; i < 8; ++i)
			plen = (plen << 8) | buf[2 + i];
		pos = 10;
	}
	if (plen > 1u << 20)
		return -1; /* refuse absurd frames */

	if (masked)
	{
		if (len < pos + 4)
			return 0;
		unsigned char mask[4];
		memcpy(mask, buf + pos, 4);
		pos += 4;
		if ((uint64_t)len < (uint64_t)pos + plen)
			return 0;
		for (uint64_t i = 0; i < plen; ++i)
			payload[i] = buf[pos + i] ^ mask[i & 3];
	}
	else
	{
		if ((uint64_t)len < (uint64_t)pos + plen)
			return 0;
		memcpy(payload, buf + pos, (size_t)plen);
	}

	*payload_len = (int)plen;
	return pos + (int)plen;
}

/* ---- terminal buffer ------------------------------------------------------ */

/* Called from sio_write() on the emulator thread. Never blocks. */
static void web_term_append(char ch)
{
	AcquireSRWLockExclusive(&s_term_lock);
	s_term[s_term_head] = ch;
	s_term_head = (s_term_head + 1) % WEB_TERM_BYTES;
	if (s_term_len < WEB_TERM_BYTES)
		s_term_len++;
	ReleaseSRWLockExclusive(&s_term_lock);
}

int web_term_take_pending(char *buf, int max)
{
	AcquireSRWLockExclusive(&s_term_lock);
	int n = s_term_len;
	if (n > max)
	{
		/* Never cut a UTF-8 sequence in half. */
		n = max;
		while (n > 0 && (s_term[(s_term_head - s_term_len + n - 1 + WEB_TERM_BYTES * 2) % WEB_TERM_BYTES] & 0xC0) == 0x80)
			--n;
	}
	if (n <= 0)
	{
		ReleaseSRWLockExclusive(&s_term_lock);
		return 0;
	}
	int start = (s_term_head - s_term_len + WEB_TERM_BYTES * 2) % WEB_TERM_BYTES;
	for (int i = 0; i < n; ++i)
	{
		buf[i] = s_term[(start + i) % WEB_TERM_BYTES];
		if (buf[i] == '\n' && s_term[(start + i - 1 + WEB_TERM_BYTES * 2) % WEB_TERM_BYTES] != '\r')
		{
			/* keep the original bytes; CR insertion happens in the browser */
		}
	}
	s_term_len -= n;
	ReleaseSRWLockExclusive(&s_term_lock);
	return n;
}

int web_term_snapshot(char *buf, int max)
{
	AcquireSRWLockExclusive(&s_term_lock);
	int n = s_term_len < max - 1 ? s_term_len : max - 1;
	int start = (s_term_head - s_term_len + WEB_TERM_BYTES * 2) % WEB_TERM_BYTES;
	for (int i = 0; i < n; ++i)
		buf[i] = s_term[(start + i) % WEB_TERM_BYTES];
	buf[n] = '\0';
	ReleaseSRWLockExclusive(&s_term_lock);
	return n;
}

void web_term_clear(void)
{
	AcquireSRWLockExclusive(&s_term_lock);
	s_term_head = 0;
	s_term_len = 0;
	ReleaseSRWLockExclusive(&s_term_lock);
}

/* ---- guest input ---------------------------------------------------------- */

int web_input_push(char ch)
{
	if (!s_input_lock_ready)
		return 0;
	EnterCriticalSection(&s_input_lock);
	if (s_input_len >= (int)sizeof(s_input))
	{
		LeaveCriticalSection(&s_input_lock);
		return 0;
	}
	s_input[(s_input_head + s_input_len) % (int)sizeof(s_input)] = ch;
	s_input_len++;
	LeaveCriticalSection(&s_input_lock);
	return 1;
}

/* Drains a few characters for the SIO. Called on the emulator thread. */
int web_input_pop(char *out)
{
	if (!s_input_lock_ready)
		return 0;
	EnterCriticalSection(&s_input_lock);
	if (s_input_len == 0)
	{
		LeaveCriticalSection(&s_input_lock);
		return 0;
	}
	*out = s_input[s_input_head];
	s_input_head = (s_input_head + 1) % (int)sizeof(s_input);
	s_input_len--;
	LeaveCriticalSection(&s_input_lock);
	return 1;
}

int web_input_peek(void)
{
	int n;
	if (!s_input_lock_ready)
		return 0;
	EnterCriticalSection(&s_input_lock);
	n = s_input_len;
	LeaveCriticalSection(&s_input_lock);
	return n;
}
