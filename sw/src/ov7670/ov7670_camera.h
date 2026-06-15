#ifndef OV7670_CAMERA_H
#define OV7670_CAMERA_H

// ===================================================
// INCLUDES NECESARIOS PARA PROTOTIPOS / DECLARAICONES
// ===================================================

// ============================================================
//  DEFINES PÚBLICOS
// ============================================================

// ============================================================
//  TIPOS Y ENUMS PÚBLICOS
// ============================================================

// ============================================================
//  PROTOTIPOS DE FUNCIONES PÚBLICAS
// ============================================================

// Funciones publicas del driver ov7670.c
int ov7670init(void);
void OV7670_SendDefaultConfiguration(void);
void OV7670_ResetSoftware(void);
void ov7670ResetCompleto(void);
void ov7670ResetPoll(void);

#endif /* OV7670_CAMERA_H */
