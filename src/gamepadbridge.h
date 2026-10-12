#pragma once

#include <QObject>
#include <QTimer>
#include <QString>

#ifdef DUKKAN_HAS_SDL2
#include <SDL.h>
#endif

class GamepadBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
public:
    explicit GamepadBridge(QObject *parent = nullptr);
    ~GamepadBridge() override;
    bool connected() const { return m_connected; }

signals:
    void action(const QString &name);
    void connectedChanged();

private:
    void poll();
    void setConnected(bool value);
    bool m_connected = false;
    QTimer m_timer;
#ifdef DUKKAN_HAS_SDL2
    SDL_GameController *m_controller = nullptr;
    void openController();
    QString m_heldAxis;
    int m_axisDelay = 0;
#endif
};
