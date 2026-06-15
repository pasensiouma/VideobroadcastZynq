// ===================================================
// INCLUDES INTERNOS
// ===================================================
#include <stdio.h>
#include <string.h>

#include "lwip/err.h"
#include "lwip/tcp.h"
#include "lwip/inet.h"
#include "xil_printf.h"
#include "xtime_l.h"

#include "../vdma/vdma_utils.h"
#include "../frame_params.h"
#include "tcpServer.h"

// ===================================================
// DEFINES INTERNOS
// ===================================================
#define TCP_CHUNK_MAX  60000  // Tamaño máximo de cada trozo enviado (valor recomendado entre 32768-60000)
#define TCP_PORT 5000

// ===================================================
// CONSTANTES INTERNOS
// ===================================================

// ===================================================
// VARIABLES GLOBALES Estáticas (solo visibles en este archivo)
// ===================================================
// PCB de la conexión activa
struct tcp_pcb *g_client_pcb = NULL;

// Estado interno del envío del frame actual
static u8 *send_ptr = NULL;				// puntero al frame actual
static u32 send_remaining = 0;			// bytes pendientesp por enviar
static int header_sent = 0;				// 1 = cabecera enviada
int TcpBusy = 0;						// 1 = enviando frame

// Variables estáticas de medición
static XTime sendStartTime = 0;
static int frameCountTcp = 0;
static XTime lastTimeTcp = 0;
static u64 bytesEnviados = 0;

// ===================================================
// VARIABLES GLOBALES Globales (visibles por otros módulos)
// ===================================================

// ===================================================
// PROTOTIPO DE FUNCIONES INTERNAS (STATICS)
// ===================================================

// ===================================================
// IMPLEMENTACIÓN DE FUNCIONES PÚBLICAS (.h)
// ===================================================
// Función para enviar el frame:
// 		- Fragmenta el envío del frame según tcp_sndbuf()
// 		- Envía el frame en fragmentos hasta completarlo con el siguiente formato:
//			- 1) Primer fragmento: 		"FRAM" + [metadatos del frame] + [datos]
//			- 2) Fragmentos restantes:	[tamaño enviado] + [datos]
int tcpTransferData(int vdmaFlagNewFrameReady, u8 *vdmaFreeFramePtr, int tcpFrameSize, int frameW, int frameH, int frameBpp)
{
    err_t err;

    // Si no hay cliente conectado: No hacer nada
    if (g_client_pcb == NULL)
        return 0;

    // Si se cumplen las siguientes condiciones: Coger un nuevo frame
    // - no hay frame en curso
    // - hay un frame nuevo marcado por la isr
    // - El tcp esta libre
    if ((send_remaining == 0) && (vdmaFlagNewFrameReady) && (TcpBusy == 0)) {
        send_ptr       = vdmaFreeFramePtr;
        send_remaining = tcpFrameSize;
        header_sent    = 0;
        TcpBusy        = 1;

        // MARCA DE TIEMPO: Inicio cuando cogemos frame
        XTime_GetTime(&sendStartTime);
    }

    // Si no hay nada que enviar: No hacer nada
    if (send_remaining == 0)
        return 0;

    // 1) Enviar cabecera si aún no se ha enviado
    if (!header_sent)
    {
    	// Cabecera: "FRAM" + width + height + bpp
        char header[16];

    	// "FRAM"
        header[0] = 'F';
        header[1] = 'R';
        header[2] = 'A';
        header[3] = 'M';

        // Metadatos del frame (width + height + bpp)
        u32 w = htonl(frameW);
        u32 h = htonl(frameH);
        u32 b = htonl(frameBpp);

        memcpy(&header[4],  &w, 4);
        memcpy(&header[8], &h, 4);
        memcpy(&header[12], &b, 4);

        // Comprobar espacio en el buffer TCP
        if (tcp_sndbuf(g_client_pcb) < sizeof(header)) {
            // no hay espacio todavía, lo intentaremos en la próxima llamada
            return 0;
        }

        // Enviar cabecera
        err = tcp_write(g_client_pcb, header, sizeof(header), TCP_WRITE_FLAG_COPY);
        if (err != ERR_OK) {
            xil_printf("tcp_write header error = %d\r\n", err);
            return -1;
        }

        // Marcar que se ha enviado el "primer fragmento"
        header_sent = 1;
    }

    // 2) Enviar datos del frame en fragmentos
    while (send_remaining > 0)
    {
        u16_t sndbuf = tcp_sndbuf(g_client_pcb);
        if (sndbuf == 0) {
            // buffer de envío lleno, continuaremos en la próxima iteración
            break;
        }

        // Calcular tamaño del fragmento
        u16_t chunk = (send_remaining > TCP_CHUNK_MAX) ? TCP_CHUNK_MAX : (u16_t)send_remaining;
        if (chunk > sndbuf)
            chunk = sndbuf;

        // Enviar fragmento por tcp
        err = tcp_write(g_client_pcb, send_ptr, chunk, TCP_WRITE_FLAG_COPY);
        if (err != ERR_OK) {
            xil_printf("tcp_write data error = %d\r\n", err);
            break;
        }

        // Avanzar puntero a siguiente fragmento (si se ha enviado)
        send_ptr       += chunk;
        send_remaining -= chunk;

        // calculo de chunkcs envaidos
        bytesEnviados += chunk;
    }

    // Forzar envio
    tcp_output(g_client_pcb);

    // 3) Si el frame ha terminado, librerar estado
    if (send_remaining == 0)
    {
        header_sent = 0;
        TcpBusy = 0;

        // MARCA DE FIN: cuando el frame se ha volcado completamente a lwIP
        XTime sendEndTime;
        XTime_GetTime(&sendEndTime);

        // Tiempo en microsegundos
        u32 elapsed_us = (u32)((sendEndTime - sendStartTime) * 1000000 / COUNTS_PER_SECOND);

        // Contador de FPS TCP
        frameCountTcp++;
        if ((sendEndTime - lastTimeTcp) >= COUNTS_PER_SECOND)
        {
            xil_printf("FPS TCP:          %d\r\n", frameCountTcp);
            xil_printf("Tiempo por frame: %u us\r\n", elapsed_us);
            xil_printf("Throughput:       %u KB/s\r\n",
                       (u32)((u64)tcpFrameSize * 1000000 / elapsed_us / 1024));
            xil_printf("Bytes enviados:   %u KB\r\n", (u32)(bytesEnviados / 1024));

            // Resetear
            frameCountTcp = 0;
            bytesEnviados = 0;
            lastTimeTcp   = sendEndTime;
        }
    }

    return 0;
}

// Función que se ejecuta cuando el cliente envía datos o cierra la conexión
// Nota: Esta aplicacion no utiliza datos entrantes, por lo que solo se utiliza para detectar desconexiones
err_t recv_callback(void *arg, struct tcp_pcb *tpcb,
                    struct pbuf *p, err_t err)
{
	// Si cliente cerró conexión
    if (!p) {
        tcp_close(tpcb);
        tcp_recv(tpcb, NULL);

        if (tpcb == g_client_pcb) {
            g_client_pcb = NULL;

            // Resetear estado del transmisor
            send_ptr = NULL;
            send_remaining = 0;
            header_sent = 0;
            TcpBusy = 0;
        }

        return ERR_OK;
    }

    // Consumir datos recibidos (no los utilizamos)
    tcp_recved(tpcb, p->len);
    pbuf_free(p);
    return ERR_OK;
}

// Función que se ejecuta cuando un cliente se conecta al puerto especificado
err_t accept_callback(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    static int connection = 1;

    // Registrar callback de recepción
    tcp_recv(newpcb, recv_callback);

    // Argumento opcional (no usado)
    tcp_arg(newpcb, (void*)(UINTPTR)connection);
    connection++;

    // Guardar PCB del cliente activo
    g_client_pcb = newpcb;

    xil_printf("Nueva conexión TCP aceptada\r\n");

    return ERR_OK;
}

// Inicialización del servidor TCP en el puerto 7
int tcpServerInit(void)
{
    struct tcp_pcb *pcb;
    err_t err;

    xil_printf("TCP_SERVER - START INICIALIZACION\n\r");

    // Crear PCB
    pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
    if (!pcb) {
        xil_printf("Error creating PCB. Out of Memory\n\r");
        return -1;
    }

    // Asociar al puerto
    err = tcp_bind(pcb, IP_ANY_TYPE, TCP_PORT);
    if (err != ERR_OK) {
        xil_printf("Unable to bind to port %d: err = %d\n\r", TCP_PORT, err);
        return -2;
    }

    //No necesitamos argumento para el PCB del servidor
    tcp_arg(pcb, NULL);

    // Escuchar conexiones entrantes
    pcb = tcp_listen(pcb);
    if (!pcb) {
        xil_printf("Out of memory while tcp_listen\n\r");
        return -3;
    }

    // Registrar callback de aceptación
    tcp_accept(pcb, accept_callback);

    xil_printf("TCP video server started @ port %d\n\r", TCP_PORT);
    xil_printf("TCP_SERVER - FIN INICIALIZACION\n\n\r");

    return 0;
}

int tcpServerIsBusy()
{
	return TcpBusy;
}

// ===================================================
// IMPLEMENTACIÓN DE FUNCIONES INTERNAS (STATICS)
// ===================================================
