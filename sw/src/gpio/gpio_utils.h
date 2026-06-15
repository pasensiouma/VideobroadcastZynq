/*
 * gpio_utils.h
 *
 *  Created on: 8 may. 2026
 *      Author: pablo
 */

#ifndef SRC_GPIO_UTILS_H_
#define SRC_GPIO_UTILS_H_

// ===================================================
// INCLUDES NECESARIOS PARA PROTOTIPOS / DECLARAICONES
// ===================================================
#include "xil_types.h"

// ============================================================
//  DEFINES PÚBLICOS
// ============================================================

// ============================================================
//  TIPOS Y ENUMS PÚBLICOS
// ============================================================

// ============================================================
//  PROTOTIPOS DE FUNCIONES PÚBLICAS
// ============================================================
void gpioInit();
void gpioCameraResetHardware();
u32 gpioReadButtonCameraReset();
void ov7670ResetPoll(void);
u32 gpioReadSwModoColor();

#endif /* SRC_GPIO_UTILS_H_ */
