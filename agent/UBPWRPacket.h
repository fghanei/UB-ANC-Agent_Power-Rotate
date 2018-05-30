#ifndef UBPWRPACKET_H
#define UBPWRPACKET_H

#include <QObject>

class UBPWRPacket
{
public:
    explicit UBPWRPacket();

public slots:
    void setCMD(quint8 cmd) {m_cmd=cmd;}
    void setPayload(const QByteArray& payload) {m_payload = payload;}

    quint8 getCMD(void) {return m_cmd;}
    QByteArray getPayload(void) {return m_payload;}

    QByteArray packetize(void);
    void depacketize(const QByteArray &packet);

protected:
    quint8 m_cmd;

    QByteArray m_payload;
};

#endif // UBPWRPACKET_H
