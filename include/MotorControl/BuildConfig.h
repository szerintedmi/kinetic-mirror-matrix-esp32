#pragma once

// Global build-time switches used across the codebase.
// Select shared-STEP spike (0 = FastAccelStepper path, 1 = SharedSTEP path).
#ifndef USE_SHARED_STEP
#define USE_SHARED_STEP 0
#endif

