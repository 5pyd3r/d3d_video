#ifndef PLATFORM_MESSAGELOOP_H
#define PLATFORM_MESSAGELOOP_H

#include <windows.h>

class MessageLoop {
public:
    struct ICallback {
        virtual ~ICallback() = default;
        virtual void OnIdle() = 0;
        // Set handled=true and return the LRESULT if handled.
        // Leave handled=false to dispatch via Translate/Dispatch.
        virtual LRESULT OnMessage(MSG& msg, bool& handled) = 0;
    };

    // Runs until WM_QUIT arrives and returns its exit code.
    int Run(HWND hwnd, ICallback* cb);
};

#endif
