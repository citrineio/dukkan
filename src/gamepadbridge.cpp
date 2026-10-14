#include "gamepadbridge.h"
#include <QGuiApplication>

#ifdef DUKKAN_HAS_SDL2
#include <SDL.h>
#endif

GamepadBridge::GamepadBridge(QObject *parent) : QObject(parent)
{
#ifdef DUKKAN_HAS_SDL2
    // SDL runs alongside Qt's Wayland input; it does not own the window.
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK | SDL_INIT_EVENTS) != 0)
        return;
    openController();
    connect(&m_timer, &QTimer::timeout, this, &GamepadBridge::poll);
    m_timer.start(30);
#endif
}

GamepadBridge::~GamepadBridge()
{
#ifdef DUKKAN_HAS_SDL2
    if (m_controller) SDL_GameControllerClose(m_controller);
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK | SDL_INIT_EVENTS);
#endif
}

void GamepadBridge::setConnected(bool connected)
{
    if (m_connected == connected) return;
    m_connected = connected;
    emit connectedChanged();
}

void GamepadBridge::poll()
{
#ifdef DUKKAN_HAS_SDL2
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        // SDL translates SIGTERM/SIGINT into SDL_QUIT. Handle it even while
        // the Guide owns focus, so systemd does not wait for its kill timeout.
        if (event.type == SDL_QUIT) {
            QCoreApplication::quit();
            return;
        } else if (event.type == SDL_CONTROLLERDEVICEADDED) {
            openController();
        } else if (event.type == SDL_CONTROLLERDEVICEREMOVED) {
            if (m_controller && !SDL_GameControllerGetAttached(m_controller)) {
                SDL_GameControllerClose(m_controller);
                m_controller = nullptr;
                setConnected(false);
                m_heldAxis.clear();
                openController();
            }
        } else if (event.type == SDL_CONTROLLERBUTTONDOWN) {
            QString name;
            switch (event.cbutton.button) {
                case SDL_CONTROLLER_BUTTON_A: name = QStringLiteral("select"); break;
                case SDL_CONTROLLER_BUTTON_B: name = QStringLiteral("back"); break;
                case SDL_CONTROLLER_BUTTON_X: name = QStringLiteral("refresh"); break;
                case SDL_CONTROLLER_BUTTON_Y: name = QStringLiteral("library"); break;
                case SDL_CONTROLLER_BUTTON_START: name = QStringLiteral("search"); break;
                case SDL_CONTROLLER_BUTTON_DPAD_UP: name = QStringLiteral("up"); break;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN: name = QStringLiteral("down"); break;
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT: name = QStringLiteral("left"); break;
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: name = QStringLiteral("right"); break;
                default: break;
            }
            if (!name.isEmpty() && QGuiApplication::focusWindow()) emit action(name);
        }
    }
    if (!m_controller || !QGuiApplication::focusWindow()) {
        m_heldAxis.clear();
        m_axisDelay = 0;
        return;
    }
    const int x = SDL_GameControllerGetAxis(m_controller, SDL_CONTROLLER_AXIS_LEFTX);
    const int y = SDL_GameControllerGetAxis(m_controller, SDL_CONTROLLER_AXIS_LEFTY);
    QString direction;
    if (qAbs(x) > qAbs(y) && qAbs(x) > 21000)
        direction = x > 0 ? QStringLiteral("right") : QStringLiteral("left");
    else if (qAbs(y) > 21000)
        direction = y > 0 ? QStringLiteral("down") : QStringLiteral("up");
    if (direction != m_heldAxis) {
        m_heldAxis = direction;
        m_axisDelay = 0;
        if (!direction.isEmpty()) emit action(direction);
    } else if (!direction.isEmpty()) {
        ++m_axisDelay;
        if (m_axisDelay >= 10 && (m_axisDelay - 10) % 4 == 0)
            emit action(direction);
    }
#endif
}

#ifdef DUKKAN_HAS_SDL2
void GamepadBridge::openController()
{
    if (m_controller) return;
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (!SDL_IsGameController(i)) continue;
        m_controller = SDL_GameControllerOpen(i);
        if (m_controller) {
            setConnected(true);
            return;
        }
    }
}
#endif
