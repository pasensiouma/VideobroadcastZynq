// ===================================================
// INCLUDES INTERNOS
// ===================================================
#include "xparameters.h"
#include "xuartps.h"

// ===================================================
// DEFINES INTERNOS
// ===================================================
#define UART_BASEADDR            XPAR_PS7_UART_1_BASEADDR			// UART

// ===================================================
// CONSTANTES INTERNOS
// ===================================================

// ===================================================
// VARIABLES GLOBALES
// ===================================================
// Estáticas (solo visibles en este archivo)
// Globales (visibles por otros módulos)

// ===================================================
// PROTOTIPO DE FUNCIONES INTERNAS (STATICS)
// ===================================================

// ===================================================
// IMPLEMENTACIÓN DE FUNCIONES PÚBLICAS (.h)
// ===================================================
// limpia el FIFO de recepción de la UART
void uartInit(void)
{
	xil_printf("UART - START INICIALIZACION\n\r");

    while (XUartPs_IsReceiveData(UART_BASEADDR)) {
        (void)XUartPs_ReadReg(UART_BASEADDR, XUARTPS_FIFO_OFFSET);
    }

    xil_printf("UART - FIN INICIALIZACION\n\n\r");
}

// ===================================================
// IMPLEMENTACIÓN DE FUNCIONES INTERNAS (STATICS)
// ===================================================
