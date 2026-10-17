#pragma once

// Header stabile: va conservato nel repository Git.
// Il BAT genera GitVersion.generated.h durante il prebuild.
// La Deep Search di Visual Micro funziona anche quando il file generato manca.
#if defined(__has_include)
#  if __has_include("GitVersion.generated.h")
#    include "GitVersion.generated.h"
#  endif
#endif

#ifndef GIT_VERSION
#  define GIT_VERSION "XXXXXXX"
#endif
