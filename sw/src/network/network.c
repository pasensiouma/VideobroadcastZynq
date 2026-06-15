// ===================================================
// INCLUDES INTERNOS
// ===================================================
#include "xil_printf.h"

#include "lwip/tcp.h"
#include "../platform/platform.h"
#include "../platform/platform_config.h"

#if LWIP_DHCP==1
#include "lwip/dhcp.h"
#endif

// ===================================================
// DEFINES INTERNOS
// ===================================================

// ===================================================
// CONSTANTES INTERNOS
// ===================================================

// ===================================================
// VARIABLES GLOBALES ESTATICAS
// ===================================================
// Interfaz de red lwIP
static struct netif server_netif;		// estructura interna de la interfaz Ethernet
struct netif *echo_netif;				// Puntero global usado por platform_zynq.c

// Variables de red
ip_addr_t ipaddr, netmask, gw;

// ===================================================
// VARIABLES GLOBALES EXTERNAS
// ===================================================
#if LWIP_DHCP==1
extern volatile int dhcp_timoutcntr;
err_t dhcp_start(struct netif *netif);
#endif

// Variables del timer lwIP (definidas en platform_zynq.c)
extern volatile int TcpFastTmrFlag;
extern volatile int TcpSlowTmrFlag;

// ===================================================
// PROTOTIPO DE FUNCIONES INTERNAS (STATICS)
// ===================================================
// Prototipos de otros archivos
void tcp_fasttmr(void);
void tcp_slowtmr(void);
void lwip_init(void);
struct netif * xemac_add(struct netif *netif,
	ip_addr_t *ipaddr, ip_addr_t *netmask, ip_addr_t *gw,
	unsigned char *mac_ethernet_address,
	unsigned mac_baseaddr);

// Funciones propias
static void print_ip_settings(ip_addr_t *ip, ip_addr_t *mask, ip_addr_t *gw);
static void print_ip(char *msg, ip_addr_t *ip);

// ===================================================
// IMPLEMENTACIÓN DE FUNCIONES PÚBLICAS (.h)
// ===================================================
void networkInit(void)
{
	xil_printf("NETWORK - START INICIALIZACION\n\r");

	// Copiar la estructura interna de la interfaz Ethernet al puntero usado por platform_zynq.c
	echo_netif = &server_netif;

	// Inicializar plataforma
	init_platform();

	// Dirección MAC fija de la Zybo (debe ser única en la red)
	unsigned char mac_ethernet_address[] = { 0x00, 0x0a, 0x35, 0x00, 0x01, 0x02 };

    // --------------------------------------------------------------------
    // 1) Configuración inicial de IP (DHCP o fija)
    // --------------------------------------------------------------------
#if LWIP_DHCP==1
    // Si usamos DHCP, la interfaz debe arrancar con IP = 0.0.0.0
    ipaddr.addr  = 0;
    netmask.addr = 0;
    gw.addr      = 0;
#else
    // IP fija configurada manualmente
    IP4_ADDR(&ipaddr,  192, 168,   1, 10);
    IP4_ADDR(&netmask, 255, 255, 255,  0);
    IP4_ADDR(&gw,      192, 168,   1,  1);
#endif

    // --------------------------------------------------------------------
    // 2) Inicializar la pila lwIP
    // --------------------------------------------------------------------
    lwip_init();

    // --------------------------------------------------------------------
    // 3) Añadir la interfaz Ethernet al stack lwIP
    // --------------------------------------------------------------------
    xemac_add(echo_netif, &ipaddr, &netmask, &gw,
                       mac_ethernet_address, PLATFORM_EMAC_BASEADDR);

    // Hacer que esta sea la interfaz por defecto
    netif_set_default(echo_netif);

    // --------------------------------------------------------------------
    // 4) Activar interrupciones del sistema (GIC + timer SCU)
    // --------------------------------------------------------------------
    platform_enable_interrupts();

    // --------------------------------------------------------------------
    // 5) Marcar la interfaz como activa (UP)
    // --------------------------------------------------------------------
    netif_set_up(echo_netif);

    // --------------------------------------------------------------------
    // 6) Obtener IP mediante DHCP (si está habilitado)
    // --------------------------------------------------------------------
#if LWIP_DHCP==1
    dhcp_start(echo_netif);
    dhcp_timoutcntr = 24;   // Tiempo de espera (~24 iteraciones)

    // Esperar a que el router asigne una IP
    while (((echo_netif->ip_addr.addr) == 0) && (dhcp_timoutcntr > 0)) {
        xemacif_input(echo_netif);
    }

    // Si DHCP falla, asignar IP por defecto
    if (dhcp_timoutcntr <= 0 && echo_netif->ip_addr.addr == 0) {
        xil_printf("DHCP Timeout. Using default IP 192.168.1.10\r\n");
        IP4_ADDR(&(echo_netif->ip_addr),  192, 168,   1, 10);
        IP4_ADDR(&(echo_netif->netmask), 255, 255, 255,  0);
        IP4_ADDR(&(echo_netif->gw),      192, 168,   1,  1);
    }

    // Actualizar variables locales con la IP final
    ipaddr.addr  = echo_netif->ip_addr.addr;
    netmask.addr = echo_netif->netmask.addr;
    gw.addr      = echo_netif->gw.addr;
#endif

    // --------------------------------------------------------------------
    // 7) Mostrar por UART la IP final, máscara y gateway
    // --------------------------------------------------------------------
    print_ip_settings(&ipaddr, &netmask, &gw);

    xil_printf("NETWORK - FIN INICIALIZACION\n\n\r");
}

// Procesar timers lwIP
void networkProcesarStackLWIP(void)
{
	// Procesar timers
    if (TcpFastTmrFlag) {
        tcp_fasttmr();
        TcpFastTmrFlag = 0;
    }
    if (TcpSlowTmrFlag) {
        tcp_slowtmr();
        TcpSlowTmrFlag = 0;
    }

    // Procesar tráfico entrante
    xemacif_input(echo_netif);
}

// ===================================================
// IMPLEMENTACIÓN DE FUNCIONES INTERNAS (STATICS)
// ===================================================
// Imprime IP, máscara y gateway de la placa
static void print_ip_settings(ip_addr_t *ip, ip_addr_t *mask, ip_addr_t *gw)
{
    print_ip("Board IP: ", ip);
    print_ip("Netmask : ", mask);
    print_ip("Gateway : ", gw);
}

// Imprime una dirección IP
static void print_ip(char *msg, ip_addr_t *ip)
{
    print(msg);
    xil_printf("%d.%d.%d.%d\n\r", ip4_addr1(ip), ip4_addr2(ip),
               ip4_addr3(ip), ip4_addr4(ip));
}
