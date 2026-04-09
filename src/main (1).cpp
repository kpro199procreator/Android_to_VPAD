/*
 * android_to_vpad — Fase 1
 * Plugin WUPS: recibe input desde Android/Termux via TCP
 * e inyecta los botones en VPADRead del Wii U.
 *
 * Protocolo: paquetes UDP/TCP de 12 bytes
 *   [0..3]  uint32_t  hold    (botones mantenidos)
 *   [4..7]  uint32_t  trigger (botones recién presionados)
 *   [8..11] uint32_t  release (botones recién soltados)
 *
 * Puerto por defecto: 4322
 */

#include <wups.h>
#include <wups/config/WUPSConfigItemBoolean.h>
#include <wups/config/WUPSConfigItemIntegerRange.h>

#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <coreinit/systeminfo.h>
#include <coreinit/dynload.h>
#include <nsysnet/socket.h>
#include <vpad/input.h>

#include <cstring>
#include <cstdio>
#include <cstdlib>

#include <notifications/notifications.h>
#include "logger.h"

// ── Metadatos del plugin ──────────────────────────────────────────────────────
WUPS_PLUGIN_NAME("AndroidToVPad");
WUPS_PLUGIN_DESCRIPTION("Receive Android controller input over TCP/UDP");
WUPS_PLUGIN_VERSION("v0.1.0");
WUPS_PLUGIN_AUTHOR("Sucu");
WUPS_PLUGIN_LICENSE("GPL-3.0");

WUPS_USE_WUT_DEVOPTAB();
WUPS_USE_STORAGE("android_to_vpad");

// ── Constantes ────────────────────────────────────────────────────────────────
#define ATV_PORT_DEFAULT    4322
#define ATV_PACKET_SIZE     12      // 3 x uint32_t
#define ATV_STACK_SIZE      (4096 * 4)
#define ATV_INPUT_TIMEOUT   5000    // ms: si no hay paquete en 5s, silencio

// ── Estado global ─────────────────────────────────────────────────────────────
static bool     g_enabled       = true;
static int32_t  g_port          = ATV_PORT_DEFAULT;
static bool     g_passthrough   = true;   // seguir pasando input real del GamePad

// Input virtual (escrito por el thread de red, leído en VPADRead)
static volatile uint32_t g_virt_hold    = 0;
static volatile uint32_t g_virt_trigger = 0;
static volatile uint32_t g_virt_release = 0;
static volatile bool     g_has_input    = false;
static volatile uint32_t g_last_input_time = 0;  // en ticks de OSGetTick

// Thread del servidor TCP
static OSThread  g_serverThread;
static uint8_t   g_serverStack[ATV_STACK_SIZE];
static bool      g_serverRunning = false;
static int        g_serverSocket = -1;

// ── Protocolo ─────────────────────────────────────────────────────────────────
#pragma pack(push, 1)
struct AndroidInputPacket {
    uint32_t hold;
    uint32_t trigger;
    uint32_t release;
};
#pragma pack(pop)

// ── Thread del servidor TCP ───────────────────────────────────────────────────
static int serverThreadFunc(int argc, const char **argv)
{
    (void)argc; (void)argv;

    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        log_printf("ATV: socket() failed\n");
        return -1;
    }

    // SO_REUSEADDR para evitar errores al recargar el plugin
    int yes = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    // Timeout de recv: 1 segundo para poder revisar g_serverRunning
    struct timeval tv;
    tv.tv_sec  = 1;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons((uint16_t)g_port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        log_printf("ATV: bind() failed on port %d\n", g_port);
        socketclose(sock);
        return -1;
    }

    if (listen(sock, 2) < 0) {
        log_printf("ATV: listen() failed\n");
        socketclose(sock);
        return -1;
    }

    g_serverSocket = sock;
    log_printf("ATV: server listening on port %d\n", g_port);

    NotificationModule_AddInfoNotificationWithDuration("AndroidToVPad: ready", 3.0f);

    while (g_serverRunning)
    {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);

        int client = accept(sock, (struct sockaddr *)&clientAddr, &clientLen);
        if (client < 0) {
            // timeout normal (SO_RCVTIMEO en accept no aplica, pero usamos
            // un non-blocking accept indirecto via select-style en un loop)
            continue;
        }

        // Heredar timeout al cliente también
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        log_printf("ATV: client connected\n");
        NotificationModule_AddInfoNotificationWithDuration("AndroidToVPad: client connected", 2.0f);

        // Loop de recepción de paquetes de este cliente
        AndroidInputPacket pkt;
        while (g_serverRunning)
        {
            ssize_t received = recv(client, &pkt, ATV_PACKET_SIZE, MSG_WAITALL);
            if (received <= 0) {
                // desconexión o timeout
                break;
            }
            if (received < (ssize_t)ATV_PACKET_SIZE) {
                continue; // paquete incompleto
            }

            // Aplicar big-endian → little-endian (el Wii U es big-endian,
            // pero desde Android enviamos en network byte order = big-endian,
            // así que ntohl está correcto para ambos lados)
            g_virt_hold    = ntohl(pkt.hold);
            g_virt_trigger = ntohl(pkt.trigger);
            g_virt_release = ntohl(pkt.release);
            g_has_input    = true;
            g_last_input_time = (uint32_t)(OSGetTime() / OSTimerClockSpeed);
        }

        socketclose(client);
        g_has_input = false;
        log_printf("ATV: client disconnected\n");
        NotificationModule_AddInfoNotificationWithDuration("AndroidToVPad: client disconnected", 2.0f);
    }

    socketclose(sock);
    g_serverSocket = -1;
    log_printf("ATV: server stopped\n");
    return 0;
}

static void startServer()
{
    if (g_serverRunning) return;

    g_serverRunning = true;
    OSCreateThread(&g_serverThread,
                   serverThreadFunc,
                   0, nullptr,
                   g_serverStack + ATV_STACK_SIZE,
                   ATV_STACK_SIZE,
                   16,   // prioridad
                   OS_THREAD_ATTRIB_AFFINITY_ANY);
    OSResumeThread(&g_serverThread);
    log_printf("ATV: server thread started\n");
}

static void stopServer()
{
    if (!g_serverRunning) return;
    g_serverRunning = false;
    g_has_input     = false;

    // Cerrar el socket de escucha para forzar salida del accept()
    if (g_serverSocket >= 0) {
        socketclose(g_serverSocket);
        g_serverSocket = -1;
    }

    OSJoinThread(&g_serverThread, nullptr);
    log_printf("ATV: server thread joined\n");
}

// ── Hooks de ciclo de vida ────────────────────────────────────────────────────
INITIALIZE_PLUGIN()
{
    WHBLogUdpInit();
    log_printf("ATV: INITIALIZE_PLUGIN\n");

    // Leer configuración persistente
    WUPSStorageError err;
    err = WUPSStorageAPI::GetOrStoreDefault("enabled",     g_enabled,     true);
    err = WUPSStorageAPI::GetOrStoreDefault("port",        g_port,        ATV_PORT_DEFAULT);
    err = WUPSStorageAPI::GetOrStoreDefault("passthrough", g_passthrough, true);
    (void)err;

    // Init Notifications
    WUPSConfigAPIOptionsV1 opts = { .name = "AndroidToVPad" };
    WUPSConfigAPI_Init(opts,
        [](WUPSConfigCategoryHandle root) -> WUPSConfigAPICallbackStatus {
            WUPSConfigCategoryHandle cat;
            WUPSConfigAPI_Category_CreateHandled(root, "General", &cat);

            WUPSConfigItemBoolean_AddToCategoryHandled(
                cat, "enabled", "Enable plugin",
                true, g_enabled,
                [](ConfigItemBoolean *item, bool val) {
                    g_enabled = val;
                    WUPSStorageAPI::Store("enabled", g_enabled);
                    if (!g_enabled) {
                        stopServer();
                    } else if (!g_serverRunning) {
                        startServer();
                    }
                });

            WUPSConfigItemBoolean_AddToCategoryHandled(
                cat, "passthrough", "Pass real GamePad input",
                true, g_passthrough,
                [](ConfigItemBoolean *item, bool val) {
                    g_passthrough = val;
                    WUPSStorageAPI::Store("passthrough", g_passthrough);
                });

            WUPSConfigItemIntegerRange_AddToCategoryHandled(
                cat, "port", "TCP Port",
                1024, 65535,
                ATV_PORT_DEFAULT, g_port,
                [](ConfigItemIntegerRange *item, int32_t val) {
                    if (val != g_port) {
                        stopServer();
                        g_port = val;
                        WUPSStorageAPI::Store("port", g_port);
                        if (g_enabled) startServer();
                    }
                });

            return WUPSCONFIG_API_CALLBACK_RESULT_SUCCESS;
        },
        []() { WUPSStorageAPI::SaveStorage(); }
    );
}

DEINITIALIZE_PLUGIN()
{
    stopServer();
    WHBLogUdpDeinit();
    log_printf("ATV: DEINITIALIZE_PLUGIN\n");
}

ON_APPLICATION_START()
{
    socket_lib_init();
    log_printf("ATV: ON_APPLICATION_START\n");
    if (g_enabled && !g_serverRunning) {
        startServer();
    }
}

ON_APPLICATION_ENDS()
{
    log_printf("ATV: ON_APPLICATION_ENDS\n");
    stopServer();
    socket_lib_finish();
}

// ── Parche de VPADRead ────────────────────────────────────────────────────────
DECL_FUNCTION(int32_t, VPADRead,
              VPADChan chan,
              VPADStatus *buffers,
              uint32_t count,
              VPADReadError *outError)
{
    int32_t result = 0;

    if (g_passthrough) {
        result = real_VPADRead(chan, buffers, count, outError);
    } else {
        // Sin passthrough: inicializar buffer a cero
        if (count > 0) {
            memset(buffers, 0, sizeof(VPADStatus) * count);
            if (outError) *outError = VPAD_READ_NO_SAMPLES;
        }
    }

    if (!g_enabled || !g_has_input || chan != VPAD_CHAN_0 || count == 0) {
        return result;
    }

    // Inyectar input virtual del cliente Android
    buffers[0].hold    |= g_virt_hold;
    buffers[0].trigger |= g_virt_trigger;
    buffers[0].release |= g_virt_release;

    // Corregir error si no había datos reales
    if (outError && *outError == VPAD_READ_NO_SAMPLES) {
        *outError = VPAD_READ_SUCCESS;
        if (result <= 0) result = 1;
    }

    return result;
}

WUPS_MUST_REPLACE(VPADRead, WUPS_LOADER_LIBRARY_VPAD, VPADRead);
