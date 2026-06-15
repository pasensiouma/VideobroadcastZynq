/*
 * rgb2gray.c
 *
 *  Created on: 2 jun. 2026
 *      Author: pablo
 */


// ===================================================
// INCLUDES INTERNOS
// ===================================================
#include "xrgb2gray.h"
#include "xparameters.h"
#include "rgb2gray.h"

// ===================================================
// DEFINES INTERNOS
// ===================================================
#define RGB2GRAY_ID			XPAR_RGB2GRAY_0_DEVICE_ID

// ===================================================
// CONSTANTES INTERNOS
// ===================================================

// ===================================================
// VARIABLES GLOBALES
// ===================================================
static XRgb2gray rgb2gray;

// ===================================================
// PROTOTIPO DE FUNCIONES INTERNAS (STATICS)
// ===================================================

// ===================================================
// IMPLEMENTACION DE FUNCIONES PUBLICAS (.h)
// ===================================================
void rgb2grayInit()
{
	xil_printf("IMAGE_FILTER - START INICIALIZACION\n\r");

    XRgb2gray_Config *cfg;

    cfg = XRgb2gray_LookupConfig(RGB2GRAY_ID);
    XRgb2gray_CfgInitialize(&rgb2gray, cfg);

    XRgb2gray_EnableAutoRestart(&rgb2gray);
    XRgb2gray_Start(&rgb2gray);

    xil_printf("IMAGE FILTER - FIN INICIALIZACION\n\n\r");
}

// ===================================================
// IMPLEMENTACION DE FUNCIONES INTERNAS (STATICS)
// ===================================================

