#ifndef PANIC_HANDLER_H
#define PANIC_HANDLER_H

/**
 * Panic Handler
 * 
 * Registers handlers to catch crashes, panics, and exceptions
 * and write them to the log buffer before the system resets.
 * 
 * Call registerPanicHandler() early in setup() to enable crash logging.
 */

/**
 * Register the panic handler
 * Should be called early in setup() before other initialization
 */
void registerPanicHandler();

#endif // PANIC_HANDLER_H

