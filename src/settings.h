/* settings.h
 * Github: https:\\github.com\tommojphillips
 *
 * Central runtime settings for the emulator. Every settable option is
 * registered here so that the command line, the web API and the browser UI
 * all read and write through one place.
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdint.h>

/* Where the values came from, so the API can tell the UI. */
enum
{
	SETTING_SOURCE_DEFAULT = 0,
	SETTING_SOURCE_CLI = 1,
	SETTING_SOURCE_WEB = 2,
	SETTING_SOURCE_FILE = 3
};

/* Every setting gets an id so the web API can address it by name. */
enum
{
	SETTING_ID_CPU_CLOCK = 0,
	SETTING_ID_REFRESH_RATE,
	SETTING_ID_RAM_SIZE,
	SETTING_ID_LEAD_IN,
	SETTING_ID_HOST,
	SETTING_ID_PORT,
	SETTING_ID_DISKS_DIR,
	SETTING_ID_ROMS_DIR,
	SETTING_ID_WEB_ROOT,
	SETTING_ID_AUTOSTART,
	SETTING_ID_CASE_FOLD,
	SETTING_ID_BACKSPACE,
	SETTING_ID_PACING,
	SETTING_ID_DEBUG_PORTS,
	SETTING_ID_COUNT
};

typedef struct
{
	const char *key;   /* name used by the API and settings file */
	const char *label; /* human readable label */
	const char *help;  /* one line description */
	const char *unit;  /* may be NULL */
	int type;		   /* 0 = int, 1 = bool, 2 = string */
	int64_t value;
	const char *svalue;
	int64_t def_value;
	const char *def_svalue;
	int64_t min;
	int64_t max;
	int source;
} SETTING;

/* Fixed size structures. Strings are copied in, never referenced, so the
 * values can come from argv (which the emulator does not own) or from a
 * socket buffer that is reused. */
#define SETTINGS_STRMAX 260

typedef struct
{
	int64_t value;
	char svalue[SETTINGS_STRMAX];
	int source;
} SETTING_VALUE;

typedef struct
{
	uint32_t cpu_clock;
	uint32_t refresh_rate;
	uint32_t ram_size;
	int lead_in;
	char host[SETTINGS_STRMAX];
	int port;
	char disks_dir[SETTINGS_STRMAX];
	char roms_dir[SETTINGS_STRMAX];
	char web_root[SETTINGS_STRMAX];
	int autostart;
	int case_fold;
	int backspace;
	int pacing;
	int debug_ports;
} SETTINGS;

extern SETTINGS settings;

/* Registration and defaults. Call once. */
void settings_init(void);

/* Number of registered settings, and access by index / id / key. */
int settings_count(void);
SETTING *settings_at(int index);
SETTING *settings_by_id(int id);
SETTING *settings_by_key(const char *key);

/* Read / write. Both accept either a number (for int/bool settings), a
 * boolean-ish word, or for string settings the string itself. */
int settings_set_int(int id, int64_t value, int source);
int settings_set_str(int id, const char *value, int source);
int settings_get_int(int id);
const char *settings_get_str(int id);

/* Setters used by code paths that already have typed values. These record
 * the source and keep the JSON view in sync. */
void settings_set_cpu_clock(uint32_t hz, int source);
void settings_set_refresh_rate(uint32_t hz, int source);
void settings_set_ram_size(uint32_t bytes, int source);
void settings_set_port(int port, int source);
void settings_set_host(const char *host, int source);
void settings_set_disks_dir(const char *dir, int source);
void settings_set_roms_dir(const char *dir, int source);

/* Parse "-k=v" / "-k v" style overrides coming from the API. */
int settings_apply_kv(const char *key, const char *value, int source);

/* Settings file (key=value lines). Returns 0 on success. */
int settings_load(const char *path);
int settings_save(const char *path);

/* Random access helpers used by the web layer. */
const char *settings_source_name(int source);

#endif
