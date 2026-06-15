// ===================================================
// INCLUDES INTERNOS
// ===================================================
#include "xgpio.h"
#include "xparameters.h"
#include "xil_printf.h"

#include "sleep.h"

#include "gpio_utils.h"

// ===================================================
// DEFINES INTERNOS
// ===================================================

// DEVICES ID
#define RESET_BUTTON_DEVICE_ID   XPAR_RESETCAMBUTTON_DEVICE_ID		// GPIO: botón de reset (input)
#define CAMERA_RESET_DEVICE_ID   XPAR_RESETCAM_DEVICE_ID			// GPIO: Señal de reset de la cámara (output)
#define SW_COLOR_MODE_ID		 XPAR_SW_COLOR_MODE_DEVICE_ID		// GPIO: SW para elegir el modo de transmisión (color / gris)

// CHANNELS
#define GPIO_CHANNEL_1 		1
#define GPIO_CHANNEL_2 		2

// DIRECTION MASKS
#define GPIO_DIRECTION_MASK_INPUT	0xFFFFFFFF
#define GPIO_DIRECTION_MASK_OUTPUT	0x00000000

// WRITE VALUES
#define CAMERA_RESET_SIGNAL_ON 	1
#define CAMERA_RESET_SIGNAL_OFF	0

// TIEMPOS
#define TIEMPO_ESPERA_RESET_S 1

// ===================================================
// CONSTANTES INTERNAS
// ===================================================

// ===================================================
// VARIABLES GLOBALES
// ===================================================
// Estáticas (solo visibles en este archivo)
static XGpio gpioButton3;			// Botón físico para reset (input)
static XGpio gpioCameraReset;		// Señal de reset para la cámara (output)
static XGpio gpioSwModoColor;		// Switch para seleccionar el modo de transmision (input)

static u32 swModoColorLastState = 0;

// Globales (visibles por otros módulos)

// ===================================================
// PROTOTIPO DE FUNCIONES INTERNAS (STATICS)
// ===================================================

// ===================================================
// IMPLEMENTACIÓN DE FUNCIONES INTERNAS (STATICS)
// ===================================================

// ===================================================
// IMPLEMENTACIÓN DE FUNCIONES PÚBLICAS (.h)
// ===================================================

// Inicializar los GPIOS
void gpioInit()
{
	xil_printf("GPIO - START INICIALIZACION\n\r");

	// Inicializar boton reset software (input)
    XGpio_Initialize(&gpioButton3, RESET_BUTTON_DEVICE_ID);
    XGpio_SetDataDirection(&gpioButton3, GPIO_CHANNEL_1, GPIO_DIRECTION_MASK_INPUT);

    // Inicializar senal reset camara hardware (output)
    XGpio_Initialize(&gpioCameraReset, CAMERA_RESET_DEVICE_ID);
    XGpio_SetDataDirection(&gpioCameraReset, GPIO_CHANNEL_1, GPIO_DIRECTION_MASK_OUTPUT);

    // Inicializar switch habilitar filtro de imagen (input)
    XGpio_Initialize(&gpioSwModoColor, SW_COLOR_MODE_ID);
    XGpio_SetDataDirection(&gpioSwModoColor, GPIO_CHANNEL_1, GPIO_DIRECTION_MASK_INPUT);

    xil_printf("GPIO - FIN INICIALIZACION\n\n\r");
}

// Reset físico de la cámara vía GPIO
void gpioCameraResetHardware()
{
	xil_printf("GPIO - CameraResetHardware - SIGNAL OFF\n\r");
    XGpio_DiscreteWrite(&gpioCameraReset, GPIO_CHANNEL_1, CAMERA_RESET_SIGNAL_OFF);
    sleep(TIEMPO_ESPERA_RESET_S);
    XGpio_DiscreteWrite(&gpioCameraReset, GPIO_CHANNEL_1, CAMERA_RESET_SIGNAL_ON);
    xil_printf("GPIO - CameraResetHardware - SIGNAL ON\n\r");
}

// Leer botón reset
u32 gpioReadButtonCameraReset()
{
	return XGpio_DiscreteRead(&gpioButton3, GPIO_CHANNEL_1);
}


// Leer switch modo color
u32 gpioReadSwModoColor()
{
	u32 swState = XGpio_DiscreteRead(&gpioSwModoColor, GPIO_CHANNEL_1);

	if (!(swModoColorLastState == swState))
	{
		xil_printf("GPIO - SW modo color n = %d\n\r", swState);
		swModoColorLastState = swState;

		usleep(20000);
	}

	return swState;
}
