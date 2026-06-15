#ifndef VDMA_UTILS_H
#define VDMA_UTILS_H

// ===================================================
// INCLUDES NECESARIOS PARA PROTOTIPOS / DECLARAICONES
// ===================================================
#include "xaxivdma.h"

// ============================================================
//  DEFINES PÚBLICOS
// ============================================================

// ============================================================
//  TIPOS Y ENUMS PÚBLICOS
// ============================================================

// ============================================================
//  PROTOTIPOS DE FUNCIONES PÚBLICAS
// ============================================================
void vdmaInit(void);
void vdmaClearErrors();
u8* vdmaGetFreeFramePtr(int n);
int vdmaGetFlagNewFrameReady(int n);


#endif /* VDMA_UTILS_H */
