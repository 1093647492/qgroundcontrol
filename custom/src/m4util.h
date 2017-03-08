/*!
 * @file
 *   @brief ST16 Controller
 *   @author Gus Grubba <mavlink@grubba.com>
 *
 */

#pragma once

#include "m4common.h"
#include "m4def.h"

extern uint8_t crc8(uint8_t* buffer, int len);

#define DEFAULT_TX_MAX_CHANNEL			24

typedef  struct  TableDeviceLocalInfo{
    uint8_t 	index;
    uint16_t    mode;
    uint16_t    nodeId;
    uint8_t 	parseIndex;
    uint8_t 	extAddr;
    uint16_t    panId;
    uint16_t    txAddr;
} TableDeviceLocalInfo_t;

typedef  struct  TableDeviceChannelInfo{
    uint8_t index;
    uint8_t aNum;
    uint8_t aBits;
    uint8_t trNum;
    uint8_t trBits;
    uint8_t swNum;
    uint8_t swBits;
    uint8_t replyChannelNum;
    uint8_t replyChannelBits;
    uint8_t requestChannelNum;
    uint8_t requestChannelBits;
    uint8_t extraNum;
    uint8_t extraBits;
    uint8_t analogType;
    uint8_t trimType;
    uint8_t switchType;
    uint8_t replyChannelType;
    uint8_t requestChannelType;
    uint8_t extraType;
} TableDeviceChannelInfo_t;

typedef  struct  TableDeviceChannelNumInfo{
    uint8_t index;
    uint8_t channelMap[DEFAULT_TX_MAX_CHANNEL];
} TableDeviceChannelNumInfo_t;

typedef enum {
    ChannelNumAanlog = 1,
    ChannelNumTrim,
    ChannelNumSwitch,
    ChannelNumMonitor,
    ChannelNumExtra,
} ChannelNumType_t;

class M4SerialComm;

//-----------------------------------------------------------------------------
//-- Accessor class to handle data structure in an "Yuneec" way
class m4Packet
{
public:
    m4Packet(QByteArray data_)
        : data(data_)
    {
    }
    int type()
    {
        return data[2] & 0xFF;
    }
    int commandID()
    {
        return data[9] & 0xFF;
    }
    int commandIdFromMission()
    {
        if(data.size() > 10)
            return data[10] & 0xff;
        else
            return 0;
    }
    int subCommandIDFromMission()
    {
        if(data.size() > 11)
            return data[11] & 0xff;
        else
            return 0;
    }
    QByteArray commandValues()
    {
        return data.mid(10); //-- Data - header = payload
    }
    int mixCommandId(int type, int commandId, int subCommandId)
    {
        if(type != Yuneec::COMMAND_TYPE_MISSION) {
            return commandId;
        } else {
            return type << 16 | commandId << 8 | subCommandId;
        }
    }
    int mixCommandId()
    {
        if(type() != Yuneec::COMMAND_TYPE_MISSION) {
            return commandID();
        } else {
            return mixCommandId(type(), commandIdFromMission(), subCommandIDFromMission());
        }
    }
    QByteArray data;
};

//-----------------------------------------------------------------------------
class RxBindInfo {
public:
    RxBindInfo();
    void clear();
    enum {
        TYPE_NULL   = -1,
        TYPE_SR12S  = 0,
        TYPE_SR12E  = 1,
        TYPE_SR24S  = 2,
        TYPE_RX24   = 3,
        TYPE_SR19P  = 4,
    };
    QString getName();
    int mode;
    int panId;
    int nodeId;
    int aNum;
    int aBit;
    int trNum;
    int trBit;
    int swNum;
    int swBit;
    int monitNum;
    int monitBit;
    int extraNum;
    int extraBit;
    int txAddr;
    QByteArray achName;
    QByteArray trName;
    QByteArray swName;
    QByteArray monitName;
    QByteArray extraName;
};

//-----------------------------------------------------------------------------
class ControllerLocation {
public:
    ControllerLocation()
        : longitude(0.0)
        , latitude(0.0)
        , altitude(0.0)
        , satelliteCount(0)
        , accuracy(0.0f)
        , speed(0.0f)
        , angle(0.0f)
    {
    }
    ControllerLocation& operator=(ControllerLocation& other)
    {
        longitude       = other.longitude;
        latitude        = other.latitude;
        altitude        = other.altitude;
        satelliteCount  = other.satelliteCount;
        accuracy        = other.accuracy;
        speed           = other.speed;
        angle           = other.angle;
        return *this;
    }
    /**
     * Longitude of remote-controller
     */
    double longitude;
    /**
     * Latitude of remote-controller
     */
    double latitude;
    /**
     * Altitude of remote-controller
     */
    double altitude;
    /**
     * The number of satellite has searched
     */
    int satelliteCount;

    /**
     * Accuracy of remote-controller
     */
    float accuracy;

    /**
     * Speed of remote-controller
     */
    float speed;

    /**
     * Angle of remote-controller
     */
    float angle;
};

//-----------------------------------------------------------------------------
class SwitchChanged {
public:
    /**
     * Hardware ID
     */
    int hwId;
    /**
     * Old status of hardware
     */
    int oldState;
    /**
     * New status of hardware
     */
    int newState;
};

#define m4CommandHeaderLen sizeof(m4CommandHeader)

//-----------------------------------------------------------------------------
// Base Yuneec Protocol Command
class m4Command
{
public:
    m4Command(int id, int type = Yuneec::COMMAND_TYPE_NORMAL)
    {
        data.fill(0, Yuneec::COMMAND_BODY_EXCLUDE_VALUES_LENGTH);
        data[2] = (uint8_t)type;
        data[9] = (uint8_t)id;
    }
    virtual ~m4Command() {}
    QByteArray pack(QByteArray payload = QByteArray())
    {
        if(payload.size()) {
            data.append(payload);
        }
        QByteArray command;
        command.resize(3);
        command[0] = 0x55;
        command[1] = 0x55;
        command[2] = (uint8_t)data.size() + 1;
        command.append(data);
        uint8_t crc = crc8((uint8_t*)data.data(), data.size());
        command.append(crc);
        return command;
    }
    QByteArray data;
};

//-----------------------------------------------------------------------------
// Base Yuneec Protocol Message
class m4Message
{
public:
    m4Message(int id, int type = 0)
    {
        data.fill(0, 8);
        data[2] = (uint8_t)type;
        data[3] = (uint8_t)id;
    }
    virtual ~m4Message() {}
    QByteArray pack()
    {
        QByteArray command;
        command.resize(3);
        command[0] = 0x55;
        command[1] = 0x55;
        command[2] = (uint8_t)data.size() + 1;
        command.append(data);
        uint8_t crc = crc8((uint8_t*)data.data(), data.size());
        command.append(crc);
        return command;
    }
    QByteArray data;
};
