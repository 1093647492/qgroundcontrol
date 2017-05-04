/*!
 * @file
 *   @brief Yuneec Firmware Plugin (PX4)
 *   @author Gus Grubba <mavlink@grubba.com>
 *
 */

#include "YuneecFirmwarePlugin.h"
#include "YuneecAutoPilotPlugin.h"
#include "CameraMetaData.h"

QVariantList YuneecFirmwarePlugin::_cameraList;

YuneecFirmwarePlugin::YuneecFirmwarePlugin(void)
{
    for (int i=0; i<_flightModeInfoList.count(); i++) {
        FlightModeInfo_t& info = _flightModeInfoList[i];

        // Only the following px4 flight modes are user selectable:
        if (info.name != _simpleFlightMode &&
                info.name != _posCtlFlightMode &&
                info.name != _altCtlFlightMode) {
            // No other flight modes can be set
            info.canBeSet = false;
        }
        qDebug() << *info.name << info.canBeSet;
    }

    //  The following flight modes are renamed:
    _posCtlFlightMode = tr("Angle");
    _rtlFlightMode = tr("RTL");
    _simpleFlightMode = tr("Smart");
    _manualFlightMode = tr("Manual (No GPS)");
    _altCtlFlightMode = tr("Manual");
}

AutoPilotPlugin* YuneecFirmwarePlugin::autopilotPlugin(Vehicle* vehicle)
{
    return new YuneecAutoPilotPlugin(vehicle, vehicle);
}

QString YuneecFirmwarePlugin::vehicleImageOpaque(const Vehicle* vehicle) const
{
    Q_UNUSED(vehicle);
    return QStringLiteral("/typhoonh/img/VehicleIndicator.svg");
}

QString YuneecFirmwarePlugin::vehicleImageOutline(const Vehicle* vehicle) const
{
    Q_UNUSED(vehicle);
    return QStringLiteral("/typhoonh/img/VehicleIndicator.svg");
}

QString YuneecFirmwarePlugin::vehicleImageCompass(const Vehicle* vehicle) const
{
    Q_UNUSED(vehicle);
    return QStringLiteral("/typhoonh/img/CompassIndicator.svg");
}

const QVariantList& YuneecFirmwarePlugin::toolBarIndicators(const Vehicle* vehicle)
{
    Q_UNUSED(vehicle);
    if(_toolBarIndicators.size() == 0) {
        _toolBarIndicators.append(QVariant::fromValue(QUrl::fromUserInput("qrc:/toolbar/MessageIndicator.qml")));
        _toolBarIndicators.append(QVariant::fromValue(QUrl::fromUserInput("qrc:/typhoonh/YGPSIndicator.qml")));
        _toolBarIndicators.append(QVariant::fromValue(QUrl::fromUserInput("qrc:/toolbar/TelemetryRSSIIndicator.qml")));
        _toolBarIndicators.append(QVariant::fromValue(QUrl::fromUserInput("qrc:/typhoonh/WIFIRSSIIndicator.qml")));
        _toolBarIndicators.append(QVariant::fromValue(QUrl::fromUserInput("qrc:/toolbar/RCRSSIIndicator.qml")));
        _toolBarIndicators.append(QVariant::fromValue(QUrl::fromUserInput("qrc:/toolbar/BatteryIndicator.qml")));
        _toolBarIndicators.append(QVariant::fromValue(QUrl::fromUserInput("qrc:/toolbar/ModeIndicator.qml")));
    }
    return _toolBarIndicators;
}

const QVariantList& YuneecFirmwarePlugin::cameraList(const Vehicle* vehicle)
{
    Q_UNUSED(vehicle);

    if (_cameraList.size() == 0) {
        CameraMetaData* metaData1;
        metaData1 = new CameraMetaData(tr("Typhoon H CGO3+"),   // Camera name
                                      6.264,                    // sensorWidth
                                      4.698,                    // sensorHeight
                                      4000,                     // imageWidth
                                      3000,                     // imageHeight
                                      14,                       // focalLength
                                      true,                     // true: landscape orientation
                                      true,                     // true: camera is fixed orientation
                                      this);                    // parent
        CameraMetaData* metaData2;
        metaData2 = new CameraMetaData(tr("Typhoon H E90"),     // Camera name
                                      13.3056,                  // sensorWidth
                                      8.656,                    // sensorHeight
                                      5472,                     // imageWidth
                                      3080,                     // imageHeight
                                      8.29,                     // focalLength
                                      true,                     // true: landscape orientation
                                      true,                     // true: camera is fixed orientation
                                      this);                    // parent
        _cameraList.append(QVariant::fromValue(metaData1));
        _cameraList.append(QVariant::fromValue(metaData2));
    }

    return _cameraList;
}

bool YuneecFirmwarePlugin::isGuidedMode(const Vehicle* vehicle) const
{
    if (vehicle->flightMode() == _posCtlFlightMode) {
        // PosCtl/Angle is supported for Guided
        return true;
    } else {
        return PX4FirmwarePlugin::isGuidedMode(vehicle);
    }
}

bool YuneecFirmwarePlugin::hasGimbal(Vehicle* vehicle, bool& rollSupported, bool& pitchSupported, bool& yawSupported)
{
    Q_UNUSED(vehicle);
    rollSupported = false;
    pitchSupported = true;
    yawSupported = true;
    return true;
}

void YuneecFirmwarePlugin::batteryConsumptionData(Vehicle* vehicle, int& mAhBattery, double& hoverAmps, double& cruiseAmps) const
{
    Q_UNUSED(vehicle);
    mAhBattery = 6850;
    hoverAmps = 15.2;
    cruiseAmps = hoverAmps * 1.2;
}

bool YuneecFirmwarePlugin::vehicleYawsToNextWaypointInMission(const Vehicle* vehicle) const
{
    // The Yuneec implementation of this method differs from standard PX4 in that it returns true for offline vehicles.
    // This way the Heading item in waypoints will not normally show up. It will only show up if the vehicle specfically
    // has MIS_YAWMODE modified from the default setting.
    if (vehicle->isOfflineEditingVehicle()) {
        return true;
    } else {
        return PX4FirmwarePlugin::vehicleYawsToNextWaypointInMission(vehicle);
    }
}
