#ifndef UDS_TRANSPORT_H
#define UDS_TRANSPORT_H

/* Schedule a reset/power action only after the last accepted response is ACKed. */
void UDS_AfterResponse(void (*action)(void));

#endif
