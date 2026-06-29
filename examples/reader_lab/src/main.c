/**
 * reader_lab — TCP client for lab meter (Public / Reader / Configurator profiles).
 *
 * Usage:
 *   reader_lab public   [host] [port]
 *   reader_lab reader   [host] [port]
 *   reader_lab config   [host] [port]
 *   reader_lab config   192.168.1.116 4059 0.0.1.0.0.255
 *   reader_lab public   127.0.0.1 4059 0.0.10.0.0.255 class=3 set-u32=42
 *   reader_lab public   127.0.0.1 4059 0.0.10.0.0.255 class=3 action=1
 *   reader_lab public   127.0.0.1 4059 0.0.1.0.0.255 sap=1 attr=2 set-hex=090c...
 *
 * Defaults: host=192.168.1.116 port=4059
 */

#include "opendlms_reader.h"
#include "reader_hal.h"

#include "csm_association.h"
#include "csm_axdr_codec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET reader_sock_t;
#define READER_SOCK_INVALID INVALID_SOCKET
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int reader_sock_t;
#define READER_SOCK_INVALID (-1)
#endif

static reader_sock_t g_sock = READER_SOCK_INVALID;

static int platform_init(void)
{
#if defined(_WIN32) || defined(WIN32)
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        fprintf(stderr, "WSAStartup failed\n");
        return -1;
    }
#endif
    return 0;
}

static void platform_cleanup(void)
{
#if defined(_WIN32) || defined(WIN32)
    WSACleanup();
#endif
}

static int tcp_connect(const char *host, int port)
{
    struct sockaddr_in addr;

    g_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (g_sock == READER_SOCK_INVALID)
    {
        perror("socket");
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((uint16_t)port);

    if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0)
    {
        fprintf(stderr, "Invalid host: %s\n", host);
        return -1;
    }

    if (connect(g_sock, (struct sockaddr *)&addr, sizeof(addr)) != 0)
    {
        perror("connect");
        return -1;
    }

#if defined(_WIN32) || defined(WIN32)
    {
        DWORD tv_ms = OPENDLMS_READER_RX_TIMEOUT_MS;
        setsockopt(g_sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv_ms, sizeof(tv_ms));
    }
#else
    {
        struct timeval tv;
        tv.tv_sec  = (time_t)(OPENDLMS_READER_RX_TIMEOUT_MS / 1000U);
        tv.tv_usec = (suseconds_t)((OPENDLMS_READER_RX_TIMEOUT_MS % 1000U) * 1000U);
        setsockopt(g_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }
#endif

    printf("Connected to %s:%d\n", host, port);
    return 0;
}

static void tcp_close(void)
{
    if (g_sock != READER_SOCK_INVALID)
    {
#if defined(_WIN32) || defined(WIN32)
        shutdown(g_sock, SD_BOTH);
        closesocket(g_sock);
#else
        shutdown(g_sock, SHUT_RDWR);
        close(g_sock);
#endif
        g_sock = READER_SOCK_INVALID;
    }
}

static int tcp_io_write(void *ctx, const uint8_t *buf, uint32_t len)
{
    (void)ctx;
    uint32_t total = 0U;

    while (total < len)
    {
#if defined(_WIN32) || defined(WIN32)
        int sent = send(g_sock, (const char *)buf + total, (int)(len - total), 0);
#else
        ssize_t sent = send(g_sock, buf + total, len - total, 0);
#endif
        if (sent <= 0)
        {
            return -1;
        }
        total += (uint32_t)sent;
    }

    return (int)total;
}

static int tcp_io_read(void *ctx, uint8_t *buf, uint32_t len, uint32_t timeout_ms)
{
    (void)ctx;
#if defined(_WIN32) || defined(WIN32)
    DWORD tv_ms = timeout_ms;
    setsockopt(g_sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv_ms, sizeof(tv_ms));
#else
    struct timeval tv;
    tv.tv_sec  = (time_t)(timeout_ms / 1000U);
    tv.tv_usec = (suseconds_t)((timeout_ms % 1000U) * 1000U);
    setsockopt(g_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

    uint32_t total = 0U;
    while (total < len)
    {
#if defined(_WIN32) || defined(WIN32)
        int n = recv(g_sock, (char *)buf + total, (int)(len - total), 0);
#else
        ssize_t n = recv(g_sock, buf + total, len - total, 0);
#endif
        if (n <= 0)
        {
            return (total > 0U) ? (int)total : (int)n;
        }
        total += (uint32_t)n;
    }

    return (int)total;
}

static int parse_obis_part(const char **cursor, unsigned int *value, char delimiter)
{
    unsigned int parsed = 0U;
    const char *p = *cursor;

    if ((p == NULL) || (*p < '0') || (*p > '9'))
    {
        return -1;
    }

    while ((*p >= '0') && (*p <= '9'))
    {
        parsed = (parsed * 10U) + (unsigned int)(*p - '0');
        if (parsed > 255U)
        {
            return -1;
        }
        p++;
    }

    if ((delimiter != '\0') && (*p != delimiter))
    {
        return -1;
    }
    if ((delimiter == '\0') && (*p != '\0'))
    {
        return -1;
    }

    *value = parsed;
    *cursor = (delimiter != '\0') ? p + 1 : p;
    return 0;
}

static int parse_obis(const char *str, csm_obis_code *obis, uint16_t *class_id)
{
    unsigned int vals[6];
    const char *cursor = str;

    if ((str == NULL) || (obis == NULL) || (class_id == NULL))
    {
        return -1;
    }

    for (uint32_t i = 0U; i < 6U; i++)
    {
        char delimiter = (i == 5U) ? '\0' : '.';
        if (parse_obis_part(&cursor, &vals[i], delimiter) != 0)
        {
            return -1;
        }
    }

    obis->A = (uint8_t)vals[0];
    obis->B = (uint8_t)vals[1];
    obis->C = (uint8_t)vals[2];
    obis->D = (uint8_t)vals[3];
    obis->E = (uint8_t)vals[4];
    obis->F = (uint8_t)vals[5];

    if (vals[2] == 1U)
    {
        *class_id = 8U;
    }
    else if (vals[2] == 40U)
    {
        *class_id = 15U;
    }
    else if (vals[2] == 43U)
    {
        *class_id = 1U;
    }
    else
    {
        *class_id = (uint16_t)vals[2];
    }

    return 0;
}

static int parse_decimal_int(const char *str, int min_value, int max_value, int *out)
{
    int value = 0;

    if ((str == NULL) || (out == NULL) || (min_value > max_value) ||
        (*str < '0') || (*str > '9'))
    {
        return -1;
    }

    while ((*str >= '0') && (*str <= '9'))
    {
        int digit = *str - '0';
        if (value > ((max_value - digit) / 10))
        {
            return -1;
        }
        value = (value * 10) + digit;
        str++;
    }

    if ((*str != '\0') || (value < min_value))
    {
        return -1;
    }

    *out = value;
    return 0;
}

static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9')
    {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f')
    {
        return 10 + (c - 'a');
    }
    if (c >= 'A' && c <= 'F')
    {
        return 10 + (c - 'A');
    }
    return -1;
}

static int parse_hex(const char *hex, uint8_t *out, uint32_t out_size, uint32_t *out_len)
{
    uint32_t len = 0U;

    if (!hex || !out || !out_len)
    {
        return -1;
    }

    while (*hex != '\0')
    {
        while (*hex == ':' || *hex == '-' || *hex == ' ')
        {
            hex++;
        }
        if (*hex == '\0')
        {
            break;
        }

        int hi = hex_nibble(*hex++);
        int lo = (*hex != '\0') ? hex_nibble(*hex++) : -1;
        if (hi < 0 || lo < 0 || len >= out_size)
        {
            return -1;
        }
        out[len++] = (uint8_t)((hi << 4) | lo);
    }

    *out_len = len;
    return 0;
}

static void setup_profile(const char *profile,
                          opendlms_reader_auth_t *auth,
                          csm_asso_config *asso_cfg,
                          uint8_t plain_hls)
{
    memset(asso_cfg, 0, sizeof(*asso_cfg));

    if (strcmp(profile, "reader") == 0)
    {
        opendlms_reader_auth_reader(auth);
        asso_cfg->llc.ssap               = 32U;
        asso_cfg->llc.dsap               = 1U;
        asso_cfg->application_context    = (uint8_t)LN_REF;
        reader_hal_set_lls_password(32U, "00000001");
    }
    else if (strcmp(profile, "config") == 0 || strcmp(profile, "configurator") == 0)
    {
        static const uint8_t lab_client_title[8] = {
            0x41U, 0x42U, 0x43U, 0x44U, 0x45U, 0x46U, 0x47U, 0x48U
        };

        opendlms_reader_auth_configurator(auth);
        if (plain_hls != 0U)
        {
            auth->ciphering           = 0U;
            auth->application_context = (uint8_t)LN_REF;
        }
        asso_cfg->llc.ssap            = 48U;
        asso_cfg->llc.dsap            = 48U;
        asso_cfg->application_context = plain_hls ? (uint8_t)LN_REF : (uint8_t)LN_REF_WITH_CYPHERING;
        reader_hal_set_system_title(lab_client_title);
        reader_hal_keyring_set_hex(48U, "000102030405060708090A0B0C0D0E0F",
                                   "D0D1D2D3D4D5D6D7D8D9DADBDCDDDEDF",
                                   "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
        reader_hal_set_dedicated_key_hex(48U, "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
        reader_hal_set_invocation_counter(48U, plain_hls ? 1U : 2U);
    }
    else
    {
        opendlms_reader_auth_public(auth);
        asso_cfg->llc.ssap            = 16U;
        asso_cfg->llc.dsap            = 1U;
        asso_cfg->application_context = (uint8_t)LN_REF;
    }

    asso_cfg->conformance       = 0xFFFFFFFFU;
    asso_cfg->is_auto_connected = 0U;
}

static int is_known_profile(const char *profile)
{
    return (strcmp(profile, "public") == 0) ||
           (strcmp(profile, "reader") == 0) ||
           (strcmp(profile, "config") == 0) ||
           (strcmp(profile, "configurator") == 0);
}

int main(int argc, char **argv)
{
    const char *profile = "public";
    const char *host    = "192.168.1.116";
    int           port  = 4059;
    int           obis_arg = -1;
    int           class_override = -1;
    int           attr_override = 2;
    int           set_u32 = -1;
    int           action_method = -1;
    int           source_wport_override = -1;
    int           dest_wport_override = -1;
    uint16_t      dest_wport = OPENDLMS_READER_TCP_LOGICAL_DEVICE;
    const char   *set_hex = NULL;
    uint8_t       set_hex_buf[256];
    uint32_t      set_hex_len = 0U;
    uint8_t       sync_ic = 0U;
    uint8_t       plain_hls = 0U;

    opendlms_reader_t            reader;
    opendlms_reader_session_t    session;
    opendlms_reader_auth_t       auth;
    csm_asso_config              asso_cfg;
    opendlms_reader_io_t         io;
    csm_response                 response;
    csm_obis_code                obis;
    uint16_t                     class_id;

    if (argc < 2)
    {
        printf("Usage: %s <public|reader|config> [host] [port] [obis] [plain] [class=N] [attr=N] [sap=N] [dest=N] [set-u32=N|set-hex=HEX|action=N]\n", argv[0]);
        return 1;
    }

    profile = argv[1];
    if (!is_known_profile(profile))
    {
        fprintf(stderr, "Bad profile: %s\n", profile);
        return 1;
    }

    if (argc >= 3)
    {
        host = argv[2];
    }
    if (argc >= 4)
    {
        if (parse_decimal_int(argv[3], 1, 65535, &port) != 0)
        {
            fprintf(stderr, "Bad port: %s\n", argv[3]);
            return 1;
        }
    }

    for (int i = 4; i < argc; i++)
    {
        if (strcmp(argv[i], "sync-ic") == 0)
        {
            sync_ic = 1U;
        }
        else if (strcmp(argv[i], "plain") == 0)
        {
            plain_hls = 1U;
        }
        else if (strncmp(argv[i], "class=", 6) == 0)
        {
            if (parse_decimal_int(argv[i] + 6, 0, 65535, &class_override) != 0)
            {
                fprintf(stderr, "Bad class id: %s\n", argv[i] + 6);
                return 1;
            }
        }
        else if (strncmp(argv[i], "attr=", 5) == 0)
        {
            if (parse_decimal_int(argv[i] + 5, 1, 255, &attr_override) != 0)
            {
                fprintf(stderr, "Bad attr id: %s\n", argv[i] + 5);
                return 1;
            }
        }
        else if (strncmp(argv[i], "sap=", 4) == 0)
        {
            if (parse_decimal_int(argv[i] + 4, 1, 65535, &source_wport_override) != 0)
            {
                fprintf(stderr, "Bad SAP: %s\n", argv[i] + 4);
                return 1;
            }
        }
        else if (strncmp(argv[i], "dest=", 5) == 0)
        {
            if (parse_decimal_int(argv[i] + 5, 1, 65535, &dest_wport_override) != 0)
            {
                fprintf(stderr, "Bad destination SAP: %s\n", argv[i] + 5);
                return 1;
            }
        }
        else if (strncmp(argv[i], "set-u32=", 8) == 0)
        {
            if (parse_decimal_int(argv[i] + 8, 0, 2147483647, &set_u32) != 0)
            {
                fprintf(stderr, "Bad set-u32 value: %s\n", argv[i] + 8);
                return 1;
            }
        }
        else if (strncmp(argv[i], "set-hex=", 8) == 0)
        {
            set_hex = argv[i] + 8;
        }
        else if (strncmp(argv[i], "action=", 7) == 0)
        {
            if (parse_decimal_int(argv[i] + 7, 1, 255, &action_method) != 0)
            {
                fprintf(stderr, "Bad action id: %s\n", argv[i] + 7);
                return 1;
            }
        }
        else
        {
            obis_arg = i;
        }
    }

    if (obis_arg < 0)
    {
        obis.A = 0U;
        obis.B = 0U;
        obis.C = 1U;
        obis.D = 0U;
        obis.E = 0U;
        obis.F = 255U;
        class_id = 8U;
    }
    else if (parse_obis(argv[obis_arg], &obis, &class_id) < 0)
    {
        fprintf(stderr, "Bad OBIS: %s\n", argv[obis_arg]);
        return 1;
    }
    if (class_override >= 0)
    {
        class_id = (uint16_t)class_override;
    }
    if (attr_override <= 0 || attr_override > 255)
    {
        fprintf(stderr, "Bad attr id: %d\n", attr_override);
        return 1;
    }
    if (set_hex != NULL)
    {
        if (parse_hex(set_hex, set_hex_buf, sizeof(set_hex_buf), &set_hex_len) < 0 ||
            set_hex_len == 0U)
        {
            fprintf(stderr, "Bad set-hex payload\n");
            return 1;
        }
    }

    if (platform_init() < 0)
    {
        return 1;
    }

    reader_hal_init();
    reader_hal_sys_init();
    setup_profile(profile, &auth, &asso_cfg, plain_hls);
    if (source_wport_override > 0)
    {
        asso_cfg.llc.ssap = (uint16_t)source_wport_override;
    }
    dest_wport = asso_cfg.llc.dsap;
    if (dest_wport_override > 0)
    {
        dest_wport = (uint16_t)dest_wport_override;
    }

    if (sync_ic != 0U)
    {
        fprintf(stderr,
                "Invocation counter sync requires the security-client handshake path; "
                "seed the counter with reader_hal_set_invocation_counter() for now.\n");
        platform_cleanup();
        return 1;
    }

    if (tcp_connect(host, port) < 0)
    {
        platform_cleanup();
        return 1;
    }

    if (opendlms_reader_init(&reader, &asso_cfg, 1U) < 0)
    {
        tcp_close();
        platform_cleanup();
        return 1;
    }

    io.ctx            = NULL;
    io.write          = tcp_io_write;
    io.read           = tcp_io_read;
    io.rx_timeout_ms  = OPENDLMS_READER_RX_TIMEOUT_MS;

    opendlms_reader_session_init(&session, &reader, io, &asso_cfg);
    opendlms_reader_session_set_transport(&session, OPENDLMS_READER_TRANSPORT_TCP_WRAPPER,
                                          asso_cfg.llc.ssap,
                                          dest_wport);
    opendlms_reader_session_set_auth(&session, &auth);
    if (plain_hls != 0U)
    {
        printf("Configurator: plain HLS (context 1)\n");
    }
    else if (strcmp(profile, "config") == 0 || strcmp(profile, "configurator") == 0)
    {
        printf("Configurator: glo-ciphering (context 3)\n");
    }

    printf("=== Profile: %s (SAP %u) ===\n", profile, (unsigned)asso_cfg.llc.ssap);

    if (opendlms_reader_connect(&session) < 0)
    {
        fprintf(stderr, "Connect failed\n");
        opendlms_reader_disconnect(&session);
        tcp_close();
        platform_cleanup();
        return 1;
    }

    printf("Association OK\n");

    if (set_hex != NULL)
    {
        printf("SET %u.%u.%u.%u.%u.%u class=%u attr=%d hex_len=%lu\n",
               obis.A, obis.B, obis.C, obis.D, obis.E, obis.F,
               (unsigned)class_id, attr_override, (unsigned long)set_hex_len);

        if (opendlms_reader_set(&session, class_id, &obis, (uint8_t)attr_override,
                                set_hex_buf, set_hex_len, &response) < 0)
        {
            fprintf(stderr, "SET failed\n");
        }
        else
        {
            printf("SET OK: service=%d result=%u access=%u\n", (int)response.service,
                   (unsigned)response.result, (unsigned)response.access_result);
        }
    }
    else if (set_u32 >= 0)
    {
        uint8_t set_data[] = {
            AXDR_TAG_UNSIGNED32,
            (uint8_t)(((uint32_t)set_u32 >> 24U) & 0xFFU),
            (uint8_t)(((uint32_t)set_u32 >> 16U) & 0xFFU),
            (uint8_t)(((uint32_t)set_u32 >> 8U) & 0xFFU),
            (uint8_t)((uint32_t)set_u32 & 0xFFU)
        };

        printf("SET %u.%u.%u.%u.%u.%u class=%u attr=%d value=%d\n",
               obis.A, obis.B, obis.C, obis.D, obis.E, obis.F,
               (unsigned)class_id, attr_override, set_u32);

        if (opendlms_reader_set(&session, class_id, &obis, (uint8_t)attr_override,
                                set_data, sizeof(set_data), &response) < 0)
        {
            fprintf(stderr, "SET failed\n");
        }
        else
        {
            printf("SET OK: service=%d result=%u access=%u\n", (int)response.service,
                   (unsigned)response.result, (unsigned)response.access_result);
        }
    }
    else if (action_method >= 0)
    {
        printf("ACTION %u.%u.%u.%u.%u.%u class=%u method=%d\n",
               obis.A, obis.B, obis.C, obis.D, obis.E, obis.F,
               (unsigned)class_id, action_method);

        if (opendlms_reader_action(&session, class_id, &obis, (uint8_t)action_method,
                                   NULL, 0U, &response) < 0)
        {
            fprintf(stderr, "ACTION failed\n");
        }
        else
        {
            printf("ACTION OK: service=%d result=%u access=%u\n", (int)response.service,
                   (unsigned)response.result, (unsigned)response.access_result);
        }
    }
    else
    {
        printf("GET %u.%u.%u.%u.%u.%u class=%u attr=%d\n",
               obis.A, obis.B, obis.C, obis.D, obis.E, obis.F,
               (unsigned)class_id, attr_override);

        if (opendlms_reader_get(&session, class_id, &obis, (uint8_t)attr_override,
                                &response) < 0)
        {
            fprintf(stderr, "GET failed\n");
        }
        else
        {
            printf("GET OK: service=%d result=%u access=%u\n", (int)response.service,
                   (unsigned)response.result, (unsigned)response.access_result);
        }
    }

    opendlms_reader_disconnect(&session);
    tcp_close();
    platform_cleanup();
    return 0;
}
