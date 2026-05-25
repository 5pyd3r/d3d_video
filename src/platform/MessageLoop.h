#ifndef PLATFORM_MESSAGELOOP_H
#define PLATFORM_MESSAGELOOP_H

#include <windows.h>

class MessageLoop {
public:
    struct ICallback {
        virtual ~ICallback() = default;
        virtual void OnIdle() = 0;
        // Return true if message was handled and should NOT be dispatched.
        virtual bool OnMessage(MSG& msg) = 0;
    };

    int Run(HWND hwnd, ICallback* cb);
    void Quit();

private:
    bool m_quitFlag = false;
};

#endif
