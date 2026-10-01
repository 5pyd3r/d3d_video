#include "MessageLoop.h"

int MessageLoop::Run(ICallback* cb) {
    MSG msg = {};

    for (;;) {
        BOOL hasMsg = PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE);
        if (hasMsg) {
            if (msg.message == WM_QUIT) break;
            bool handled = false;
            cb->OnMessage(msg, handled);
            if (handled) continue;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            cb->OnIdle();
        }
    }

    return static_cast<int>(msg.wParam);
}
