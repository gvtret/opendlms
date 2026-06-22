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

static int parse_obis_string(const char *str, csm_obis_code *obis)
{
    unsigned int a = 0U, b = 0U, c = 0U, d = 0U, e = 0U, f = 0U;
    if (sscanf(str, "%u.%u.%u.%u.%u.%u", &a, &b, &c, &d, &e, &f) != 6)
    {
        return FALSE;
    }
    obis->A = (uint8_t)a;
    obis->B = (uint8_t)b;
    obis->C = (uint8_t)c;
    obis->D = (uint8_t)d;
    obis->E = (uint8_t)e;
    obis->F = (uint8_t)f;
    return TRUE;
}

static const char *skip_whitespace(const char *p)
{
    while (*p == ' ' || *p == '\t')
    {
        p++;
    }
    return p;
}

static int starts_with(const char *line, const char *prefix)
{
    return strncmp(line, prefix, strlen(prefix)) == 0;
}

static int parse_line(const char *line)
{
    const char *p = skip_whitespace(line);

    if (*p == '#' || *p == '\0' || *p == '\n' || *p == '\r')
    {
        return TRUE;
    }

    if (starts_with(p, "class_id:"))
    {
        if (catalog_count <= 0)
        {
            return FALSE;
        }
        unsigned int val = 0U;
        if (sscanf(p + 9, "%u", &val) != 1)
        {
            return FALSE;
        }
        catalog_entries[catalog_count - 1].class_id = (uint16_t)val;
    }
    else if (starts_with(p, "logical_name:"))
    {
        if (catalog_count <= 0)
        {
            return FALSE;
        }
        const char *val = skip_whitespace(p + 13);
        char obis_str[32];
        int i = 0;
        if (*val == '"')
        {
            val++;
            while (*val != '"' && *val != '\0' && i < (int)sizeof(obis_str) - 1)
            {
                obis_str[i++] = *val++;
            }
        }
        else
        {
            while (*val != ' ' && *val != '\t' && *val != '\0' && *val != '\n' && i < (int)sizeof(obis_str) - 1)
            {
                obis_str[i++] = *val++;
            }
        }
        obis_str[i] = '\0';
        if (parse_obis_string(obis_str, &catalog_entries[catalog_count - 1].obis) != TRUE)
        {
            return FALSE;
        }
    }
    else if (starts_with(p, "version:"))
    {
        if (catalog_count <= 0)
        {
            return FALSE;
        }
        unsigned int val = 0U;
        if (sscanf(p + 8, "%u", &val) != 1)
        {
            return FALSE;
        }
        catalog_entries[catalog_count - 1].version = (uint8_t)val;
    }
    else if (starts_with(p, "- ") || starts_with(p, "-"))
    {
        if (catalog_count >= (int)CSM_MODEL_CATALOG_MAX_ENTRIES)
        {
            CSM_ERR("[CATALOG] Full, max %u entries", CSM_MODEL_CATALOG_MAX_ENTRIES);
            return FALSE;
        }
        memset(&catalog_entries[catalog_count], 0, sizeof(csm_object_t));
        catalog_entries[catalog_count].id = -1;
        catalog_count++;

        const char *rest = skip_whitespace(p + 1);
        if (starts_with(rest, "class_id:"))
        {
            unsigned int val = 0U;
            if (sscanf(rest + 9, "%u", &val) == 1)
            {
                catalog_entries[catalog_count - 1].class_id = (uint16_t)val;
            }
        }
    }

    return TRUE;
}

void csm_model_catalog_reset(void)
{
    catalog_count = 0;
    memset(catalog_entries, 0, sizeof(catalog_entries));
}

int csm_model_catalog_parse_buffer(const char *yaml, size_t len)
{
    if (yaml == NULL || len == 0U)
    {
        return FALSE;
    }

    csm_model_catalog_reset();

    const char *p = yaml;
    const char *end = yaml + len;
    char line[128];
    int line_pos = 0;

    while (p < end)
    {
        if (*p == '\n' || *p == '\r' || p == end - 1)
        {
            if (line_pos > 0 && line_pos < (int)sizeof(line) - 1)
            {
                line[line_pos] = '\0';
                if (parse_line(line) != TRUE)
                {
                    CSM_ERR("[CATALOG] Parse error near: %s", line);
                    return FALSE;
                }
            }
            line_pos = 0;
            if (*p == '\r' && *(p + 1) == '\n')
            {
                p++;
            }
        }
        else
        {
            if (line_pos < (int)sizeof(line) - 1)
            {
                line[line_pos++] = *p;
            }
        }
        p++;
    }

    CSM_LOG("[CATALOG] Loaded %d entries", catalog_count);
    return TRUE;
}

int csm_model_catalog_load_yaml(const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (fp == NULL)
    {
        CSM_ERR("[CATALOG] Cannot open file: %s", filename);
        return FALSE;
    }

    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (fsize <= 0L || fsize > 8192L)
    {
        fclose(fp);
        CSM_ERR("[CATALOG] Invalid file size: %ld", fsize);
        return FALSE;
    }

    char buf[8192];
    size_t nread = fread(buf, 1, (size_t)fsize, fp);
    fclose(fp);

    buf[nread] = '\0';
    return csm_model_catalog_parse_buffer(buf, nread);
}

int csm_model_catalog_count(void)
{
    return catalog_count;
}

const csm_object_t *csm_model_catalog_get(int index)
{
    if (index < 0 || index >= catalog_count)
    {
        return NULL;
    }
    return &catalog_entries[index];
}
