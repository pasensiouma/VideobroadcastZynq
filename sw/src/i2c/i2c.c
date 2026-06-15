// ===================================================
// INCLUDES INTERNOS
// ===================================================
#include "xparameters.h"
#include "xil_types.h"
#include "xstatus.h"
#include "xiicps.h"

// ===================================================
// DEFINES INTERNOS
// ===================================================
// Parámetros del bus I2C utilizado por la cámara ov7670
#define I2C_IIC_DEVICE_ID   XPAR_XIICPS_0_DEVICE_ID		// Controlador I2C del PS
#define I2C_IIC_SCLK_RATE   100000						// Frecuencia del i2c (100 Khz)

// ===================================================
// CONSTANTES INTERNOS
// ===================================================

// ===================================================
// VARIABLES GLOBALES
// ===================================================
// Estáticas (solo visibles en este archivo)
static XIicPs IicInstance;		// Instancia del periferico I2C del PS

// Globales (visibles por otros módulos)

// ===================================================
// PROTOTIPO DE FUNCIONES INTERNAS (STATICS)
// ===================================================

// ===================================================
// IMPLEMENTACIÓN DE FUNCIONES PÚBLICAS (.h)
// ===================================================
// Inicializar el periferico i2C
int i2cInit(void)
{
	xil_printf("I2C - START INICIALIZACION\n\r");
    int status;
    XIicPs_Config *config_ptr;

    config_ptr = XIicPs_LookupConfig(I2C_IIC_DEVICE_ID);
    if (config_ptr == NULL) {
        return XST_FAILURE;
    }

    status = XIicPs_CfgInitialize(&IicInstance, config_ptr, config_ptr->BaseAddress);
    if (status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    status = XIicPs_SelfTest(&IicInstance);
    if (status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    XIicPs_SetSClk(&IicInstance, I2C_IIC_SCLK_RATE);

    xil_printf("I2C - FIN INICIALIZACION\n\n\r");

    return XST_SUCCESS;
}

XIicPs* i2cGetInstance(void)
{
    return &IicInstance;
}

// ===================================================
// IMPLEMENTACIÓN DE FUNCIONES INTERNAS (STATICS)
// ===================================================
