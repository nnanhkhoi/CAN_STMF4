#ifndef APP_RUNNABLES_H
#define APP_RUNNABLES_H

/* Ordered runnable groups. Keep each group within its period; do not block. */
void App_Runnables10ms(void);
void App_Runnables20ms(void);
void App_Runnables50ms(void);

#endif
