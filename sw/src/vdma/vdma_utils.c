// ===================================================
// INCLUDES INTERNOS
// ===================================================
#include "xaxivdma.h"
#include "xil_types.h"
#include "xstatus.h"
#include "sleep.h"
#include "xil_cache.h"
#include "xscugic.h"
#include "xparameters.h"

#include "vdma_utils.h"
#include "xil_printf.h"
#include "xtime_l.h"

#include "../frame_params.h"

// ===================================================
// DEFINES INTERNOS
// ===================================================
// Periferico PS7_SCUGIC
#define INTC_BASE_ADDR      	XPAR_SCUGIC_0_CPU_BASEADDR
#define INTC_DIST_BASE_ADDR 	XPAR_SCUGIC_0_DIST_BASEADDR

// DEVICE ID
#define VDMA_COLOR_DEVICE_ID      XPAR_AXIVDMA_0_DEVICE_ID  // VDMA Original para HDMI
#define VDMA_GRAY_DEVICE_ID       XPAR_AXIVDMA_1_DEVICE_ID  // VDMA Nuevo para Blanco y Negro

// IDs de las líneas de interrupción físicas
#define VDMA_COLOR_INTR_ID       XPAR_FABRIC_AXI_VDMA_0_S2MM_INTROUT_INTR
#define VDMA_GRAY_INTR_ID        XPAR_FABRIC_AXI_VDMA_1_S2MM_INTROUT_INTR

// Reparto de la Memoria DDR RAM (Direcciones Base separadas)
#define DDR_BASE_ADDR              0x01000000
#define DDR_HIGH_ADDR              0x0F000000

// Parcela de Memoria 1: Buffers de Color (Original)
#define MEM_COLOR_BASE_ADDR        (DDR_BASE_ADDR + 0x01000000) // 0x02000000
#define COLOR_WRITE_ADDRESS_BASE   MEM_COLOR_BASE_ADDR
#define COLOR_READ_ADDRESS_BASE    MEM_COLOR_BASE_ADDR

// Parcela de Memoria 2: Buffers de Escala de Grises (Avanzamos 4 MB para no colisionar)
#define MEM_GRAY_BASE_ADDR         (MEM_COLOR_BASE_ADDR + 0x00400000) // 0x02400000
#define GRAY_WRITE_ADDRESS_BASE    MEM_GRAY_BASE_ADDR
#define GRAY_READ_ADDRESS_BASE    MEM_GRAY_BASE_ADDR

// Configuracion del VDMA
#define VDMA_CIRC                  1
#define VDMA_NOCIRC                0
#define VDMA_EXT_GENLOCK           0
#define VDMA_INT_GENLOCK           2
#define VDMA_S2MM_FSYNC            8

// Numero de frame buffers
#define VDMA_COLOR_FRAME_BUFFERS	4
#define VDMA_GRAY_FRAME_BUFFERS		3

// Tiempos
#define TIEMPO_ESPERA_WRITE_FRAME_COMPLETO_S 5
#define TIEMPO_ESPERA_LIMPIAR_ERRORES_S 1

// ===================================================
// CONSTANTES INTERNOS
// ===================================================

// ===================================================
// VARIABLES GLOBALES
// ===================================================
// Estáticas (solo visibles en este archivo)

// Controlador AXI VMDA
static XAxiVdma AxiVdmaColor; // Estructura de control VDMA Color
static XAxiVdma AxiVdmaGray;  // Estructura de control VDMA Gris

// Control de flags y punteros independientes
static int flagColorFrameReady = 0;
static int flagGrayFrameReady  = 0;

static u8 *freeColorFramePtr = NULL;
static u8 *freeGrayFramePtr  = NULL;

static u32 colorLastFrameWritten = -1;
static u32 grayLastFrameWritten = -1;

// Globales (visibles por otros módulos)

// ===================================================
// PROTOTIPO DE FUNCIONES INTERNAS (STATICS)
// ===================================================
static int vdmaInstanceInit(XAxiVdma *vdma_ptr, int device_id);
static int VDMA_ResetChannel(XAxiVdma *vdma_ptr, int direction);

static int VDMA_ConfigureChannel(XAxiVdma *vdma_ptr,
		int direction,
		int frame_stores,
		int frameHeightPixels,
		int frameStrideBytes,
		int frameSizeBytes,
		int addrMem);

static int VDMA_StartChannel(XAxiVdma *vdma_ptr, int direction);

static void vdmaChannelInit(
		XAxiVdma *vdma_ptr,
		int direction,
		int frame_stores,
		int frameHeightPixels,
		int frameStrideBytes,
		int frameSizeBytes,
		int addrMem);


static void vdmaClearChannelErrors(XAxiVdma *vdma, u16 Direction);
static void VdmaColorS2MMInterruptHandler(void *CallbackRef);
static void VdmaGrayS2MMInterruptHandler(void *CallbackRef);
static int SetupVdmaInterrupt(void);

// ===================================================
// IMPLEMENTACIÓN DE FUNCIONES PÚBLICAS (.h)
// ===================================================
void vdmaInit(void)
{
	xil_printf("VDMA - START INICIALIZACION\n\r");

	// Inicializar instancia VDMA
	vdmaInstanceInit(&AxiVdmaColor, VDMA_COLOR_DEVICE_ID);
	vdmaInstanceInit(&AxiVdmaGray, VDMA_GRAY_DEVICE_ID);

	// CANAL WRITE
	vdmaChannelInit(
			&AxiVdmaColor,
			XAXIVDMA_WRITE,
			VDMA_COLOR_FRAME_BUFFERS,
			VIDEO_FRAME_HEIGHT_PIXELS,
			VIDEO_STRIDE_BYTES,
			VIDEO_FRAME_SIZE_BYTES,
			COLOR_WRITE_ADDRESS_BASE);

	vdmaChannelInit(
			&AxiVdmaGray,
			XAXIVDMA_WRITE,
			VDMA_GRAY_FRAME_BUFFERS,
			VIDEO_GRAY_FRAME_HEIGHT_PIXELS,
			VIDEO_GRAY_STRIDE_BYTES,
			VIDEO_GRAY_FRAME_SIZE_BYTES,
			GRAY_WRITE_ADDRESS_BASE);

	sleep(TIEMPO_ESPERA_WRITE_FRAME_COMPLETO_S);

	vdmaClearErrors();

	// CANAL READ
	vdmaChannelInit(
			&AxiVdmaColor,
			XAXIVDMA_READ,
			VDMA_COLOR_FRAME_BUFFERS,
			VIDEO_FRAME_HEIGHT_PIXELS,
			VIDEO_STRIDE_BYTES,
			VIDEO_FRAME_SIZE_BYTES,
			COLOR_READ_ADDRESS_BASE);

	// Limpiar errores
	vdmaClearErrors();

    // Habilitar la interrupción S2MM (fin de frame escrito)
    XAxiVdma_IntrEnable(&AxiVdmaColor, XAXIVDMA_IXR_ALL_MASK, XAXIVDMA_WRITE);
    XAxiVdma_IntrEnable(&AxiVdmaGray, XAXIVDMA_IXR_ALL_MASK, XAXIVDMA_WRITE);

    // Registrar la ISR en el GIC
    SetupVdmaInterrupt();

    xil_printf("VDMA - FIN INICIALIZACION\n\n\r");
}

void vdmaClearErrors()
{
	vdmaClearChannelErrors(&AxiVdmaColor, XAXIVDMA_WRITE);
	vdmaClearChannelErrors(&AxiVdmaColor, XAXIVDMA_READ);
	vdmaClearChannelErrors(&AxiVdmaGray, XAXIVDMA_WRITE);
}

u8* vdmaGetFreeFramePtr(int n)
{
	u8* ptr = NULL;

	switch(n)
	{
	case 0:
		ptr = freeColorFramePtr;
		break;
	case 1:
		ptr = freeGrayFramePtr;
		break;
	default:
		ptr = NULL;
		break;
	}
	return ptr;
}

int vdmaGetFlagNewFrameReady(int n)
{
	int isFlagActive = 0;

	switch(n)
	{
	case 0:
		// ¿Flag activado?
		if (flagColorFrameReady)
		{
			// Si: consumir flag
			flagColorFrameReady = 0;
			isFlagActive = 1;
		}
		break;
	case 1:
		// ¿Flag activado?
		if (flagGrayFrameReady)
		{
			// Si: consumir flag
			flagGrayFrameReady = 0;
			isFlagActive = 1;
		}
		break;
	default:
		isFlagActive = 0;
		break;
	}

	return isFlagActive;
}


// ===================================================
// IMPLEMENTACIÓN DE FUNCIONES INTERNAS (STATICS)
// ===================================================
// Inicializa la instancia del VDMA
static int vdmaInstanceInit(XAxiVdma *vdma_ptr, int device_id)
{
    XAxiVdma_Config *cfg_ptr;
    int status;

    cfg_ptr = XAxiVdma_LookupConfig(device_id);
    if (cfg_ptr == NULL) {
        xil_printf("ERROR: No se encontro el VDMA para el ID %d\r\n", device_id);
        return XST_FAILURE;
    }

    status = XAxiVdma_CfgInitialize(vdma_ptr, cfg_ptr, cfg_ptr->BaseAddress);
    if (status != XST_SUCCESS) {
        xil_printf("ERROR: Fallo la inicializacion del VDMA (%d)\r\n", status);
        return XST_FAILURE;
    }

    return XST_SUCCESS;
}

// Reset de un canal del VDMA (read o write)
static int VDMA_ResetChannel(XAxiVdma *vdma_ptr, int direction)
{
    int polls = 100000;

    xil_printf("Resetting %s VDMA channel ...\r\n",
               (direction == XAXIVDMA_READ) ? "Read" : "Write");

    XAxiVdma_Reset(vdma_ptr, direction);

    // esperar a que el reset termine
    while (polls && XAxiVdma_ResetNotDone(vdma_ptr, direction)) {
        --polls;
    }

    if (!polls) {
        xil_printf("ERROR: VDMA %s channel reset failed\r\n",
                   (direction == XAXIVDMA_READ) ? "Read" : "Write");
        return XST_FAILURE;
    }

    return XST_SUCCESS;
}

// Configura un canal (read o write) para transferenccia de video
//	- Triple buffer: frame_stores = 3
//	- Write: camara -> DDR
//	- Read: DDR -> HDMI
static int VDMA_ConfigureChannel(XAxiVdma *vdma_ptr,
		int direction,
		int frame_stores,
		int frameHeightPixels,
		int frameStrideBytes,
		int frameSizeBytes,
		int addrMem)
{
    XAxiVdma_DmaSetup dma_setup;
    int status;
    int i;
    int addr;

    xil_printf("Setting up VDMA %s channel ...\r\n",
               (direction == XAXIVDMA_READ) ? "Read" : "Write");

    // Configuracion
    dma_setup.VertSizeInput = frameHeightPixels;		// Configuración básica de vídeo
    dma_setup.HoriSizeInput = frameStrideBytes;				// Configuración básica de vídeo
    dma_setup.Stride = frameStrideBytes;						// Configuración básica de vídeo
    dma_setup.FrameDelay = 0;									// Sin delay entre frames
    dma_setup.EnableCircularBuf = 1;							// Modo circular -> streaming continuo
    dma_setup.EnableSync = 1;									// Sincronización con señales AXI-Stream (tuser/tlast)
    dma_setup.PointNum = 0;
    dma_setup.EnableFrameCounter = 0; 							// Transferencia continua (sin contador de frames)
    dma_setup.FixedFrameStoreAddr = 0; 							// No se usa modo parking

    // Configurar número de buffers (triple buffer)
    if (vdma_ptr->MaxNumFrames > frame_stores) {
        xil_printf("Setting frame store for %s channel ...\r\n",
                   (direction == XAXIVDMA_READ) ? "Read" : "Write");
        status = XAxiVdma_SetFrmStore(vdma_ptr, frame_stores, direction);
        if (status != XST_SUCCESS) {
            xil_printf("WARNING: no se pudo fijar el numero de frame stores (%d) para %s\r\n",
                       frame_stores,
                       (direction == XAXIVDMA_READ) ? "Read" : "Write");
            return XST_FAILURE;
        }
    }

    // Aplicar configuracion
    status = XAxiVdma_DmaConfig(vdma_ptr, direction, &dma_setup);
    if (status != XST_SUCCESS) {
        xil_printf("ERROR: VDMA %s channel config failed (%d)\r\n",
                   (direction == XAXIVDMA_READ) ? "Read" : "Write",
                   status);
        return XST_FAILURE;
    }

    //=======================================================================
    // ASIGNACION DE DIRECCIONES DE MEMORIA (DDR) PARA LOS BUFFERS
    //=======================================================================
    // Se crean 3 buffers consecutivos en memoria
    addr = addrMem;

    for (i = 0; i < frame_stores; ++i) {
        xil_printf("vdma_setup: Address %d = 0x%08x\r\n", i, addr);
        dma_setup.FrameStoreStartAddr[i] = addr;
        addr += frameSizeBytes;
    }

    // Registrar direcciones en el hardware
    status = XAxiVdma_DmaSetBufferAddr(vdma_ptr, direction, dma_setup.FrameStoreStartAddr);
    if (status != XST_SUCCESS) {
        xil_printf("ERROR: VDMA %s channel set buffer address failed (%d)\r\n",
                   (direction == XAXIVDMA_READ) ? "Read" : "Write",
                   status);
        return XST_FAILURE;
    }

    return XST_SUCCESS;
}

// Iniciar la transferencia VDMA en el canal indicado
static int VDMA_StartChannel(XAxiVdma *vdma_ptr, int direction)
{
    int status;

    status = XAxiVdma_DmaStart(vdma_ptr, direction);
    if (status != XST_SUCCESS) {
        xil_printf("ERROR: VDMA start %s transfer failed\r\n",
                   (direction == XAXIVDMA_READ) ? "Read" : "Write");
        return XST_FAILURE;
    }

    xil_printf("VDMA - %s channel starting ...\r\n",
               (direction == XAXIVDMA_READ) ? "Read" : "Write");
    return XST_SUCCESS;
}

static void vdmaChannelInit(
		XAxiVdma *vdma_ptr,
		int direction,
		int frame_stores,
		int frameHeightPixels,
		int frameStrideBytes,
		int frameSizeBytes,
		int addrMem)
{
	VDMA_ResetChannel(vdma_ptr, direction);
	VDMA_ConfigureChannel(vdma_ptr, direction, frame_stores, frameHeightPixels, frameStrideBytes, frameSizeBytes, addrMem);
	VDMA_StartChannel(vdma_ptr, direction);
}

static void vdmaClearChannelErrors(XAxiVdma *vdma, u16 Direction)
{
    u32 errors = XAxiVdma_GetDmaChannelErrors(vdma, Direction);

    if (errors)
    {
    	// Indicar error detectado
        xil_printf("VDMA ERROR (dir=%d): 0x%08x\n", Direction, errors);

        //Limpiar errores
        xil_printf("Limpiando error automaticamente\n\r");
        XAxiVdma_ClearDmaChannelErrors(vdma, Direction, errors);
        sleep(TIEMPO_ESPERA_LIMPIAR_ERRORES_S);
    }
}

// ISR del canal WRITE (S2MM) del VDMA:
// Se dispara cada vez que el VDMA completa la escritura de un frame en DDR
static void VdmaColorS2MMInterruptHandler(void *CallbackRef)
{
    XAxiVdma *vdma = (XAxiVdma *)CallbackRef;

    // Variables internas
    int writeFrameIdx;
    int freeFrameIdx;

    // limpiar el flag de interrupción en el VDMA
    XAxiVdma_IntrClear(vdma, XAXIVDMA_IXR_ALL_MASK, XAXIVDMA_WRITE);

    // leer qué frame store está activo en cada canal
    writeFrameIdx = XAxiVdma_CurrFrameStore(vdma, XAXIVDMA_WRITE);

    // Comprobar si ya había un frame escrito
    if (colorLastFrameWritten == -1)
    {
    	// Actualizar para la proxima interrupcion
    	colorLastFrameWritten = writeFrameIdx;
    	return;
    }

    // Determinar el índice del buffer libre (el ultimo buffer escrito)
    freeFrameIdx = colorLastFrameWritten;

    // Actualizar para la proxima interrupcion
    colorLastFrameWritten = writeFrameIdx;

    // calcular la dirección DDR del buffer libre
    freeColorFramePtr = (u8 *)(COLOR_WRITE_ADDRESS_BASE + freeFrameIdx * VIDEO_FRAME_SIZE_BYTES);

    // 5) Invalidar la caché de datos
    Xil_DCacheInvalidateRange((UINTPTR)freeColorFramePtr, VIDEO_FRAME_SIZE_BYTES);

    // Indicar que hay un nuevo frame disponible
    flagColorFrameReady = 1;

    // debug
    //xil_printf("vdma color interrupcion W idx = %d\n\r", writeFrameIdx);
    //xil_printf("vdma color interrupcion F idx = %d\n\r", freeFrameIdx);
    //xil_printf("vdma color interrupcion R idx = %d\n\r", XAxiVdma_CurrFrameStore(vdma, XAXIVDMA_READ));

    // Contador de frames (debug)

    static int frameCount = 0;
    static XTime lastTime = 0;
    XTime now;
    XTime_GetTime(&now);
    frameCount++;
    // Imprimir cada segundo (COUNTS_PER_SECOND = frecuencia del timer)
    if ((now - lastTime) >= COUNTS_PER_SECOND)
    {
        xil_printf("FPS camara (ISR): %d\r\n", frameCount);
        frameCount = 0;
        lastTime = now;
    }

}

static void VdmaGrayS2MMInterruptHandler(void *CallbackRef)
{
    XAxiVdma *vdma = (XAxiVdma *)CallbackRef;

    // Variables internas
    int writeFrameIdx;
    int freeFrameIdx;

    // limpiar el flag de interrupción en el VDMA
    XAxiVdma_IntrClear(vdma, XAXIVDMA_IXR_ALL_MASK, XAXIVDMA_WRITE);

    // leer qué frame store está activo en cada canal
    writeFrameIdx = XAxiVdma_CurrFrameStore(vdma, XAXIVDMA_WRITE);

    // Comprobar si ya había un frame escrito
    if (grayLastFrameWritten == -1)
    {
    	// Actualizar para la proxima interrupcion
    	grayLastFrameWritten = writeFrameIdx;
    	return;
    }

    // Determinar el índice del buffer libre (el ultimo buffer escrito)
    freeFrameIdx = grayLastFrameWritten;

    // Actualizar para la proxima interrupcion
    grayLastFrameWritten = writeFrameIdx;

    // calcular la dirección DDR del buffer libre
    freeGrayFramePtr = (u8 *)(GRAY_WRITE_ADDRESS_BASE + freeFrameIdx * VIDEO_GRAY_FRAME_SIZE_BYTES);

    // 5) Invalidar la caché de datos
    Xil_DCacheInvalidateRange((UINTPTR)freeGrayFramePtr, VIDEO_GRAY_FRAME_SIZE_BYTES);

    // Indicar que hay un nuevo frame disponible
    flagGrayFrameReady = 1;

    // debug
    //xil_printf("vdma gray interrupcion idx = %d\n\r", freeFrameIdx);
}

// Registro de la ISR en el GIC (Generic Interrupt Controller)
static int SetupVdmaInterrupt(void)
{
	// Registrar el manejador para la interrupción S2MM del VDMA
    XScuGic_RegisterHandler(
        INTC_BASE_ADDR,
		VDMA_COLOR_INTR_ID,
        (Xil_ExceptionHandler)VdmaColorS2MMInterruptHandler,
        (void *)&AxiVdmaColor
    );

    XScuGic_RegisterHandler(
        INTC_BASE_ADDR,
		VDMA_GRAY_INTR_ID,
        (Xil_ExceptionHandler)VdmaGrayS2MMInterruptHandler,
        (void *)&AxiVdmaGray
    );

    // Habilitar la interrupción en el distribuidor del GIC
    XScuGic_EnableIntr(INTC_DIST_BASE_ADDR, VDMA_COLOR_INTR_ID);
    XScuGic_EnableIntr(INTC_DIST_BASE_ADDR, VDMA_GRAY_INTR_ID);



    return XST_SUCCESS;
}
