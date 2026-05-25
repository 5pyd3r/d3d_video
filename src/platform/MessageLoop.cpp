#include "MessageLoop.h"

int MessageLoop::Run(HWND hwnd, ICallback* cb) {
    m_quitFlag = false;
    MSG msg = {};

    while (!m_quitFlag) {
        BOOL hasMsg = PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE);
        if (hasMsg) {
            if (msg.message == WM_QUIT) break;
            if (!cb->OnMessage(msg)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        } else {
            cb->OnIdle();
        }
    }

    return static_cast<int>(msg.wParam);
}

void MessageLoop::Quit() {
    m_quitFlag = true;
}
