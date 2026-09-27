#include "app_runnables.h"
#include "app_can.h"

/* Execute application runnables assigned to the 10 ms timing group. */
void App_Runnables10ms(void)
{
  App_CanMainFunction10ms();
  /* Add fast application runnables here, in execution order. */
}

/* Execute application runnables assigned to the 20 ms timing group. */
void App_Runnables20ms(void)
{
  /* Add application/control runnables here, in execution order. */
}

/* Execute application runnables assigned to the 50 ms timing group. */
void App_Runnables50ms(void)
{
  App_CanDiagnostics50ms();
  /* Add slow monitoring runnables here, in execution order. */
}
