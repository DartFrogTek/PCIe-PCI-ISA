#include "it8888.h"

#define GET_IN(type, name) type *name=NULL; size_t inLen=0; status=WdfRequestRetrieveInputBuffer(Request, sizeof(type), (PVOID*)&name, &inLen); if(!NT_SUCCESS(status)) break
#define GET_OUT(type, name) type *name=NULL; size_t outLen=0; status=WdfRequestRetrieveOutputBuffer(Request, sizeof(type), (PVOID*)&name, &outLen); if(!NT_SUCCESS(status)) break
#define COMPLETE(sz) do { info=(sz); } while(0)

VOID It8888EvtIoDeviceControl(WDFQUEUE Queue, WDFREQUEST Request, size_t OutputBufferLength, size_t InputBufferLength, ULONG IoControlCode)
{
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);
    PDEVICE_CONTEXT ctx = DeviceGetContext(WdfIoQueueGetDevice(Queue));
    NTSTATUS status = STATUS_SUCCESS;
    size_t info = 0;

    do {
        switch (IoControlCode) {
        case IOCTL_IT8888_GET_INFO: { GET_OUT(IT8888_INFO, o); status=It8888GetInfo(ctx,o); if(NT_SUCCESS(status)) COMPLETE(sizeof(*o)); break; }
        case IOCTL_IT8888_CFG_READ: { GET_IN(IT8888_CFG_ACCESS, in); GET_OUT(IT8888_CFG_ACCESS, out); *out=*in; status=It8888PciRead(ctx,in->Offset,in->Width,&out->Value); if(NT_SUCCESS(status)) COMPLETE(sizeof(*out)); break; }
        case IOCTL_IT8888_CFG_WRITE: { GET_IN(IT8888_CFG_ACCESS, in); status=It8888PciWrite(ctx,in->Offset,in->Width,in->Value); break; }
        case IOCTL_IT8888_PORT_READ: { GET_IN(IT8888_PORT_ACCESS, in); GET_OUT(IT8888_PORT_ACCESS, out); *out=*in; status=It8888PortRead(ctx,in->Port,in->Width,&out->Value); if(NT_SUCCESS(status)) COMPLETE(sizeof(*out)); break; }
        case IOCTL_IT8888_PORT_WRITE: { GET_IN(IT8888_PORT_ACCESS, in); status=It8888PortWrite(ctx,in->Port,in->Width,in->Value); break; }
        case IOCTL_IT8888_INIT_DEFAULT: { status=It8888ApplyDefaultInit(ctx); break; }
        case IOCTL_IT8888_DMA_ALLOC: { GET_IN(IT8888_DMA_ALLOC_REQUEST, in); GET_OUT(IT8888_DMA_INFO, out); status=It8888DmaAllocate(ctx,in->Size,out); if(NT_SUCCESS(status)) COMPLETE(sizeof(*out)); break; }
        case IOCTL_IT8888_DMA_FREE: { It8888DmaFree(ctx); break; }
        case IOCTL_IT8888_DMA_INFO: { GET_OUT(IT8888_DMA_INFO, out); status=It8888DmaInfo(ctx,out); if(NT_SUCCESS(status)) COMPLETE(sizeof(*out)); break; }
        case IOCTL_IT8888_8237_RESET: { Vdma8237Reset(ctx); break; }
        case IOCTL_IT8888_8237_OUT: { GET_IN(IT8888_8237_PORT_OP, in); status=Vdma8237Out(ctx,in->Port,in->Value); break; }
        case IOCTL_IT8888_8237_IN: { GET_IN(IT8888_8237_PORT_OP, in); GET_OUT(IT8888_8237_PORT_OP, out); *out=*in; status=Vdma8237In(ctx,in->Port,&out->Value); if(NT_SUCCESS(status)) COMPLETE(sizeof(*out)); break; }
        case IOCTL_IT8888_8237_SNAPSHOT: { GET_OUT(IT8888_8237_SNAPSHOT, out); Vdma8237Snapshot(ctx,out); COMPLETE(sizeof(*out)); break; }
        case IOCTL_IT8888_8237_PREPARE: { GET_IN(IT8888_8237_PREPARE, in); GET_OUT(IT8888_8237_PREPARE, out); status=Vdma8237Prepare(ctx,in->Channel,out); if(NT_SUCCESS(status)) COMPLETE(sizeof(*out)); break; }
        case IOCTL_IT8888_DDMA_ARM: { GET_IN(IT8888_DDMA_REQUEST, in); GET_OUT(IT8888_DDMA_STATUS, out); status=It8888DdmaArm(ctx,in,out); if(NT_SUCCESS(status)) COMPLETE(sizeof(*out)); break; }
        case IOCTL_IT8888_DDMA_START: { GET_OUT(IT8888_DDMA_STATUS, out); status=It8888DdmaStart(ctx,out); if(NT_SUCCESS(status)) COMPLETE(sizeof(*out)); break; }
        case IOCTL_IT8888_DDMA_POLL: { GET_OUT(IT8888_DDMA_STATUS, out); status=It8888DdmaPoll(ctx,out); if(NT_SUCCESS(status)) COMPLETE(sizeof(*out)); break; }
        case IOCTL_IT8888_DDMA_STATUS: { GET_OUT(IT8888_DDMA_STATUS, out); It8888DdmaStatus(ctx,out); COMPLETE(sizeof(*out)); break; }
        case IOCTL_IT8888_DDMA_CLEAR: { It8888DdmaClear(ctx); break; }
        case IOCTL_IT8888_IRQ_STATUS: { GET_OUT(IT8888_IRQ_STATUS, out); RtlZeroMemory(out,sizeof(*out)); out->IrqCount=ctx->IrqCount; out->LastVector=ctx->LastIrqVector; out->LastStatus=ctx->LastIrqStatus; out->Pending=(UCHAR)(ctx->IrqPending!=0); COMPLETE(sizeof(*out)); break; }
        case IOCTL_IT8888_IRQ_ACK: { InterlockedExchange(&ctx->IrqPending,0); KeClearEvent(&ctx->IrqEvent); break; }
        case IOCTL_IT8888_WAIT_IRQ: { GET_IN(IT8888_WAIT_IRQ_REQUEST, in); LARGE_INTEGER timeout; timeout.QuadPart = -((LONGLONG)in->TimeoutMs * 10000LL); status=KeWaitForSingleObject(&ctx->IrqEvent, Executive, KernelMode, FALSE, in->TimeoutMs?&timeout:NULL); if(status==STATUS_SUCCESS) { } break; }
        case IOCTL_IT8888_TRACE_GET: { GET_OUT(IT8888_TRACE_PACKET, out); It8888TraceGet(ctx,out); COMPLETE(sizeof(*out)); break; }
        case IOCTL_IT8888_TRACE_CLEAR: { It8888TraceClear(ctx); break; }
        case IOCTL_IT8888_PANIC_RESET: { status=It8888PanicReset(ctx); break; }
        case IOCTL_IT8888_CLEAR_ERRORS: { status=It8888ClearErrors(ctx); break; }
        default: status = STATUS_INVALID_DEVICE_REQUEST; break;
        }
    } while (0);

    WdfRequestCompleteWithInformation(Request, status, info);
}
