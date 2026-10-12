/* web.h
 * Github: https:\\github.com\tommojphillips
 *
 * A dependency free HTTP/1.1 + WebSocket server hosting the Altair 8800.
 *
 * The server owns a small pool of worker threads. Each worker runs one of
 * two jobs:
 *   - the emulator worker runs the CPU and pushes terminal output to every
 *     connected browser;
 *   - the HTTP workers accept connections, serve the static UI and answer
 *     the JSON API (which is served directly on the socket, so it never
 *     touches the emulator mutex).
 */

#ifndef WEB_H
#define WEB_H

#include <stdint.h>

/* Terminal ring buffer size. The browser replays this on reconnect. */
#define WEB_TERM_BYTES 32768

/* Maximum disk images / ROMs returned by the browser endpoints. */
#define WEB_MAX_DISKS 16
#define WEB_MAX_ROMS 64

/* ---- lifecycle ---------------------------------------------------------- */

/* Starts the server on settings.host:settings.port. Returns 0 on success,
 * 1 if the socket could not be bound. */
int web_start(void);

/* Blocks until web_stop() is called. Drives the emulator. */
void web_run(void);

/* Signal the server to shut down and wait for the workers to exit. */
void web_stop(void);

int web_is_running(void);

/* ---- terminal shared state ---------------------------------------------- */

/* Bytes produced by the guest since the last call. Copies up to `max`
 * bytes into `buf` and removes them from the pending queue. */
int web_term_take_pending(char *buf, int max);

/* Snapshot of the whole terminal history, NUL terminated. */
int web_term_snapshot(char *buf, int max);

void web_term_clear(void);

/* ---- input --------------------------------------------------------------- */

/* Queue one character for the guest. Returns 0 if the queue is full. */
int web_input_push(char ch);

/* ---- status -------------------------------------------------------------- */

typedef struct
{
	int running;
	int paused;
	int clients;
	uint32_t pc;
	uint32_t cycles;
	uint32_t ram_size;
	uint32_t cpu_clock;
	uint32_t refresh_rate;
	int port;
	char host[64];
	char mounted[WEB_MAX_DISKS][64]; /* empty string when not mounted */
} WEB_STATUS;

void web_status(WEB_STATUS *out);

/* ---- program loading ------------------------------------------------------ */

/* Loads a file from disk into guest memory at `offset`. Safe to call from
 * the API thread. Returns 0 on success. */
int web_load_program(const char *path, uint32_t offset);

/* Hardware reset, like the front panel RESET switch. */
void web_reset(void);

#endif
