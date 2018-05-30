#include "UBPWRPacket.h"

UBPWRPacket::UBPWRPacket() : m_cmd(0)
{

}

QByteArray UBPWRPacket::packetize(void) {
    QByteArray cmd(((char*)(&m_cmd)), sizeof(quint8));

    return cmd + m_payload;
}

void UBPWRPacket::depacketize(const QByteArray& packet) {
    m_cmd = *((quint8*)(packet.mid(0, sizeof(quint8)).data()));

    m_payload = packet.mid(0 + sizeof(quint8) + sizeof(quint8));
}
