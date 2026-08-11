import { runApplication } from './app.js';

void runApplication().catch((error: unknown) => {
  const message = error instanceof Error ? error.stack ?? error.message : String(error);
  console.error(`[fatal] ${message}`);
  process.exitCode = 1;
});
