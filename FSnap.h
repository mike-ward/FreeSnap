
// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the FSNAP_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// FSNAP_API functions as being imported from a DLL, wheras this DLL sees symbols
// defined with this macro as being exported.
#ifdef FSNAP_EXPORTS
#define FSNAP_API __declspec(dllexport)
#else
#define FSNAP_API __declspec(dllimport)
#endif

#include <vector>

FSNAP_API bool snap_install(std::vector<SIZE> sizes, bool altKeys, bool undo, bool task_switch);
FSNAP_API bool snap_uninstall();
FSNAP_API LRESULT CALLBACK FSnapKeyboardProc(int code, WPARAM wpar, LPARAM lpar);
