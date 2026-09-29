#pragma once
// Voice input itself was removed from SugiIME, but the worker-pipe framing helpers in the
// engine contract are header-only and still referenced by ipc.cpp. Keep the shim so the IPC
// layer links; nothing calls the voice path anymore.
#include "engine/contracts/voice_composition_pipe.h"
