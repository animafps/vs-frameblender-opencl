/* Generated by Cython 0.29.24 */

#ifndef __PYX_HAVE__vapoursynth
#define __PYX_HAVE__vapoursynth

#include "Python.h"

#ifndef __PYX_HAVE_API__vapoursynth

#ifndef __PYX_EXTERN_C
  #ifdef __cplusplus
    #define __PYX_EXTERN_C extern "C"
  #else
    #define __PYX_EXTERN_C extern
  #endif
#endif

#ifndef DL_IMPORT
  #define DL_IMPORT(_T) _T
#endif

__PYX_EXTERN_C int vpy4_createScript(VSScript *);
__PYX_EXTERN_C int vpy_evaluateScript(VSScript *, char const *, char const *, int);
__PYX_EXTERN_C int vpy_evaluateFile(VSScript *, char const *, int);
__PYX_EXTERN_C int vpy4_evaluateBuffer(VSScript *, char const *, char const *);
__PYX_EXTERN_C int vpy4_evaluateFile(VSScript *, char const *);
__PYX_EXTERN_C void vpy4_freeScript(VSScript *);
__PYX_EXTERN_C char const *vpy4_getError(VSScript *);
__PYX_EXTERN_C VSNode *vpy4_getOutput(VSScript *, int);
__PYX_EXTERN_C VSNode *vpy4_getAlphaOutput(VSScript *, int);
__PYX_EXTERN_C int vpy4_getAltOutputMode(VSScript *, int);
__PYX_EXTERN_C int vpy_clearOutput(VSScript *, int);
__PYX_EXTERN_C VSCore *vpy4_getCore(VSScript *);
__PYX_EXTERN_C VSAPI const *vpy4_getVSAPI(int);
__PYX_EXTERN_C int vpy4_getVariable(VSScript *, char const *, VSMap *);
__PYX_EXTERN_C int vpy4_setVariables(VSScript *, VSMap const *);
__PYX_EXTERN_C int vpy_clearVariable(VSScript *, char const *);
__PYX_EXTERN_C void vpy_clearEnvironment(VSScript *);
__PYX_EXTERN_C int vpy4_initVSScript(void);

#endif /* !__PYX_HAVE_API__vapoursynth */

/* WARNING: the interface of the module init function changed in CPython 3.5. */
/* It now returns a PyModuleDef instance instead of a PyModule instance. */

#if PY_MAJOR_VERSION < 3
PyMODINIT_FUNC initvapoursynth(void);
#else
PyMODINIT_FUNC PyInit_vapoursynth(void);
#endif

#endif /* !__PYX_HAVE__vapoursynth */