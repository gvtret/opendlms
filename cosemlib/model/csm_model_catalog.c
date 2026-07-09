/**
 * YAML catalog parser for COSEM object definitions
 *
 * Minimal line-based parser for the catalog YAML format:
 *   catalog:
 *     - class_id: 8
 *       logical_name: "0.0.1.0.0.255"
 *       version: 0
 *
 * Copyright (c) 2016, Anthony Rabine
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms of the MIT license.
 * See LICENSE.txt for more details.
 *
 */

#include "csm_model_catalog.h"
#include "csm_config.h"
#include <stdio.h>
#include <string.h>

static csm_object_t catalog_entries[CSM_MODEL_CATALOG_MAX_ENTRIES];
static int catalog_count = 0;

static const char *skip_whitespace(const char *p);

static int parse_uint(const char *str, uint32_t max_value, uint32_t *out) {
	uint32_t value = 0U;
	int have_digit = FALSE;

	str = skip_whitespace(str);
	while ((*str >= '0') && (*str <= '9')) {
		uint32_t digit = (uint32_t)(*str - '0');
		have_digit = TRUE;
		if ((value > (max_value / 10U)) || ((value == (max_value / 10U)) && (digit > (max_value % 10U)))) {
			return FALSE;
		}
		value = (value * 10U) + digit;
		str++;
	}

	if (!have_digit) {
		return FALSE;
	}

	*out = value;
	return TRUE;
}

static int parse_obis_string(const char *str, csm_obis_code *obis) {
	uint32_t values[6] = {0U, 0U, 0U, 0U, 0U, 0U};
	uint8_t part = 0U;
	int have_digit = FALSE;

	if ((str == NULL) || (obis == NULL)) {
		return FALSE;
	}

	while (*str != '\0') {
		if ((*str >= '0') && (*str <= '9')) {
			uint32_t digit = (uint32_t)(*str - '0');
			have_digit = TRUE;
			if ((values[part] > 25U) || ((values[part] == 25U) && (digit > 5U))) {
				return FALSE;
			}
			values[part] = (values[part] * 10U) + digit;
		} else if (*str == '.') {
			if (!have_digit || (part >= 5U)) {
				return FALSE;
			}
			part++;
			have_digit = FALSE;
		} else {
			return FALSE;
		}
		str++;
	}

	if (!have_digit || (part != 5U)) {
		return FALSE;
	}

	obis->A = (uint8_t)values[0];
	obis->B = (uint8_t)values[1];
	obis->C = (uint8_t)values[2];
	obis->D = (uint8_t)values[3];
	obis->E = (uint8_t)values[4];
	obis->F = (uint8_t)values[5];
	return TRUE;
}

static const char *skip_whitespace(const char *p) {
	while (*p == ' ' || *p == '\t') {
		p++;
	}
	return p;
}

static int starts_with(const char *line, const char *prefix) {
	return strncmp(line, prefix, strlen(prefix)) == 0;
}

static int parse_line(const char *line) {
	const char *p = skip_whitespace(line);

	if (*p == '#' || *p == '\0' || *p == '\n' || *p == '\r') {
		return TRUE;
	}

	if (starts_with(p, "class_id:")) {
		if (catalog_count <= 0) {
			return FALSE;
		}
		uint32_t val = 0U;
		if (parse_uint(p + 9, 0xFFFFU, &val) != TRUE) {
			return FALSE;
		}
		catalog_entries[catalog_count - 1].class_id = (uint16_t)val;
	} else if (starts_with(p, "logical_name:")) {
		if (catalog_count <= 0) {
			return FALSE;
		}
		const char *val = skip_whitespace(p + 13);
		char obis_str[32];
		int i = 0;
		if (*val == '"') {
			val++;
			while (*val != '"' && *val != '\0' && i < (int)sizeof(obis_str) - 1) {
				obis_str[i++] = *val++;
			}
		} else {
			while (*val != ' ' && *val != '\t' && *val != '\0' && *val != '\n' && i < (int)sizeof(obis_str) - 1) {
				obis_str[i++] = *val++;
			}
		}
		obis_str[i] = '\0';
		if (parse_obis_string(obis_str, &catalog_entries[catalog_count - 1].obis) != TRUE) {
			return FALSE;
		}
	} else if (starts_with(p, "version:")) {
		if (catalog_count <= 0) {
			return FALSE;
		}
		uint32_t val = 0U;
		if (parse_uint(p + 8, 0xFFU, &val) != TRUE) {
			return FALSE;
		}
		catalog_entries[catalog_count - 1].version = (uint8_t)val;
	} else if (starts_with(p, "- ") || starts_with(p, "-")) {
		if (catalog_count >= (int)CSM_MODEL_CATALOG_MAX_ENTRIES) {
			CSM_ERR("[CATALOG] Full, max %u entries", CSM_MODEL_CATALOG_MAX_ENTRIES);
			return FALSE;
		}
		memset(&catalog_entries[catalog_count], 0, sizeof(csm_object_t));
		catalog_entries[catalog_count].id = -1;
		catalog_count++;

		const char *rest = skip_whitespace(p + 1);
		if (starts_with(rest, "class_id:")) {
			uint32_t val = 0U;
			if (parse_uint(rest + 9, 0xFFFFU, &val) != TRUE) {
				return FALSE;
			}
			catalog_entries[catalog_count - 1].class_id = (uint16_t)val;
		}
	}

	return TRUE;
}

void csm_model_catalog_reset(void) {
	catalog_count = 0;
	memset(catalog_entries, 0, sizeof(catalog_entries));
}

int csm_model_catalog_parse_buffer(const char *yaml, size_t len) {
	if (yaml == NULL || len == 0U) {
		return FALSE;
	}

	csm_model_catalog_reset();

	const char *p = yaml;
	const char *end = yaml + len;
	char line[128];
	int line_pos = 0;

	while (p < end) {
		if (*p == '\n' || *p == '\r') {
			if (line_pos > 0) {
				line[line_pos] = '\0';
				if (parse_line(line) != TRUE) {
					CSM_ERR("[CATALOG] Parse error near: %s", line);
					csm_model_catalog_reset();
					return FALSE;
				}
			}
			line_pos = 0;
			if (*p == '\r' && *(p + 1) == '\n') {
				p++;
			}
		} else {
			if (line_pos >= (int)sizeof(line) - 1) {
				CSM_ERR("[CATALOG] Line too long");
				csm_model_catalog_reset();
				return FALSE;
			}
			line[line_pos++] = *p;
		}
		p++;
	}

	if (line_pos > 0) {
		line[line_pos] = '\0';
		if (parse_line(line) != TRUE) {
			CSM_ERR("[CATALOG] Parse error near: %s", line);
			csm_model_catalog_reset();
			return FALSE;
		}
	}

	CSM_LOG("[CATALOG] Loaded %d entries", catalog_count);
	return TRUE;
}

int csm_model_catalog_load_yaml(const char *filename) {
	if (filename == NULL) {
		CSM_ERR("[CATALOG] Null filename");
		return FALSE;
	}

	FILE *fp = fopen(filename, "r");
	if (fp == NULL) {
		CSM_ERR("[CATALOG] Cannot open file: %s", filename);
		return FALSE;
	}

	if (fseek(fp, 0, SEEK_END) != 0) {
		fclose(fp);
		CSM_ERR("[CATALOG] Cannot seek file: %s", filename);
		return FALSE;
	}
	long fsize = ftell(fp);
	if (fsize < 0L || fseek(fp, 0, SEEK_SET) != 0) {
		fclose(fp);
		CSM_ERR("[CATALOG] Cannot determine file size: %s", filename);
		return FALSE;
	}

	char buf[8192];
	if (fsize <= 0L || fsize >= (long)sizeof(buf)) {
		fclose(fp);
		CSM_ERR("[CATALOG] Invalid file size: %ld", fsize);
		return FALSE;
	}

	size_t nread = fread(buf, 1, (size_t)fsize, fp);
	fclose(fp);
	if (nread != (size_t)fsize) {
		CSM_ERR("[CATALOG] Short read: %s", filename);
		return FALSE;
	}

	buf[nread] = '\0';
	return csm_model_catalog_parse_buffer(buf, nread);
}

int csm_model_catalog_count(void) {
	return catalog_count;
}

const csm_object_t *csm_model_catalog_get(int index) {
	if (index < 0 || index >= catalog_count) {
		return NULL;
	}
	return &catalog_entries[index];
}
