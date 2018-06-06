#ifndef UBAGENT_H
#define UBAGENT_H

#include <QObject>

class QTimer;
class Vehicle;
class UBNetwork;
class UBPower;

class UBAgent : public QObject
{
    Q_OBJECT
public:
    explicit UBAgent(QObject *parent = nullptr);

public slots:
    void startAgent();

protected slots:
    void setMAV(Vehicle* mav);

    void vehicleAddedEvent(Vehicle* mav);
    void vehicleRemovedEvent(Vehicle* mav);

    void armedChangedEvent(bool armed);
    void flightModeChangedEvent(QString mode);

    void dataReadyEvent(quint8 srcID, QByteArray data);
    void missionTracker();

protected:
    void stateIdle();
    void stateTakeoff();
    void stateMission();
    void stateLand();

protected:
    enum EMissionState {
        STATE_IDLE,
        STATE_TAKEOFF,
        STATE_MISSION,
        STATE_LAND,
    } m_mission_state;

    struct SMissionData {
        int stage;
        int tick;
        QString previousFlightMode;

        void reset() {
            stage = 0;
            tick = 0;
            previousFlightMode = "";
        }
    } m_mission_data;

protected:    
    Vehicle* m_mav;
    UBNetwork* m_net;
    UBPower* m_power;

    QTimer* m_timer;
};

#endif // UBAGENT_H
