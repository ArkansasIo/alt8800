/* settings.c
 * Github: https:\\github.com\tommojphillips
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "settings.h"

SETTINGS settings = {0};

/* The registry. Order must match the SETTING_ID_* enum in settings.h. */
static SETTING s_settings[SETTING_ID_COUNT];
static int s_settings_ready = 0;

static void str_copy(char *dst, const char *src, size_t cap)
{
	if (src == NULL)
	{
		dst[0] = '\0';
		return;
	}
	strncpy(dst, src, cap - 1);
	dst[cap - 1] = '\0';
}

static void register_int(int index, const char *key, const char *label, const char *help,
						 const char *unit, int64_t def, int64_t min, int64_t max)
{
	SETTING *s = &s_settings[index];
	s->key = key;
	s->label = label;
	s->help = help;
	s->unit = unit;
	s->type = 0;
	s->value = def;
	s->svalue = NULL;
	s->def_value = def;
	s->def_svalue = NULL;
	s->min = min;
	s->max = max;
	s->source = SETTING_SOURCE_DEFAULT;
}
static void register_bool(int index, const char *key, const char *label, const char *help, int64_t def)
{
	register_int(index, key, label, help, NULL, def, 0, 1);
	s_settings[index].type = 1;
}
static void register_str(int index, const char *key, const char *label, const char *help, const char *def)
{
	SETTING *s = &s_settings[index];
	s->key = key;
	s->label = label;
	s->help = help;
	s->unit = NULL;
	s->type = 2;
	s->value = 0;
	s->svalue = def;
	s->def_value = 0;
	s->def_svalue = def;
	s->min = 0;
	s->max = 0;
	s->source = SETTING_SOURCE_DEFAULT;
}

void settings_init(void)
{
	if (s_settings_ready)
		return;
	s_settings_ready = 1;

	register_int(SETTING_ID_CPU_CLOCK, "cpu_clock", "CPU clock",
				 "Speed of the emulated 8080 in Hz.", "Hz", 2000000, 100000, 20000000);
	register_int(SETTING_ID_REFRESH_RATE, "refresh_rate", "Refresh rate",
				 "Emulated frame rate; the CPU runs one frame worth of cycles per tick.", "Hz", 60, 1, 240);
	register_int(SETTING_ID_RAM_SIZE, "ram_size", "RAM size",
				 "Installed RAM in bytes (0x8000 = 32K).", "bytes", 0x10000, 0x400, 0x10000);
	register_int(SETTING_ID_LEAD_IN, "lead_in", "Startup lead in",
				 "Extra cycles given to the ROM before the first terminal character is captured.",
				 "cycles", 2000000, 0, 500000000);
	register_str(SETTING_ID_HOST, "host", "Bind address",
				 "Address the web server listens on.", "127.0.0.1");
	register_int(SETTING_ID_PORT, "port", "HTTP port",
				 "Port the web server listens on.", NULL, 8800, 1, 65535);
	register_str(SETTING_ID_DISKS_DIR, "disk_dir", "Disk image directory",
				 "Folder shown in the disk browser.", "disks");
	register_str(SETTING_ID_ROMS_DIR, "rom_dir", "ROM directory",
				 "Folder shown in the ROM browser.", "roms");
	register_str(SETTING_ID_WEB_ROOT, "web_root", "Web root",
				 "Folder containing the UI static files.", "web");
	register_bool(SETTING_ID_AUTOSTART, "autostart", "Autostart",
				  "Start the CPU as soon as the emulator is created.", 1);
	register_bool(SETTING_ID_CASE_FOLD, "case_fold", "Uppercase input",
				  "Fold typed lowercase letters to uppercase, like the real 2SIO board.", 1);
	register_bool(SETTING_ID_BACKSPACE, "backspace", "Backspace handling",
				  "Emit a destructive backspace for ASCII 0x08.", 1);
	register_bool(SETTING_ID_PACING, "real_time", "Real time pacing",
				  "Sleep between frames so emulation runs at wall clock speed.", 1);
	register_bool(SETTING_ID_DEBUG_PORTS, "debug_ports", "Log unmapped I/O",
				  "Print a message when the guest touches an unmapped port.", 1);
}

int settings_count(void) { return SETTING_ID_COUNT; }
SETTING *settings_at(int index)
{
	if (index < 0 || index >= SETTING_ID_COUNT)
		return NULL;
	return &s_settings[index];
}
SETTING *settings_by_id(int id) { return settings_at(id); }
SETTING *settings_by_key(const char *key)
{
	if (key == NULL)
		return NULL;
	for (int i = 0; i < SETTING_ID_COUNT; ++i) {
		if (strcmp(s_settings[i].key, key) == 0) return &s_settings[i];
	}
	return NULL;
}

const char *settings_source_name(int source)
{
	switch (source)
	{
	case SETTING_SOURCE_CLI:
		return "command line";
	case SETTING_SOURCE_WEB:
		return "web ui";
	case SETTING_SOURCE_FILE:
		return "settings file";
	default:
		return "default";
	}
}

static int clamp_int(int id, int64_t v, int64_t *out)
{
	SETTING *s = settings_by_id(id);
	if (s == NULL)
		return 0;
	if (v < s->min)
		v = s->min;
	if (v > s->max)
		v = s->max;
	*out = v;
	return 1;
}

static int parse_bool(const char *value, int64_t *out)
{
	if (value == NULL)
		return 0;
	if (strcmp(value, "true") == 0 || strcmp(value, "on") == 0 ||
		strcmp(value, "yes") == 0 || strcmp(value, "1") == 0)
	{
		*out = 1;
		return 1;
	}
	if (strcmp(value, "false") == 0 || strcmp(value, "off") == 0 ||
		strcmp(value, "no") == 0 || strcmp(value, "0") == 0)
	{
		*out = 0;
		return 1;
	}
	return 0;
}

int settings_set_int(int id, int64_t value, int source)
{
	SETTING *s = settings_by_id(id);
	int64_t v = 0;
	if (s == NULL || s->type == 2)
		return 0;
	if (s->type == 1)
	{
		if (!clamp_int(id, value, &v))
			return 0;
	}
	else
	{
		if (!clamp_int(id, value, &v))
			return 0;
	}
	s->value = v;
	s->source = source;

	switch (id)
	{
	case SETTING_ID_CPU_CLOCK:
		settings.cpu_clock = (uint32_t)v;
		break;
	case SETTING_ID_REFRESH_RATE:
		settings.refresh_rate = (uint32_t)v;
		break;
	case SETTING_ID_RAM_SIZE:
		settings.ram_size = (uint32_t)v;
		break;
	case SETTING_ID_LEAD_IN:
		settings.lead_in = (int)v;
		break;
	case SETTING_ID_PORT:
		settings.port = (int)v;
		break;
	case SETTING_ID_AUTOSTART:
		settings.autostart = (int)v;
		break;
	case SETTING_ID_CASE_FOLD:
		settings.case_fold = (int)v;
		break;
	case SETTING_ID_BACKSPACE:
		settings.backspace = (int)v;
		break;
	case SETTING_ID_PACING:
		settings.pacing = (int)v;
		break;
	case SETTING_ID_DEBUG_PORTS:
		settings.debug_ports = (int)v;
		break;
	}
	return 1;
}

int settings_set_str(int id, const char *value, int source)
{
	SETTING *s = settings_by_id(id);
	if (s == NULL || s->type != 2 || value == NULL)
		return 0;
	s->svalue = s->def_svalue; /* replaced below; pointer kept for JSON view */
	s->source = source;

	switch (id)
	{
	case SETTING_ID_HOST:
		str_copy(settings.host, value, SETTINGS_STRMAX);
		s->svalue = settings.host;
		break;
	case SETTING_ID_DISKS_DIR:
		str_copy(settings.disks_dir, value, SETTINGS_STRMAX);
		s->svalue = settings.disks_dir;
		break;
	case SETTING_ID_ROMS_DIR:
		str_copy(settings.roms_dir, value, SETTINGS_STRMAX);
		s->svalue = settings.roms_dir;
		break;
	case SETTING_ID_WEB_ROOT:
		str_copy(settings.web_root, value, SETTINGS_STRMAX);
		s->svalue = settings.web_root;
		break;
	default:
		return 0;
	}
	return 1;
}

int settings_get_int(int id)
{
	SETTING *s = settings_by_id(id);
	return s == NULL ? 0 : (int)s->value;
}
const char *settings_get_str(int id)
{
	SETTING *s = settings_by_id(id);
	if (s == NULL || s->svalue == NULL)
		return "";
	return s->svalue;
}

void settings_set_cpu_clock(uint32_t hz, int source) { settings_set_int(SETTING_ID_CPU_CLOCK, hz, source); }
void settings_set_refresh_rate(uint32_t hz, int source) { settings_set_int(SETTING_ID_REFRESH_RATE, hz, source); }
void settings_set_ram_size(uint32_t bytes, int source) { settings_set_int(SETTING_ID_RAM_SIZE, bytes, source); }
void settings_set_port(int port, int source) { settings_set_int(SETTING_ID_PORT, port, source); }
void settings_set_host(const char *host, int source) { settings_set_str(SETTING_ID_HOST, host, source); }
void settings_set_disks_dir(const char *dir, int source) { settings_set_str(SETTING_ID_DISKS_DIR, dir, source); }
void settings_set_roms_dir(const char *dir, int source) { settings_set_str(SETTING_ID_ROMS_DIR, dir, source); }

int settings_apply_kv(const char *key, const char *value, int source)
{
	SETTING *s = settings_by_key(key);
	if (s == NULL || value == NULL)
		return 0;

	if (s->type == 2) {
		return settings_set_str((int)(s - s_settings), value, source);
	}

	int64_t v = 0;
	if (s->type == 1 && parse_bool(value, &v)) {
		return settings_set_int((int)(s - s_settings), v, source);
	}
	v = strtoll(value, NULL, 0);
	return settings_set_int((int)(s - s_settings), v, source);
}

int settings_load(const char *path)
{
	FILE *f = NULL;
	char line[512];
	fopen_s(&f, path, "r");
	if (f == NULL)
		return 1;

	while (fgets(line, sizeof(line), f) != NULL)
	{
		char *p = line;
		while (*p == ' ' || *p == '\t')
			++p;
		if (*p == '\0' || *p == '\n' || *p == '#' || *p == ';')
			continue;

		char *eq = strchr(p, '=');
		if (eq == NULL)
			continue;
		*eq = '\0';
		char *key = p;
		char *val = eq + 1;

		/* trim */
		char *end = key + strlen(key);
		while (end > key && (end[-1] == ' ' || end[-1] == '\t'))
			*--end = '\0';
		while (*val == ' ' || *val == '\t')
			++val;
		end = val + strlen(val);
		while (end > val && (end[-1] == '\r' || end[-1] == '\n' || end[-1] == ' ' || end[-1] == '\t'))
			*--end = '\0';

		settings_apply_kv(key, val, SETTING_SOURCE_FILE);
	}
	fclose(f);
	return 0;
}

int settings_save(const char *path)
{
	FILE *f = NULL;
	fopen_s(&f, path, "w");
	if (f == NULL)
		return 1;

	fprintf(f, "# Altair 8800 emulator settings\n");
	fprintf(f, "# Written by the web UI.\n\n");
	for (int i = 0; i < SETTING_ID_COUNT; ++i)
	{
		SETTING *s = &s_settings[i];
		if (s->type == 2)
		{
			fprintf(f, "%-16s = %s\n", s->key, s->svalue ? s->svalue : "");
		}
		else
		{
			fprintf(f, "%-16s = %lld\n", s->key, (long long)s->value);
		}
	}
	fclose(f);
	return 0;
}
