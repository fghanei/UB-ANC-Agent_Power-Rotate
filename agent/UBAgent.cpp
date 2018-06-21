#include "UBAgent.h"
#include "UBNetwork.h"
#include "UBPower.h"

#include "UBConfig.h"

#include <QTime>
#include <QTimer>
#include <QCommandLineParser>

#include "Vehicle.h"
#include "TCPLink.h"
#include "QGCApplication.h"

#define RC_OVERRIDE_YAW 1600

UBAgent::UBAgent(QObject *parent) : QObject(parent),
    m_mav(nullptr)
{
    m_net = new UBNetwork;
    connect(m_net, SIGNAL(dataReady(quint8, QByteArray)), this, SLOT(dataReadyEvent(quint8, QByteArray)));

    m_power = new UBPower;
    connect(m_power, SIGNAL(dataReady(quint8, QByteArray)), this, SLOT(dataReadyEvent(quint8, QByteArray)));

    m_timer = new QTimer;
    connect(m_timer, SIGNAL(timeout()), this, SLOT(missionTracker()));

    startAgent();
}

void UBAgent::startAgent() {
    QCommandLineParser parser;
    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);

    parser.addOptions({
        {{"I", "instance"}, "Set instance (ID) of the agent", "id"},
    });

//    parser.process(*QCoreApplication::instance());
    parser.parse(QCoreApplication::arguments());

    quint8 id = parser.value("I").toUInt();
    LinkConfiguration* link = nullptr;
    if (id) {
        quint32 port = 10 * id + STL_PORT + 3;
        TCPConfiguration* tcp = new TCPConfiguration(tr("TCP Port %1").arg(port));
        tcp->setAddress(QHostAddress::LocalHost);
        tcp->setPort(port);

        link = tcp;
    } else {
        SerialConfiguration* serial = new SerialConfiguration("Serial Port");
        serial->setBaud(BAUD_RATE);
        serial->setPortName(SERIAL_PORT);

        link = serial;
    }

    link->setDynamic();
    link->setAutoConnect();

    LinkManager* linkManager = qgcApp()->toolbox()->linkManager();
    linkManager->addConfiguration(link);
    linkManager->linkConfigurationsChanged();

    connect(qgcApp()->toolbox()->multiVehicleManager(), SIGNAL(vehicleAdded(Vehicle*)), this, SLOT(vehicleAddedEvent(Vehicle*)));
    connect(qgcApp()->toolbox()->multiVehicleManager(), SIGNAL(vehicleRemoved(Vehicle*)), this, SLOT(vehicleRemovedEvent(Vehicle*)));

    m_net->connectToHost(QHostAddress::LocalHost, 10 * id + NET_PORT);
    m_power->connectToHost(QHostAddress::LocalHost, PWR_PORT);
    m_timer->start(1000.0*MISSION_TRACK_DELAY);

    m_mission_data.reset();
}

void UBAgent::setMAV(Vehicle* mav) {
    if (m_mav) {
        disconnect(m_mav, SIGNAL(armedChanged(bool)), this, SLOT(armedChangedEvent(bool)));
        disconnect(m_mav, SIGNAL(flightModeChanged(QString)), this, SLOT(flightModeChangedEvent(QString)));
    }

    m_mav = mav;

    if (m_mav) {
        connect(m_mav, SIGNAL(armedChanged(bool)), this, SLOT(armedChangedEvent(bool)));
        connect(m_mav, SIGNAL(flightModeChanged(QString)), this, SLOT(flightModeChangedEvent(QString)));
    }
}

void UBAgent::vehicleAddedEvent(Vehicle* mav) {
    if (!mav || m_mav == mav) {
        return;
    }

    setMAV(mav);
    m_net->setID(mav->id());

    qInfo() << "New MAV connected with ID: " << m_mav->id();
}

void UBAgent::vehicleRemovedEvent(Vehicle* mav) {
    if (!mav || m_mav != mav) {
        return;
    }

    setMAV(nullptr);
    m_net->setID(0);

    qInfo() << "MAV disconnected with ID: " << mav->id();
}

void UBAgent::armedChangedEvent(bool armed) {
    if (!armed) {
        if (m_mission_state!=STATE_LAND) { //do not interfere lnding procedure.
            m_mission_state = STATE_IDLE;
        }
        return;
    }

    if (m_mav->altitudeRelative()->rawValue().toDouble() > POINT_ZONE) {
        qWarning() << "The mission can not start while the drone is airborne!";
        return;
    }

//    m_mav->setGuidedMode(true);
    if (!m_mav->guidedMode()) {
        qWarning() << "The mission can not start while the drone is not in Guided mode!";
        return;
    }

    m_mission_data.reset();
    qInfo() << "Mission starts...";
    qInfo() << "Starting measurement";
    m_power->sendData(UBPower::PWR_START, QByteArray());

    m_mission_state = STATE_TAKEOFF;
//    m_mav->guidedModeTakeoff();
    m_mav->sendMavCommand(m_mav->defaultComponentId(),
                            MAV_CMD_NAV_TAKEOFF,
                            true, // show error
                            0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                            TAKEOFF_ALT);
}

void UBAgent::flightModeChangedEvent(QString mode) {
    qInfo() << mode;
    // automatic arm after switching from Land to Guided. be careful!
    if (m_mission_data.previousFlightMode == "Land" && 
        mode=="Guided" && !m_mav->armed()) {
            m_mav->setArmed(true);
    }
    m_mission_data.previousFlightMode = mode;
}

void UBAgent::dataReadyEvent(quint8 srcID, QByteArray data) {
//    Q_UNUSED(data)
    qInfo() << "Data received from srcID=" << srcID << ":\n" << data;
//    This is used to arm one UAV by previous one.
//    if(srcID == m_mav->id() - 1 && !m_mav->armed()) {
//        m_mav->setArmed(true);
//    }
}

void UBAgent::missionTracker() {
    switch (m_mission_state) {
    case STATE_IDLE:
        stateIdle();
        break;
    case STATE_TAKEOFF:
        stateTakeoff();
        break;
    case STATE_MISSION:
        stateMission();
        break;
    case STATE_LAND:
        stateLand();
        break;
    default:
        break;
    }
}

void UBAgent::stateIdle() {
}

void UBAgent::stateTakeoff() {
    if (m_mission_data.stage == 0) {
    	if (m_mav->altitudeRelative()->rawValue().toDouble() > TAKEOFF_ALT - POINT_ZONE) {
            m_mission_data.tick=0;
            m_mission_data.stage++;
            mavlink_message_t msg;
            mavlink_msg_rc_channels_override_pack(
			255, 1, &msg,
			m_mav->id(), m_mav->defaultComponentId(),
                        0, 0, 0, RC_OVERRIDE_YAW, 0, 0, 0, 0);
            m_mav->sendMessageOnLink(m_mav->priorityLink(), msg);
	}
    }
    if (m_mission_data.stage == 1) {
        m_mission_data.tick++;
	if (m_mission_data.tick > (3.0 * 1.0 / MISSION_TRACK_DELAY - 0.001)) { //waiting for 3 seconds to stabilize
            m_mission_data.stage = 0;
            m_mission_state = STATE_MISSION;
            qInfo() << "Takeoff completed.";
        }
    }
}

void UBAgent::stateLand() {
    switch (m_mission_data.stage) {
        case (0): {
            if (m_mav->altitudeRelative()->rawValue().toDouble() < POINT_ZONE) {
                m_mission_data.stage++;
                qInfo() << "Land completed. Waiting for disarm";
            }
            break;
        }
        case (1): {
            if (!m_mav->armed()) {
                m_mission_data.tick=0;
                m_mission_data.stage++;
                qInfo() << "Motors stopped. Waiting for 3 seconds.";
            }
            break;
        }
        case (2): {
            m_mission_data.tick++;
            if (m_mission_data.tick >= (3.0 * 1.0 / MISSION_TRACK_DELAY - 0.001)) { //waiting for 3 seconds to stabilize
                m_mission_data.stage = 0;
                m_mission_state = STATE_IDLE;
                qInfo() << "Finishing measurement.";
                m_power->sendData(UBPower::PWR_STOP, QByteArray());
            }
            break;
         }
    }
}

void UBAgent::stateMission() {
    static QGeoCoordinate dest;
    QByteArray info;
    unsigned long int now;
    int hover_time=120;

    now = QDateTime::currentMSecsSinceEpoch();
    switch (m_mission_data.stage) {
        case (0): {
            m_mission_data.tick = 0;
            m_mission_data.stage++;
            qInfo() << "Sending EVENT packet";
            m_power->sendData(UBPower::PWR_EVENT, QByteArray());
            break;
        }
        // hover
        case (1): {
            m_mission_data.tick++;
            if (m_mission_data.tick%2==0) {
	            // sending information to the logger, this works for tick = half a second
	            info.clear();
	            info += QByteArray::number(now/1000.0, 'f', 3);
	            info += " Seconds passed: ";
	            info += QByteArray::number(m_mission_data.tick/2);
	            m_power->sendData(UBPower::PWR_INFO, info);
            }

            if (m_mission_data.tick >= (hover_time * 1.0 / MISSION_TRACK_DELAY - 0.001)) {
                qInfo() << "Hover complete, sending EVENT packet";
                m_power->sendData(UBPower::PWR_EVENT, QByteArray());
                qInfo() << "Landing....";
                mavlink_message_t msg;
                mavlink_msg_rc_channels_override_pack(
			255, 1, &msg,
			m_mav->id(), m_mav->defaultComponentId(),
                        0, 0, 0, 0, 0, 0, 0, 0);
                m_mav->sendMessageOnLink(m_mav->priorityLink(), msg);
                m_mission_data.stage=0;
                m_mission_state = STATE_LAND;
                m_mav->guidedModeLand();
//              m_mission_data.stage++;
            }
            break;
         }
//        //  arming next uav
//        case (2): {
//            m_net->sendData(m_mav->id() + 1, QByteArray(1, MAV_CMD_NAV_TAKEOFF));
//            m_mission_data.stage++;
//        }
//        // move
//        case (3): {
//            dest = m_mav->coordinate().atDistanceAndAzimuth(10, 90); // 0 -> North, 90 (M_PI / 2) -> East
//            m_mav->guidedModeGotoLocation(dest);
//        }    
    }
}
