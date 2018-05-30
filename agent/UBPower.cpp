#include "UBPower.h"
#include "UBPWRPacket.h"

#include "UBConfig.h"

#include <QHostAddress>

UBPower::UBPower(QTcpSocket *parent) : QTcpSocket(parent)
{
    connect(this, SIGNAL(readyRead()), this, SLOT(dataReadyEvent()));
}

void UBPower::sendData(quint8 cmd, const QByteArray& data) {
    UBPWRPacket packet;

    packet.setCMD(cmd);
    packet.setPayload(data);

    write(packet.packetize().append(PACKET_END));
}

void UBPower::dataReadyEvent() {
    m_data += readAll();

    while (true) {
        int bytes = m_data.indexOf(PACKET_END);
        if (bytes == -1) {
            break;
        }

        UBPWRPacket packet;
        packet.depacketize(m_data.left(bytes));
        m_data.remove(0, bytes + qstrlen(PACKET_END));

        emit dataReady(packet.getCMD(), packet.getPayload());

        qInfo() << "Power Sensor CMD Received | CMD " << packet.getCMD() << " | Size: " << packet.getPayload().size();
    }
}
