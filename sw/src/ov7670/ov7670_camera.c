// ===================================================
// INCLUDES INTERNOS
// ===================================================
#include "ov7670_camera.h"
#include "../gpio/gpio_utils.h"

#include <stdio.h>
#include "sleep.h"
#include "xparameters.h"
#include "xil_types.h"
#include "xstatus.h"
#include "xiicps.h"

#include "xil_printf.h"
#include "../i2c/i2c.h"

// ===================================================
// DEFINES INTERNOS
// ===================================================
// Dirección I2C de la ov7670
#define OV7670_IIC_SLAVE_ADDR  0x21

// TIEMPOS
#define TIEMPO_RETARDO_100_US 100
#define TIEMPO_RETARDO_1000_US 1000
#define TIEMPO_RETARDO_2500_US 2500
#define TIEMPO_RETARDO_1_S 1

// ===================================================
// TIPO Y ENUMS INTERNOS
// ===================================================
// Estructura simple para representar un par "registro/valor" para definir la tabla de configuración
typedef struct {
    u8 reg;
    u8 value;
} OV7670_RegValue;

// ===================================================
// CONSTANTES INTERNOS
// ===================================================

// TABLA DE CONFIGURACION OV7670
/*
 * Cada entrada representa un par "registro->valor".
 * El orden es importante: algunos registros dependen de otros.
 * Esta configuracion produce:
 * - Formato RGB565
 * - Salida progresiva
 * - Timings compatibles con HDMI
 */

static const OV7670_RegValue Ov7670DefaultConfig[] = {
    {0x12, 0x04}, /* COM7   Selección de formato (RGB) */
    {0x0C, 0x00}, /* COM3   Sin escalado */
    {0x3E, 0x00}, /* COM14  Sin división de reloj */

    {0x8C, 0x00}, /* RGB444 Desactivado */
    {0x40, 0x10}, /* COM15  RGB565, rango completo */
    {0x3A, 0x04}, /* TSLB   Orden UV */
    {0x14, 0x38}, /* COM9   Ganancia automática */
    {0x3D, 0xC0}, /* COM13  Corrección gamma */

    {0x17, 0x11}, /* HSTART */
    {0x18, 0x61}, /* HSTOP  */
    {0x32, 0xA4}, /* HREF   */

    {0x19, 0x03}, /* VSTART */
    {0x1A, 0x7B}, /* VSTOP  */
    {0x03, 0x0A}, /* VREF   */

    {0x0E, 0x61}, /* COM5 */
    {0x0F, 0x4B}, /* COM6 */

    {0x16, 0x02},

    {0x21, 0x02},
    {0x22, 0x91},

    {0x29, 0x07},
    {0x33, 0x0B},

    {0x35, 0x0B},
    {0x37, 0x1D},

    {0x38, 0x71},
    {0x39, 0x2A},

    {0x3C, 0x78}, /* COM12 */
    {0x4D, 0x40},

    {0x4E, 0x20},
    {0x69, 0x00}, /* GFIX */

    {0x6B, 0x4A}, // valor original 0x4A
    {0x74, 0x10},

    {0x8D, 0x4F},
    {0x8E, 0x00},

    {0x8F, 0x00},
    {0x90, 0x00},

    {0x91, 0x00},
    {0x96, 0x00},

    {0x9A, 0x00},
    {0xB0, 0x84},

    {0xB1, 0x0C},
    {0xB2, 0x0E},

    {0xB3, 0x82},
    {0xB8, 0x0A},

    {0x11, 0x01}  /* CLKRC  División de reloj de entrada --- Valor original 0x00 */
};

// ===================================================
// VARIABLES GLOBALES
// ===================================================
// Estáticas (solo visibles en este archivo)
static XIicPs *IicInstancePtr = NULL;

// Globales (visibles por otros módulos)

// ===================================================
// PROTOTIPO DE FUNCIONES INTERNAS (STATICS)
// ===================================================
static int OV7670_WriteRegister(u8 reg, u8 value);
static int OV7670_ReadRegister(u8 reg, u8 *value);

// ===================================================
// IMPLEMENTACIÓN DE FUNCIONES PÚBLICAS (.h)
// ===================================================

// CONFIGURACION COMPLETA DE LA CAMARA: Aplica la tabla de configuracion
// Secuencia para cada registro:
// 	- 1) Escribir
// 	- 2) Esperar
//	- 3) leer para verificar
void OV7670_SendDefaultConfiguration(void)
{
    unsigned int i;
    const unsigned int count = (unsigned int)(sizeof(Ov7670DefaultConfig) / sizeof(Ov7670DefaultConfig[0]));

    xil_printf("OV7670 - Enviando configuración por defecto por i2c\r");

    // Recorrer toda la tabla de registros
    for (i = 0; i < count; ++i)
    {
        const u8 reg = Ov7670DefaultConfig[i].reg;
        const u8 value = Ov7670DefaultConfig[i].value;
        u8 readback = 0x00;

        // Escribir registro
        OV7670_WriteRegister(reg, value);

        // Esperar
        usleep(TIEMPO_RETARDO_100_US); //Aproximadamente 100 us

        // Leer para confirmar la escritura
        if (OV7670_ReadRegister(reg, &readback) != XST_SUCCESS || readback != value) {
            xil_printf("ATENCION. El registro %02x NO se ha configurado correctamente (esperado %02x, leido %02x)\r\n",
                       reg, value, readback);
        }

        usleep(TIEMPO_RETARDO_1000_US);
    }

    xil_printf("OV7670 - Configuración por defecto enviada \n\r");
}

// Reset software mediante com7
void OV7670_ResetSoftware(void)
{
	xil_printf("OV7670 - Enviando reset software por i2c\r");
    OV7670_WriteRegister(0x12, 0x80);
    sleep(TIEMPO_RETARDO_1_S);
    xil_printf("OV7670 - Reset software enviado por i2c\n\r");
}

// Reset software y hardware
void ov7670ResetCompleto(void)
{
	gpioCameraResetHardware();			// Reset hardware
	OV7670_ResetSoftware();				// Reset software
	OV7670_SendDefaultConfiguration();	// Enviar configuracion por defecto
}

// Inicializar camara.
int ov7670init(void)
{
	xil_printf("OV7670 - START INICIALIZACION\n\r");

	// Obtner instancia i2c
	IicInstancePtr = i2cGetInstance();

    // Reset sensor completo
	ov7670ResetCompleto();

	xil_printf("OV7670 - FIN INICIALIZACION\n\n\r");

    return XST_SUCCESS;
}

void ov7670ResetPoll(void)
{
	if (gpioReadButtonCameraReset())
	{
		xil_printf("OV7670 - Reset camara completo solicitado\r\n");
		ov7670ResetCompleto();
		xil_printf("OV7670 - Reset camara completo finalizado\r\n");
	}
}


// ===================================================
// IMPLEMENTACIÓN DE FUNCIONES INTERNAS (STATICS)
// ===================================================

// Escritura de registros: Escribe un registro de la cámara mediante I2C
// Protocolo: [SLAVE_ADDR] [REG] [VALUE]
static int OV7670_WriteRegister(u8 reg, u8 value)
{
    int status;
    u8 send_buffer[2];

    send_buffer[0] = reg;
    send_buffer[1] = value;

    xil_printf("Enviando registro: address=%02x val=%02x\r\n", reg, value);

    status = XIicPs_MasterSendPolled(IicInstancePtr, send_buffer, 2, OV7670_IIC_SLAVE_ADDR);
    if (status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    while (XIicPs_BusIsBusy(IicInstancePtr)) {
        // Espera a que el bus este libre
    }

    return XST_SUCCESS;
}

//LECTURA DE REGISTRO: Lee un registro de la camara mediante I2C.
// Secuencia:
//	- 1) enviar direccion de registro
// 	- 2) Leer valor
static int OV7670_ReadRegister(u8 reg, u8 *value)
{
    int status;
    u8 addr = reg;
    u8 val = 0x00;

    // Enviar dirección del registro
    status = XIicPs_MasterSendPolled(IicInstancePtr, &addr, 1, OV7670_IIC_SLAVE_ADDR);
    if (status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    // Leer valor
    status = XIicPs_MasterRecvPolled(IicInstancePtr, &val, 1, OV7670_IIC_SLAVE_ADDR);
    if (status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    while (XIicPs_BusIsBusy(IicInstancePtr)) {
        //Espera a que el bus quede libre
    }

    if (value != NULL) {
        *value = val;
    }

    xil_printf("Recibido registro: address=%02x val=%02x\r\n", reg, val);
    usleep(TIEMPO_RETARDO_2500_US);

    return XST_SUCCESS;
}


