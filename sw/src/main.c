//=======================================================================
// INCLUDES
//=======================================================================

#include "sleep.h"

#include "gpio/gpio_utils.h"
#include "i2c/i2c.h"
#include "network/network.h"
#include "ov7670/ov7670_camera.h"
#include "platform/platform_config.h"
#include "platform/platform.h"
#include "tcpServer/tcpServer.h"
#include "uart/uart.h"
#include "vdma/vdma_utils.h"
#include "rgb2gray/rgb2gray.h"

#include "frame_params.h"

//=======================================================================
// DEFINES
//=======================================================================

//=======================================================================
// PROTOTIPOS DE FUNCIONES
//=======================================================================

//=======================================================================
// VARIABLES GLOBALES
//=======================================================================

//=======================================================================
// MAIN
//=======================================================================
int main(void)
{
	// Inicializaciones
	uartInit();				// Inicializar uart
	networkInit();			// Inicializar red
	tcpServerInit();		// Inicializar servidor TCP
	gpioInit();				// Inicializar gpio
	i2cInit();				// Inicializar i2c
	ov7670init();			// Inicializar ov7670
	rgb2grayInit();			// Inicializar rgb2gray
	vdmaInit();				// Inicializar vdma

	// Variables internas
	int frameW = VIDEO_FRAME_WIDTH_PIXELS;
	int frameH = VIDEO_FRAME_HEIGHT_PIXELS;
	int frameBpp = VIDEO_BYTES_PER_PIXEL;
	int frameSize = VIDEO_FRAME_SIZE_BYTES;
	int modoColor = 0;

    // BUCLE INFINITO
    while (1) {
    	// Procesar stack lwIP
    	networkProcesarStackLWIP();

    	// Seleccionar el modo de transmision si el servidor no esta ocupado
    	if (!tcpServerIsBusy())
    	{
    		modoColor = (int)gpioReadSwModoColor();
    		switch(modoColor)
    		{
    		case 0:
    			frameW = VIDEO_FRAME_WIDTH_PIXELS;
    			frameH = VIDEO_FRAME_HEIGHT_PIXELS;
    			frameBpp = VIDEO_BYTES_PER_PIXEL;
    			frameSize = VIDEO_FRAME_SIZE_BYTES;
    			break;
    		case 1:
    			frameW = VIDEO_GRAY_FRAME_WIDTH_PIXELS;
    			frameH = VIDEO_GRAY_FRAME_HEIGHT_PIXELS;
    			frameBpp = VIDEO_GRAY_BYTES_PER_PIXEL;
    			frameSize = VIDEO_GRAY_FRAME_SIZE_BYTES;
    			break;
    		default:
    			frameW = VIDEO_FRAME_WIDTH_PIXELS;
    			frameH = VIDEO_FRAME_HEIGHT_PIXELS;
    			frameBpp = VIDEO_BYTES_PER_PIXEL;
    			frameSize = VIDEO_FRAME_SIZE_BYTES;
    			break;
    		}
    	}

        // Intentar enviar frame por TCP (si hay frame listo)
    	tcpTransferData(	vdmaGetFlagNewFrameReady(modoColor),
    						vdmaGetFreeFramePtr(modoColor),
							frameSize,
							frameW,
							frameH,
							frameBpp);

        // Reset camara mediante poll de boton
    	ov7670ResetPoll();

        // Leer y limpiar errores del vdma automaticamente
    	vdmaClearErrors();
    }

    return XST_SUCCESS;
}
