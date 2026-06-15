/*
 * tcpServer.h
 *
 *  Created on: 9 may. 2026
 *      Author: pablo
 */

#ifndef SRC_TCPSERVER_TCPSERVER_H_
#define SRC_TCPSERVER_TCPSERVER_H_

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
int tcpServerInit(void);
int tcpTransferData(int vdmaFlagNewFrameReady, u8 *vdmaFreeFramePtr, int tcpFrameSize, int frameW, int frameH, int frameBpp);
int tcpServerIsBusy();

#endif /* SRC_TCPSERVER_TCPSERVER_H_ */
