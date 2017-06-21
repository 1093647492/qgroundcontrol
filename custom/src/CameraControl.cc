/*!
 * @file
 *   @brief Camera Controller
 *   @author Gus Grubba <mavlink@grubba.com>
 *
 */


#include "TyphoonHPlugin.h"
#include "TyphoonHM4Interface.h"
#include "CameraControl.h"
#include "QGCApplication.h"
#include "SettingsManager.h"
#include "QGCMapEngine.h"
#include "VideoManager.h"

#include <QDir>

QGC_LOGGING_CATEGORY(YuneecCameraLog, "YuneecCameraLog")
QGC_LOGGING_CATEGORY(YuneecCameraLogVerbose, "YuneecCameraLogVerbose")

//-----------------------------------------------------------------------------
// Video Resolution Options (CGO3+)
video_res_t videoResE50[] = {
    {"4096 x 2160 25fps (4K DCI)",    4096, 2160, 25},
    {"4096 x 2160 24fps (4K DCI)",    4096, 2160, 24},
    {"3840 x 2160 30fps (4K UHD)",    3840, 2160, 30},
    {"3840 x 2160 25fps (4K UHD)",    3840, 2160, 25},
    {"3840 x 2160 24fps (4K UHD)",    3840, 2160, 24},
    {"2560 x 1440 30fps (WQHD)",      2560, 1440, 30},
    {"2560 x 1440 25fps (WQHD)",      2560, 1440, 25},
    {"2560 x 1440 24fps (WQHD)",      2560, 1440, 24},
    {"1920 x 1080 120fps (1080P)",    1920, 1080, 120},
    {"1920 x 1080 60fps (1080P)",     1920, 1080, 60},
    {"1920 x 1080 50fps (1080P)",     1920, 1080, 50},
    {"1920 x 1080 48fps (1080P)",     1920, 1080, 48},
    {"1920 x 1080 30fps (1080P)",     1920, 1080, 30},
    {"1920 x 1080 25fps (1080P)",     1920, 1080, 25},
    {"1920 x 1080 24fps (1080P)",     1920, 1080, 24}
};

#define NUM_E50_VIDEO_RES (sizeof(videoResE50) / sizeof(video_res_t))

//-----------------------------------------------------------------------------
// Video Resolution Options (E90)
video_res_t videoResE90[] = {
    {"4096 x 2160 60fps (4K DCI)",    4096, 2160, 60},
    {"4096 x 2160 48fps (4K DCI)",    4096, 2160, 48},
    {"4096 x 2160 30fps (4K DCI)",    4096, 2160, 30},
    {"4096 x 2160 24fps (4K DCI)",    4096, 2160, 24},

    {"3840 x 2160 60fps (4K UHD)",    3840, 2160, 60},
    {"3840 x 2160 48fps (4K UHD)",    3840, 2160, 48},
    {"3840 x 2160 30fps (4K UHD)",    3840, 2160, 30},
    {"3840 x 2160 24fps (4K UHD)",    3840, 2160, 24},

    {"2515 x 1530 60fps (WQHD)",      2515, 1530, 60},
    {"2515 x 1530 48fps (WQHD)",      2515, 1530, 48},
    {"2515 x 1530 30fps (WQHD)",      2515, 1530, 30},
    {"2515 x 1530 24fps (WQHD)",      2515, 1530, 24},

    {"1920 x 1080 120fps (1080P)",    1920, 1080, 120},
    {"1920 x 1080 60fps (1080P)",     1920, 1080, 60},
    {"1920 x 1080 48fps (1080P)",     1920, 1080, 48},
    {"1920 x 1080 30fps (1080P)",     1920, 1080, 30},
    {"1920 x 1080 24fps (1080P)",     1920, 1080, 24},

    {"1280 x 720 120fps (720P)",      1280,  720, 120},
    {"1280 x 720 60fps (720P)",       1280,  720, 60},
    {"1280 x 720 48fps (720P)",       1280,  720, 48},
    {"1280 x 720 30fps (720P)",       1280,  720, 30},
    {"1280 x 720 24fps (720P)",       1280,  720, 24}
};

#define NUM_E90_VIDEO_RES (sizeof(videoResE90) / sizeof(video_res_t))

static video_res_t*   current_camera_video_res = &videoResE50[0];
static quint32        current_camera_video_res_count = NUM_E50_VIDEO_RES;

//-----------------------------------------------------------------------------
// Color Mode (CMD=SET_IQ_TYPE&mode=x)
iq_mode_t iqModeOptions[] = {
    {QT_TRANSLATE_NOOP("CameraControl", "Natural")},
    {QT_TRANSLATE_NOOP("CameraControl", "Enhanced")},
    {QT_TRANSLATE_NOOP("CameraControl", "Unprocessed")},
    {QT_TRANSLATE_NOOP("CameraControl", "Night")}
};

#define NUM_IQ_MODES (sizeof(iqModeOptions) / sizeof(iq_mode_t))

//-----------------------------------------------------------------------------
// Metering Mode (CMD=SET_METERING_MODE&mode=x)
metering_mode_t meteringModeOptions[] = {
    {QT_TRANSLATE_NOOP("CameraControl", "Spot"),    0},
    {QT_TRANSLATE_NOOP("CameraControl", "Center"),  1},
    {QT_TRANSLATE_NOOP("CameraControl", "Average"), 2}
};

#define NUM_METERING_VALUES (sizeof(meteringModeOptions) / sizeof(metering_mode_t))

//-----------------------------------------------------------------------------
// Photo Format Mode (CMD=SET_PHOTO_FORMAT&format=dng)
// E90 does not support RAW only
photo_format_t photoFormatOptionsE90[] = {
    {"Jpeg", 0},
    {"DNG + Jpeg", 2}
};

// Photo Format Mode (CMD=SET_PHOTO_FORMAT&format=dng)
// E50/CGO3+ support RAW only
photo_format_t photoFormatOptionsE50[] = {
    {"Jpeg", 0},
    {"DNG", 1},
    {"DNG + Jpeg", 2}
};

#define NUM_E90_PHOTO_FORMAT_VALUES (sizeof(photoFormatOptionsE90) / sizeof(photo_format_t))
#define NUM_E50_PHOTO_FORMAT_VALUES (sizeof(photoFormatOptionsE50) / sizeof(photo_format_t))

static photo_format_t*current_camera_photo_fmt = &photoFormatOptionsE50[0];
static quint32        current_camera_photo_fmt_count = NUM_E50_PHOTO_FORMAT_VALUES;

//-----------------------------------------------------------------------------
// White Balance (CMD=SET_WHITEBLANCE_MODE&mode=x)
white_balance_t whiteBalanceOptions[] = {
    {QT_TRANSLATE_NOOP("CameraControl", "Auto"),            0},
    {QT_TRANSLATE_NOOP("CameraControl", "Sunny"),           6500},
    {QT_TRANSLATE_NOOP("CameraControl", "Cloudy"),          7500},
    {QT_TRANSLATE_NOOP("CameraControl", "Fluorescent"),     4000},
    {QT_TRANSLATE_NOOP("CameraControl", "Incandescent"),    2800},
    {QT_TRANSLATE_NOOP("CameraControl", "Sunset"),          5000}
};

#define NUM_WB_VALUES (sizeof(whiteBalanceOptions) / sizeof(white_balance_t))

//-----------------------------------------------------------------------------
// ISO Values
iso_values_t isoValues[] = {
    {"100",   100},
    {"150",   150},
    {"200",   200},
    {"300",   300},
    {"400",   400},
    {"600",   600},
    {"800",   800},
    {"1600", 1600},
    {"3200", 3200}
};

#define NUM_ISO_VALUES (sizeof(isoValues) / sizeof(iso_values_t))

//-----------------------------------------------------------------------------
// Shutter Speeds
shutter_speeds_t shutterSpeeds[] = {
    { "4s", 4.0f},
    { "3s", 3.0f},
    { "2s", 2.0f},
    { "1s", 1.0f},
    { "1/30",   1.0f/30.0f},
    { "1/60",   1.0f/60.0f},
    { "1/125",  1.0f/125.0f},
    { "1/250",  1.0f/250.0f},
    { "1/500",  1.0f/500.0f},
    { "1/1000", 1.0f/1000.0f},
    { "1/2000", 1.0f/2000.0f},
    { "1/4000", 1.0f/4000.0f},
    { "1/8000", 1.0f/8000.0f}
};

#define NUM_SHUTTER_VALUES (sizeof(shutterSpeeds) / sizeof(shutter_speeds_t))

//-----------------------------------------------------------------------------
// Exposure Compensation
exposure_compsensation_t evOptions[] = {
    { "-3.0", -3.0f},
    { "-2.5", -2.5f},
    { "-2.0", -2.0f},
    { "-1.5", -1.5f},
    { "-1.0", -1.0f},
    { "-0.5", -0.5f},
    {   "0",  0.0f},
    { "+0.5", 0.5f},
    { "+1.0", 1.0f},
    { "+1.5", 1.5f},
    { "+2.0", 2.0f},
    { "+2.5", 2.5f},
    { "+3.0", 3.0f},
};

#define NUM_EV_VALUES       (sizeof(evOptions) / sizeof(exposure_compsensation_t))

exposure_compsensation_t*   evOptionsE50 = &evOptions[2];
#define NUM_EV_VALUES_E50   (NUM_EV_VALUES - 4)

//-----------------------------------------------------------------------------
CameraControl::CameraControl(QObject* parent)
    : QObject(parent)
    , _vehicle(NULL)
    , _currentTask(MAV_CMD_REQUEST_STORAGE_INFORMATION)
    , _cameraSupported(CAMERA_SUPPORT_UNDEFINED)
    , _true_cam_mode(CAMERA_MODE_UNDEFINED)
    , _camInfoTries(0)
    , _currentVideoResIndex(0)
    , _currentWB(0)
    , _currentIso(0)
    , _tempIso(0)
    , _minShutter(0)
    , _currentShutter(0)
    , _tempShutter(0)
    , _cameraVersion(0)
{
    _resetCameraValues();
    _cameraSound.setSource(QUrl::fromUserInput("qrc:/typhoonh/wav/camera.wav"));
    _cameraSound.setLoopCount(1);
    _cameraSound.setVolume(0.9);
    _videoSound.setSource(QUrl::fromUserInput("qrc:/typhoonh/wav/beep.wav"));
    _videoSound.setVolume(0.9);
    _errorSound.setSource(QUrl::fromUserInput("qrc:/typhoonh/wav/boop.wav"));
    _errorSound.setVolume(0.9);
}

//-----------------------------------------------------------------------------
CameraControl::~CameraControl()
{
}

//-----------------------------------------------------------------------------
void
CameraControl::setVehicle(Vehicle* vehicle)
{
    _resetCameraValues();
    _cameraSupported = CAMERA_SUPPORT_UNDEFINED;
    _currentTask     = MAV_CMD_REQUEST_STORAGE_INFORMATION;
    _camInfoTries    = 0;
    emit cameraModeChanged();
    emit videoStatusChanged();
    emit photoStatusChanged();
    if(_vehicle) {
        _vehicle = NULL;
        disconnect(&_statusTimer,   &QTimer::timeout, this, &CameraControl::_timerHandler);
        disconnect(&_recTimer,      &QTimer::timeout, this, &CameraControl::_recTimerHandler);
        disconnect(_vehicle,        &Vehicle::mavlinkMessageReceived, this, &CameraControl::_mavlinkMessageReceived);
        disconnect(_vehicle,        &Vehicle::mavCommandResult,       this, &CameraControl::_mavCommandResult);
        _cameraSupported = CAMERA_SUPPORT_UNDEFINED;
    }
    if(vehicle) {
        _vehicle = vehicle;
        connect(&_statusTimer,  &QTimer::timeout, this, &CameraControl::_timerHandler);
        connect(&_recTimer,     &QTimer::timeout, this, &CameraControl::_recTimerHandler);
        connect(_vehicle,       &Vehicle::mavlinkMessageReceived, this, &CameraControl::_mavlinkMessageReceived);
        connect(_vehicle,       &Vehicle::mavCommandResult,       this, &CameraControl::_mavCommandResult);
        _statusTimer.setSingleShot(true);
        _recTimer.setSingleShot(false);
        _recTimer.setInterval(333);
        //-- Ambarella Interface
        _initStreaming();
        //-- Request Camera Info
        _requestCameraInfo();
    }
}

//-----------------------------------------------------------------------------
void
CameraControl::_initStreaming()
{
    //-- Set RTSP resolution to 480P
    //-- TODO: No API
}

//-----------------------------------------------------------------------------
QString
CameraControl::sdFreeStr()
{
    return QGCMapEngine::bigSizeToString((quint64)_ambarellaStatus.sdfree * 1024);
}

//-----------------------------------------------------------------------------
void
CameraControl::takePhoto()
{
    qCDebug(YuneecCameraLog) << "takePhoto()";
    if(_vehicle && _cameraSupported == CAMERA_SUPPORT_YES && photoStatus() == PHOTO_CAPTURE_STATUS_IDLE) {
        _vehicle->sendMavCommand(
            MAV_COMP_ID_CAMERA,                         // Target component
            MAV_CMD_IMAGE_START_CAPTURE,                // Command id
            true,                                       // ShowError
            0,                                          // Camera ID (0 for all cameras), 1 for first, 2 for second, etc.
            0,                                          // Duration between two consecutive pictures (in seconds--ignored if single image)
            1,                                          // Number of images to capture total - 0 for unlimited capture
            -1,                                         // Horizontal resolution in pixels (set to -1 for highest resolution possible)
            -1);                                        // Vertical resolution in pixels (set to -1 for highest resolution possible)
        _cameraSound.setLoopCount(1);
        _cameraSound.play();
        //-- Capture local image as well
        QString photoPath = qgcApp()->toolbox()->settingsManager()->appSettings()->savePath()->rawValue().toString() + QStringLiteral("/Photo");
        QDir().mkpath(photoPath);
        photoPath += + "/" + QDateTime::currentDateTime().toString("yyyy-MM-dd_hh.mm.ss.zzz") + ".jpg";
        qgcApp()->toolbox()->videoManager()->videoReceiver()->grabImage(photoPath);
    } else {
        _errorSound.setLoopCount(1);
        _errorSound.play();
    }
}

//-----------------------------------------------------------------------------
void
CameraControl::startVideo()
{
    qCDebug(YuneecCameraLog) << "startVideo()";
    if(_vehicle && videoStatus() == VIDEO_CAPTURE_STATUS_STOPPED && _ambarellaSettings.cam_mode == CAMERA_MODE_VIDEO) {
        int w = -1;
        int h = -1;
        int f = -1;
        if(_currentVideoResIndex < current_camera_video_res_count) {
            w = current_camera_video_res[_currentVideoResIndex].width;
            h = current_camera_video_res[_currentVideoResIndex].height;
            f = current_camera_video_res[_currentVideoResIndex].fps;
        }
        _vehicle->sendMavCommand(
            MAV_COMP_ID_CAMERA,                         // Target component
            MAV_CMD_VIDEO_START_CAPTURE,                // Command id
            true,                                       // ShowError
            0,                                          // Camera ID (0 for all cameras), 1 for first, 2 for second, etc.
            f,                                          // FPS: (-1 for max)
            w,                                          // Horizontal resolution in pixels (set to -1 for highest resolution possible)
            h);                                         // Vertical resolution in pixels (set to -1 for highest resolution possible)
        _videoSound.setLoopCount(1);
        _videoSound.play();
    } else {
        _errorSound.setLoopCount(1);
        _errorSound.play();
    }
}

//-----------------------------------------------------------------------------
void
CameraControl::stopVideo()
{
    qCDebug(YuneecCameraLog) << "stopVideo()";
    if(_vehicle && videoStatus() == VIDEO_CAPTURE_STATUS_RUNNING) {
        _vehicle->sendMavCommand(
            MAV_COMP_ID_CAMERA,                         // Target component
            MAV_CMD_VIDEO_STOP_CAPTURE,                 // Command id
            true,                                       // ShowError
            0);                                         // Camera ID (0 for all cameras), 1 for first, 2 for second, etc.
        _videoSound.setLoopCount(2);
        _videoSound.play();
    }
}

//-----------------------------------------------------------------------------
void
CameraControl::setVideoMode()
{
    if(_vehicle && _cameraSupported == CAMERA_SUPPORT_YES && _ambarellaSettings.cam_mode != CAMERA_MODE_VIDEO) {
        qCDebug(YuneecCameraLog) << "setVideoMode()";
        //-- Force UI to update. We keep the real camera mode elsewhere so we
        //   track when the camera actually changed modes, which is quite some
        //   time later.
        _true_cam_mode = _ambarellaSettings.cam_mode;
        _ambarellaSettings.cam_mode = CAMERA_MODE_VIDEO;
        emit cameraModeChanged();
        _vehicle->sendMavCommand(
            MAV_COMP_ID_CAMERA,                         // Target component
            MAV_CMD_SET_CAMERA_MODE,                    // Command id
            true,                                       // ShowError
            0,                                          // Camera ID (0 for all, 1 for first, 2 for second, etc.)
            1,                                          // Camera mode (0: photo, 1: video)
            NAN);                                       // Audio recording enabled (0: off 1: on)
    }
}

//-----------------------------------------------------------------------------
void
CameraControl::setPhotoMode()
{
    if(_vehicle && _cameraSupported == CAMERA_SUPPORT_YES && _ambarellaSettings.cam_mode == CAMERA_MODE_VIDEO) {
        qCDebug(YuneecCameraLog) << "setPhotoMode()";
        //-- Force UI to update. We keep the real camera mode elsewhere so we
        //   track when the camera actually changed modes, which is quite some
        //   time later.
        _true_cam_mode = _ambarellaSettings.cam_mode;
        _ambarellaSettings.cam_mode = CAMERA_MODE_PHOTO;
        emit cameraModeChanged();
        _vehicle->sendMavCommand(
            MAV_COMP_ID_CAMERA,                         // Target component
            MAV_CMD_SET_CAMERA_MODE,                    // Command id
            true,                                       // ShowError
            0,                                          // Camera ID (0 for all, 1 for first, 2 for second, etc.)
            0,                                          // Camera mode (0: photo, 1: video)
            NAN);                                       // Audio recording enabled (0: off 1: on)
    }
}

//-----------------------------------------------------------------------------
void
CameraControl::setCurrentVideoRes(quint32 index)
{
    if(index < current_camera_video_res_count) {
        qCDebug(YuneecCameraLog) << "setCurrentVideoRes:" << current_camera_video_res[index].description;
        _currentVideoResIndex = index;
        emit currentVideoResChanged();
        _updateShutterLimit();
    }
}

//-----------------------------------------------------------------------------
void
CameraControl::setCurrentWB(quint32 index)
{
    if(index < NUM_WB_VALUES && _vehicle && _cameraSupported == CAMERA_SUPPORT_YES) {
        qCDebug(YuneecCameraLog) << "setCurrentWb:" << tr(whiteBalanceOptions[index].description);
        _vehicle->sendMavCommand(
            MAV_COMP_ID_CAMERA,                         // Target component
            MAV_CMD_SET_CAMERA_SETTINGS_1,              // Command id
            true,                                       // ShowError
            1,                                          // Camera ID (1 for first, 2 for second, etc.)
            NAN,                                        // Aperture (1/value) (Fixed for CGO3+)
            NAN,                                        // Shutter speed in seconds
            NAN,                                        // ISO sensitivity
            NAN,                                        // AE mode (Auto Exposure) (0: full auto 1: full manual 2: aperture priority 3: shutter priority)
            NAN,                                        // EV value (when in auto exposure)
            whiteBalanceOptions[index].temperature);    // White balance (color temperature in K) (0: Auto WB)
    }
}

//-----------------------------------------------------------------------------
void
CameraControl::_setIsoShutter(int iso, float shutter)
{
    Q_UNUSED(iso);
    Q_UNUSED(shutter);
    _vehicle->sendMavCommand(
        MAV_COMP_ID_CAMERA,                         // Target component
        MAV_CMD_SET_CAMERA_SETTINGS_1,              // Command id
        true,                                       // ShowError
        1,                                          // Camera ID (1 for first, 2 for second, etc.)
        NAN,                                        // Aperture (1/value) (Fixed for CGO3+)
        shutter,                                    // Shutter speed in seconds
        iso,                                        // ISO sensitivity
        NAN,                                        // AE mode (Auto Exposure) (0: full auto 1: full manual 2: aperture priority 3: shutter priority)
        NAN,                                        // EV value (when in auto exposure)
        NAN);                                       // White balance (color temperature in K) (0: Auto WB)
}

//-----------------------------------------------------------------------------
void
CameraControl::setCurrentIso(quint32 index)
{
    if(_vehicle && index < NUM_ISO_VALUES && _cameraSupported == CAMERA_SUPPORT_YES) {
        qCDebug(YuneecCameraLog) << "setCurrentIso:" << isoValues[index].description;
        _tempIso = index;
        _setIsoShutter(isoValues[index].value, shutterSpeeds[_tempShutter].value);
    }
}

//-----------------------------------------------------------------------------
void
CameraControl::setCurrentShutter(quint32 index)
{
    index += _minShutter;
    if(_vehicle && index < NUM_SHUTTER_VALUES && _cameraSupported == CAMERA_SUPPORT_YES) {
        qCDebug(YuneecCameraLog) << "setCurrentShutter:" << shutterSpeeds[index].description;
        _tempShutter = index;
        _setIsoShutter(isoValues[_tempIso].value, shutterSpeeds[index].value);
    }
}

//-----------------------------------------------------------------------------
void
CameraControl::setCurrentIQ(quint32 index)
{
    if(_vehicle && index < NUM_IQ_MODES && _cameraSupported == CAMERA_SUPPORT_YES) {
        qCDebug(YuneecCameraLog) << "setCurrentIQ:" << tr(iqModeOptions[index].description);
        _vehicle->sendMavCommand(
            MAV_COMP_ID_CAMERA,                         // Target component
            MAV_CMD_SET_CAMERA_SETTINGS_2,              // Command id
            true,                                       // ShowError
            1,                                          // Camera ID (1 for first, 2 for second, etc.)
            NAN,                                        // Reserved for Flicker mode (0 for Auto)
            NAN,                                        // Metering mode ID (Average, Center, Spot, etc.)
            NAN,                                        // Image format ID (Jpeg/Raw/Jpeg+Raw)
            NAN,                                        // Image quality ID (Compression)
            index);                                     // Color mode ID (Neutral, Vivid, etc.)
    }
}

//-----------------------------------------------------------------------------
void
CameraControl::setCurrentPhotoFmt(quint32 index)
{
    qCDebug(YuneecCameraLog) << "setCurrentPhotoFmt:" << current_camera_photo_fmt[index].index << index;
    if(_vehicle && _cameraSupported == CAMERA_SUPPORT_YES && index < current_camera_photo_fmt_count) {
        _vehicle->sendMavCommand(
            MAV_COMP_ID_CAMERA,                         // Target component
            MAV_CMD_SET_CAMERA_SETTINGS_2,              // Command id
            true,                                       // ShowError
            1,                                          // Camera ID (1 for first, 2 for second, etc.)
            NAN,                                        // Reserved for Flicker mode (0 for Auto)
            NAN,                                        // Metering mode ID (Average, Center, Spot, etc.)
            current_camera_photo_fmt[index].index,      // Image format ID (Jpeg/Raw/Jpeg+Raw)
            NAN,                                        // Image quality ID (Compression)
            NAN);                                       // Color mode ID (Neutral, Vivid, etc.)
    }
}

//-----------------------------------------------------------------------------
void
CameraControl::setCurrentMetering(quint32 index)
{
    if(_vehicle && index < NUM_METERING_VALUES && _cameraSupported == CAMERA_SUPPORT_YES) {
        qCDebug(YuneecCameraLog) << "setCurrentMetering:" << tr(meteringModeOptions[index].description);
        _vehicle->sendMavCommand(
            MAV_COMP_ID_CAMERA,                         // Target component
            MAV_CMD_SET_CAMERA_SETTINGS_2,              // Command id
            true,                                       // ShowError
            1,                                          // Camera ID (1 for first, 2 for second, etc.)
            NAN,                                        // Reserved for Flicker mode (0 for Auto)
            meteringModeOptions[index].mode,            // Metering mode ID (Average, Center, Spot, etc.)
            NAN,                                        // Image format ID (Jpeg/Raw/Jpeg+Raw)
            NAN,                                        // Image quality ID (Compression)
            NAN);                                       // Color mode ID (Neutral, Vivid, etc.)
    }
}

//-----------------------------------------------------------------------------
void
CameraControl::setCurrentEV(quint32 index)
{
    //-- TODO: Need to diferentiate between E50 and E90
    if(_vehicle && index < NUM_EV_VALUES && _cameraSupported == CAMERA_SUPPORT_YES) {
        qCDebug(YuneecCameraLog) << "setCurrentEV:" << evOptions[index].description;
        _vehicle->sendMavCommand(
            MAV_COMP_ID_CAMERA,                         // Target component
            MAV_CMD_SET_CAMERA_SETTINGS_1,              // Command id
            true,                                       // ShowError
            1,                                          // Camera ID (1 for first, 2 for second, etc.)
            NAN,                                        // Aperture (1/value) (Fixed for CGO3+)
            NAN,                                        // Shutter speed in seconds
            NAN,                                        // ISO sensitivity
            NAN,                                        // AE mode (Auto Exposure) (0: full auto 1: full manual 2: aperture priority 3: shutter priority)
            evOptions[index].value,                     // EV value (when in auto exposure)
            NAN);                                       // White balance (color temperature in K) (0: Auto WB)
    }
}

//-----------------------------------------------------------------------------
void
CameraControl::setAeMode(AEModes mode)
{
    if(_vehicle && _cameraSupported == CAMERA_SUPPORT_YES) {
        qCDebug(YuneecCameraLog) << "setAeMode:" << mode;
        _vehicle->sendMavCommand(
            MAV_COMP_ID_CAMERA,                         // Target component
            MAV_CMD_SET_CAMERA_SETTINGS_1,              // Command id
            true,                                       // ShowError
            1,                                          // Camera ID (1 for first, 2 for second, etc.)
            NAN,                                        // Aperture (1/value) (Fixed for CGO3+)
            NAN,                                        // Shutter speed in seconds
            NAN,                                        // ISO sensitivity
            mode == AE_MODE_MANUAL ? 1 : 0,             // AE mode (Auto Exposure) (0: full auto 1: full manual 2: aperture priority 3: shutter priority)
            NAN,                                        // EV value (when in auto exposure)
            NAN);                                       // White balance (color temperature in K) (0: Auto WB)
    }
}

//-----------------------------------------------------------------------------
void
CameraControl::formatCard()
{
    if(_vehicle && _cameraSupported == CAMERA_SUPPORT_YES) {
        _vehicle->sendMavCommand(
            MAV_COMP_ID_CAMERA,                         // Target component
            MAV_CMD_STORAGE_FORMAT,                     // Command id
            true,                                       // ShowError
            1,                                          // Storage ID (1 for first, 2 for second, etc.)
            1);                                         // Do Format
    }
}

//-----------------------------------------------------------------------------
void
CameraControl::resetSettings()
{
    if(_vehicle && _cameraSupported == CAMERA_SUPPORT_YES) {
        _vehicle->sendMavCommand(
            MAV_COMP_ID_CAMERA,                         // Target component
            MAV_CMD_RESET_CAMERA_SETTINGS,              // Command id
            true,                                       // ShowError
            0,                                          // Camera ID (0 for all cameras, 1 for first, 2 for second, etc.)
            1);                                         // Do Reset
    }
}

//-----------------------------------------------------------------------------
int CameraControl::_findShutterSpeedIndex(float shutter_speed)
{
    //-- As accuracy, use 1/10 of the smallest shutter possible.
    const float accuracy = (1.0f / 8000.0f) / 10.0f;
    for (uint32_t i = _minShutter; i < NUM_SHUTTER_VALUES; ++i) {
        float diff = fabsf(shutterSpeeds[i].value - shutter_speed);
        if (diff < accuracy) {
            return i;
        }
    }
    return -1;
}

//-----------------------------------------------------------------------------
int
CameraControl::_findVideoResIndex(int w, int h, float fps)
{
    for(uint32_t i = 0; i < current_camera_video_res_count; i++) {
        if(w == current_camera_video_res[i].width && h == current_camera_video_res[i].height && fps == current_camera_video_res[i].fps) {
            return i;
        }
    }
    return -1;
}

//-----------------------------------------------------------------------------
void
CameraControl::_timerHandler()
{
    switch(_currentTask) {
        case MAV_CMD_REQUEST_STORAGE_INFORMATION:
            _requestStorageStatus();
            break;
        case MAV_CMD_REQUEST_CAMERA_CAPTURE_STATUS:
            _requestCaptureStatus();
            break;
        case MAV_CMD_REQUEST_CAMERA_SETTINGS:
            _requestCameraSettings();
            break;
        case MAV_CMD_REQUEST_CAMERA_INFORMATION:
            if(_cameraSupported == CAMERA_SUPPORT_YES) {
                //-- We got an ACK but no response
                if(_camInfoTries++ > 3) {
                    _cameraSupported = CAMERA_SUPPORT_NO;
                    emit cameraAvailableChanged();
                    break;
                }
            }
            _requestCameraInfo();
            _startTimer(MAV_CMD_REQUEST_CAMERA_INFORMATION, 1000);
            break;
    }
}

//-----------------------------------------------------------------------------
// Getting the rec time from the camera is way too expensive because of the
// LCM interface within the camera firmware. Instead, we keep track of the
// timer here.
void
CameraControl::_recTimerHandler()
{
    _ambarellaStatus.record_time = _recTime.elapsed();
    emit recordTimeChanged();
}

//-----------------------------------------------------------------------------
void
CameraControl::_requestCameraSettings()
{
    if(_vehicle) {
        qCDebug(YuneecCameraLog) << "_requestCameraSettings";
        _vehicle->sendMavCommand(
            MAV_COMP_ID_CAMERA,                     // target component
            MAV_CMD_REQUEST_CAMERA_SETTINGS,        // command id
            false,                                  // showError
            0,                                      // Camera ID (0 for all cameras, 1 for first, 2 for second, etc.)
            1);                                     // Do Request
    }
}

//-----------------------------------------------------------------------------
void
CameraControl::_requestCaptureStatus()
{
    if(_vehicle) {
        _vehicle->sendMavCommand(
            MAV_COMP_ID_CAMERA,                     // target component
            MAV_CMD_REQUEST_CAMERA_CAPTURE_STATUS,  // command id
            false,                                  // showError
            0,                                      // Camera ID (0 for all cameras, 1 for first, 2 for second, etc.)
            1);                                     // Do Request
    }
}

//-----------------------------------------------------------------------------
void
CameraControl::_requestStorageStatus()
{
    if(_vehicle) {
        _vehicle->sendMavCommand(
            MAV_COMP_ID_CAMERA,                     // target component
            MAV_CMD_REQUEST_STORAGE_INFORMATION,    // command id
            false,                                  // showError
            0,                                      // Storage ID (0 for all, 1 for first, 2 for second, etc.)
            1);                                     // Do Request
    }
}

//-----------------------------------------------------------------------------
void
CameraControl::_requestCameraInfo()
{
    qCDebug(YuneecCameraLog) << "_requestCameraInfo()";
    if(_vehicle) {
        _vehicle->sendMavCommand(
            MAV_COMP_ID_CAMERA,                     // target component
            MAV_CMD_REQUEST_CAMERA_INFORMATION,     // command id
            false,                                  // showError
            0,                                      // Storage ID (0 for all, 1 for first, 2 for second, etc.)
            1);                                     // Do Request
    }
}

//-----------------------------------------------------------------------------
void
CameraControl::_startTimer(int task, int elapsed)
{
    _currentTask = task;
    _statusTimer.start(elapsed);
}

//-----------------------------------------------------------------------------
void
CameraControl::_handleCommandResult(bool noReponseFromVehicle, int command, int result)
{
    if(!noReponseFromVehicle && result == MAV_RESULT_ACCEPTED) {
        //-- All good
        return;
    }
    if(noReponseFromVehicle) {
        //-- No response. This is not normal. It means we sent it 3 times and it didn't respond
        qCDebug(YuneecCameraLog) << "Retry" << command;
    } else {
        if(result == MAV_RESULT_TEMPORARILY_REJECTED) {
            qCDebug(YuneecCameraLog) << "Camera is too busy for" << command << " Retrying...";
        } if(result != MAV_RESULT_ACCEPTED) {
            qCDebug(YuneecCameraLog) << "Bad response from command" << command << "Result:" << result << "Retrying...";
        }
    }
    //-- Keep trying
    _startTimer(command, 500);
}

//-----------------------------------------------------------------------------
void
CameraControl::_mavCommandResult(int /*vehicleId*/, int /*component*/, int command, int result, bool noReponseFromVehicle)
{
    //-- Do we already know if the firmware supports cameras or not?
    if(_cameraSupported == CAMERA_SUPPORT_UNDEFINED) {
        //-- Is this the response we are waiting?
        if(command == MAV_CMD_REQUEST_CAMERA_INFORMATION) {
            if(noReponseFromVehicle) {
                qCDebug(YuneecCameraLog) << "No response for MAV_CMD_REQUEST_CAMERA_INFORMATION";
                //-- We got no answer so we assume no camera support
                _cameraSupported = CAMERA_SUPPORT_NO;
                emit cameraAvailableChanged();
            } else {
                if(result == MAV_RESULT_ACCEPTED) {
                    //-- We have an answer. Start the show.
                    _cameraSupported = CAMERA_SUPPORT_YES;
                    emit cameraAvailableChanged();
                    //-- Make sure we get an answer
                    _startTimer(MAV_CMD_REQUEST_CAMERA_INFORMATION, 1000);
                } else if(result == MAV_RESULT_TEMPORARILY_REJECTED) {
                    //--Keep Trying
                    _startTimer(MAV_CMD_REQUEST_CAMERA_INFORMATION, 500);
                } else {
                    //-- We got an answer but not a good one
                    _cameraSupported = CAMERA_SUPPORT_NO;
                    emit cameraAvailableChanged();
                }
            }
        }
    } else if(_cameraSupported == CAMERA_SUPPORT_YES) {
        switch(command) {
            case MAV_CMD_REQUEST_STORAGE_INFORMATION:
            case MAV_CMD_REQUEST_CAMERA_SETTINGS:
            case MAV_CMD_REQUEST_CAMERA_CAPTURE_STATUS:
            case MAV_CMD_REQUEST_CAMERA_INFORMATION:
                _handleCommandResult(noReponseFromVehicle, command, result);
                break;
            case MAV_CMD_SET_CAMERA_SETTINGS_1:
            case MAV_CMD_SET_CAMERA_SETTINGS_2:
            case MAV_CMD_RESET_CAMERA_SETTINGS:
            case MAV_CMD_SET_CAMERA_MODE:
            case MAV_CMD_IMAGE_START_CAPTURE:
            case MAV_CMD_VIDEO_START_CAPTURE:
            case MAV_CMD_VIDEO_STOP_CAPTURE:
                if(!noReponseFromVehicle && result == MAV_RESULT_ACCEPTED) {
                    //-- Faster feedback for start/stop video
                    if(command == MAV_CMD_VIDEO_START_CAPTURE) {
                        _handleVideoRunning(VIDEO_CAPTURE_STATUS_RUNNING);
                    } else if(command == MAV_CMD_VIDEO_STOP_CAPTURE) {
                        _handleVideoRunning(VIDEO_CAPTURE_STATUS_STOPPED);
                    }
                    _startTimer(MAV_CMD_REQUEST_CAMERA_SETTINGS, 500);
                } else {
                    //-- The camera didn't take it. There isn't much what we can do.
                    //   Sound an error to let the user know. Whatever setting was
                    //   being changed has not been changed and the UI will reflect
                    //   that. There is no good reason for this to fail. The camera
                    //   firmware needs to be fixed and stop this nonsense of "busy".
                    qCDebug(YuneecCameraLog) << "Bad or no response for" << command;
                    _errorSound.setLoopCount(2);
                    _errorSound.play();
                }
                break;
            default:
                break;
        }
    }
}

//-----------------------------------------------------------------------------
void
CameraControl::_mavlinkMessageReceived(const mavlink_message_t& message)
{
    switch (message.msgid) {
        case MAVLINK_MSG_ID_CAMERA_CAPTURE_STATUS:
            _handleCaptureStatus(message);
            break;
        case MAVLINK_MSG_ID_CAMERA_SETTINGS:
            _handleCameraSettings(message);
            break;
        case MAVLINK_MSG_ID_STORAGE_INFORMATION:
            _handleStorageInfo(message);
            break;
        case MAVLINK_MSG_ID_CAMERA_INFORMATION:
            _handleCameraInfo(message);
            break;
    }
}

//-----------------------------------------------------------------------------
QString
CameraControl::firmwareVersion()
{
    QString ver;
    ver.sprintf("%d.%d.%d %c",
                (_cameraVersion & 0xFF),
                (_cameraVersion >> 8  & 0xFF),
                (_cameraVersion >> 16 & 0xFF),
                (_cameraVersion >> 24 & 0xFF));
    return ver;
}

//-----------------------------------------------------------------------------
void
CameraControl::_handleCameraInfo(const mavlink_message_t& message)
{
    mavlink_camera_information_t info;
    mavlink_msg_camera_information_decode(&message, &info);
    _cameraVersion  = info.firmware_version;
    _cameraVendor   = (const char*)&info.vendor_name[0];
    _cameraModel    = (const char*)&info.model_name[0];
    qCDebug(YuneecCameraLog) << "_handleCameraInfo:" << _cameraVendor << _cameraModel << _cameraVersion << (_cameraVersion >> 24 & 0xFF) << (_cameraVersion >> 16 & 0xFF) << (_cameraVersion >> 8 & 0xFF) << (_cameraVersion & 0xFF);
    //-- The E90 has a different set of options
    if(_cameraModel.startsWith("E90")) {
        current_camera_video_res = &videoResE90[0];
        current_camera_video_res_count = NUM_E90_VIDEO_RES;
        current_camera_photo_fmt = &photoFormatOptionsE90[0];
        current_camera_photo_fmt_count = NUM_E90_PHOTO_FORMAT_VALUES;
    } else {
        current_camera_video_res = &videoResE50[0];
        current_camera_video_res_count = NUM_E50_VIDEO_RES;
        current_camera_photo_fmt = &photoFormatOptionsE50[0];
        current_camera_photo_fmt_count = NUM_E50_PHOTO_FORMAT_VALUES;
    }
    //-- Update options based on camera type
    _videoResList.clear();
    _photoFormatList.clear();
    emit videoResListChanged();
    emit photoFormatListChanged();
    emit firmwareVersionChanged();
    emit cameraModelChanged();
    _startTimer(MAV_CMD_REQUEST_CAMERA_SETTINGS, 500);
}

//-----------------------------------------------------------------------------
void
CameraControl::_handleCameraSettings(const mavlink_message_t& message)
{
    mavlink_camera_settings_t settings;
    mavlink_msg_camera_settings_decode(&message, &settings);
    qCDebug(YuneecCameraLog) << "_handleCameraSettings:" << settings.mode_id << settings.color_mode_id << settings.ev << settings.exposure_mode << settings.image_format_id << settings.iso_sensitivity << settings.metering_mode_id << settings.shutter_speed;
    //-- Auto Exposure Mode
    int ae = settings.exposure_mode == 0 ? AE_MODE_AUTO : AE_MODE_MANUAL;
    if(_ambarellaSettings.ae_enable != ae) {
        _ambarellaSettings.ae_enable = ae;
        emit aeModeChanged();
    }
    //-- Shutter Speed
    int idx = _findShutterSpeedIndex(settings.shutter_speed);
    if(idx >= 0) {
        _tempShutter = idx;
        _currentShutter = idx;
        emit currentShutterChanged();
    }
    //-- ISO Value
    for(uint32_t i = 0; i < NUM_ISO_VALUES; i++) {
        if(isoValues[i].value == (int)settings.iso_sensitivity) {
            if(_currentIso != i) {
                _currentIso = i;
                emit currentIsoChanged();
                break;
            }
        }
    }
    //-- EV
    if(_ambarellaSettings.exposure_value != settings.ev) {
        //-- TODO: Need to diferentiate between E50 and E90
        uint32_t idx = 100000;
        for(uint32_t i = 0; i < NUM_EV_VALUES; i++) {
            if(settings.ev == evOptions[i].value) {
                idx = i;
                break;
            }
        }
        if(idx < NUM_EV_VALUES) {
            _currentEV = idx;
            emit currentEVChanged();
        }
    }
    //-- White Balance
    quint32 wb = (quint32)settings.white_balance;
    if(_ambarellaSettings.white_balance != wb) {
        _ambarellaSettings.white_balance = wb;
        for(uint32_t i = 0; i < NUM_WB_VALUES; i++) {
            if(whiteBalanceOptions[i].temperature == wb) {
                _currentWB = i;
                break;
            }
        }
        emit currentWBChanged();
    }
    //-- Camera Mode
    int cam_mode = CAMERA_MODE_UNDEFINED;
    if(settings.mode_id == 0)
        cam_mode = CAMERA_MODE_PHOTO;
    else if(settings.mode_id == 1)
        cam_mode = CAMERA_MODE_VIDEO;
    //-- Camera mode switch takes too long so we switch the UI right
    //   after the user presses the switch. Internally however, we only
    //   truly find out the mode once we get an answer from the camera.
    if(_true_cam_mode != cam_mode) {
        _true_cam_mode = cam_mode;
        _ambarellaSettings.cam_mode = cam_mode;
        emit cameraModeChanged();
        _updateShutterLimit();
        _updateAspectRatio();
    }
    //-- Audio Enabled
    bool record_enabled = settings.audio_recording == 1;
    if(_ambarellaSettings.audio_switch != record_enabled) {
        _ambarellaSettings.audio_switch = record_enabled;
        //-- TODO
    }
    //-- Color IQ
    if(_ambarellaSettings.iq_type != settings.color_mode_id && settings.color_mode_id < NUM_IQ_MODES) {
        _ambarellaSettings.iq_type = settings.color_mode_id;
        emit currentIQChanged();
    }
    //-- Photo Format
    uint32_t fmt_idx = 0;
    for(uint32_t i = 0; i < current_camera_photo_fmt_count; i++) {
        if(current_camera_photo_fmt[i].index == settings.image_format_id) {
            fmt_idx = i;
            break;
        }
    }
    if(_ambarellaSettings.photo_format != fmt_idx) {
        _ambarellaSettings.photo_format = fmt_idx;
        emit currentPhotoFmtChanged();
    }
    //-- Image Quality (Compression)
    if(_ambarellaSettings.photo_quality != settings.image_quality_id) {
        _ambarellaSettings.photo_quality = settings.image_quality_id;
        //-- TODO
    }
    //-- Metering
    if(_ambarellaSettings.metering_mode != settings.metering_mode_id && settings.metering_mode_id < NUM_METERING_VALUES) {
        _ambarellaSettings.metering_mode = settings.metering_mode_id;
        emit currentMeteringChanged();
    }
    //-- Get Storage Setting next
    _startTimer(MAV_CMD_REQUEST_STORAGE_INFORMATION, 500);
}

//-----------------------------------------------------------------------------
void
CameraControl::_handleVideoRunning(VideoStatus status)
{
    if(_ambarellaStatus.video_status != status) {
        _ambarellaStatus.video_status = status;
        emit videoStatusChanged();
        if(status == VIDEO_CAPTURE_STATUS_RUNNING) {
            _recTime.start();
            _recTimer.start();
            //-- Start recording local stream as well
            if(qgcApp()->toolbox()->videoManager()->videoReceiver()) {
                qgcApp()->toolbox()->videoManager()->videoReceiver()->startRecording();
            }
        } else {
            _recTimer.stop();
            _ambarellaStatus.record_time = 0;
            emit recordTimeChanged();
            //-- Stop recording local stream
            if(qgcApp()->toolbox()->videoManager()->videoReceiver()) {
                qgcApp()->toolbox()->videoManager()->videoReceiver()->stopRecording();
            }
        }
    }
}

//-----------------------------------------------------------------------------
void
CameraControl::_handleCaptureStatus(const mavlink_message_t &message)
{
    //-- This is a response to MAV_CMD_REQUEST_CAMERA_CAPTURE_STATUS
    mavlink_camera_capture_status_t cap;
    mavlink_msg_camera_capture_status_decode(&message, &cap);
    qCDebug(YuneecCameraLog) << "_handleCaptureStatus:" << cap.available_capacity << cap.image_interval << cap.image_resolution_h << cap.image_resolution_v << cap.image_status << cap.recording_time_ms << cap.video_framerate << cap.video_resolution_h << cap.video_resolution_v << cap.video_status;
    //-- Disk Free Space
    if(_ambarellaStatus.sdfree != cap.available_capacity) {
        _ambarellaStatus.sdfree = cap.available_capacity;
        emit sdFreeChanged();
    }
    //-- Image Capture Status
    if(_ambarellaStatus.image_status != cap.image_status) {
        _ambarellaStatus.image_status = cap.image_status;
        emit photoStatusChanged();
    }
    //-- Video Capture Status
    _handleVideoRunning((VideoStatus)cap.video_status);
    //-- Video res is only valid when video started recording
    if(_ambarellaStatus.video_status == VIDEO_CAPTURE_STATUS_RUNNING) {
        //-- Current Video Resolution and FPS
        int idx = _findVideoResIndex(cap.video_resolution_h, cap.video_resolution_v, cap.video_framerate);
        if((int)_currentVideoResIndex != idx && idx >= 0) {
            _currentVideoResIndex = idx;
            emit currentVideoResChanged();
            _updateAspectRatio();
            _updateShutterLimit();
        }
    }
    /*
    //-- Recording running time
    if(_ambarellaStatus.record_time != cap.recording_time_ms) {
        _ambarellaStatus.record_time = cap.recording_time_ms;
        emit recordTimeChanged();
    }
    */
    //-- More often than once every 5 seconds makes the camera barf
    _startTimer(MAV_CMD_REQUEST_CAMERA_CAPTURE_STATUS, 5000);
}

//-----------------------------------------------------------------------------
void
CameraControl::_handleStorageInfo(const mavlink_message_t& message)
{
    mavlink_storage_information_t st;
    mavlink_msg_storage_information_decode(&message, &st);
    qCDebug(YuneecCameraLog) << "_handleStorageInfo:" << st.available_capacity << st.status << st.storage_count << st.storage_id << st.total_capacity << st.used_capacity;
    if(_ambarellaStatus.sdtotal != st.total_capacity) {
        _ambarellaStatus.sdtotal = st.total_capacity;
        emit sdTotalChanged();
    }
    if(_ambarellaStatus.sdfree != st.available_capacity) {
        _ambarellaStatus.sdfree = st.available_capacity;
        emit sdFreeChanged();
    }
    //-- Get Capture Status next
    _startTimer(MAV_CMD_REQUEST_CAMERA_CAPTURE_STATUS, 500);
}

//-----------------------------------------------------------------------------
CameraControl::VideoStatus
CameraControl::videoStatus()
{
    if(_cameraSupported == CAMERA_SUPPORT_YES) {
        return (VideoStatus)_ambarellaStatus.video_status;
    }
    return VIDEO_CAPTURE_STATUS_UNDEFINED;
}

//-----------------------------------------------------------------------------
CameraControl::PhotoStatus
CameraControl::photoStatus()
{
    if(_cameraSupported == CAMERA_SUPPORT_YES) {
        return (PhotoStatus)_ambarellaStatus.image_status;
    }
    return PHOTO_CAPTURE_STATUS_UNDEFINED;
}

//-----------------------------------------------------------------------------
CameraControl::CameraMode
CameraControl::cameraMode()
{
    if(_cameraSupported == CAMERA_SUPPORT_YES) {
        return (CameraMode)_ambarellaSettings.cam_mode;
    }
    return CAMERA_MODE_UNDEFINED;
}

//-----------------------------------------------------------------------------
CameraControl::AEModes
CameraControl::aeMode()
{
    if(_cameraSupported == CAMERA_SUPPORT_YES) {
        return (AEModes)_ambarellaSettings.ae_enable;
    }
    return AE_MODE_UNDEFINED;
}

//-----------------------------------------------------------------------------
QStringList
CameraControl::videoResList()
{

    if(_videoResList.size() == 0) {
        for(size_t i = 0; i < current_camera_video_res_count; i++) {
            _videoResList.append(current_camera_video_res[i].description);
        }
    }
    return _videoResList;
}

//-----------------------------------------------------------------------------
QStringList
CameraControl::iqModeList()
{

    if(_iqModeList.size() == 0) {
        for(size_t i = 0; i < NUM_IQ_MODES; i++) {
            _iqModeList.append(tr(iqModeOptions[i].description));
        }
    }
    return _iqModeList;
}

//-----------------------------------------------------------------------------
QStringList
CameraControl::wbList()
{
    if(_wbList.size() == 0) {
        for(size_t i = 0; i < NUM_WB_VALUES; i++) {
            _wbList.append(tr(whiteBalanceOptions[i].description));
        }
    }
    return _wbList;
}

//-----------------------------------------------------------------------------
QStringList
CameraControl::isoList()
{
    if(_isoList.size() == 0) {
        for(size_t i = 0; i < NUM_ISO_VALUES; i++) {
            _isoList.append(isoValues[i].description);
        }
    }
    return _isoList;
}

//-----------------------------------------------------------------------------
QStringList
CameraControl::shutterList()
{
    if(_shutterList.size() == 0) {
        for(size_t i = 0; i < NUM_SHUTTER_VALUES; i++) {
            _shutterList.append(shutterSpeeds[i].description);
        }
    }
    return _shutterList;
}

//-----------------------------------------------------------------------------
QStringList
CameraControl::meteringList()
{
    if(_meteringList.size() == 0) {
        for(size_t i = 0; i < NUM_METERING_VALUES; i++) {
            _meteringList.append(tr(meteringModeOptions[i].description));
        }
    }
    return _meteringList;
}

//-----------------------------------------------------------------------------
QStringList
CameraControl::photoFormatList()
{
    if(_photoFormatList.size() == 0) {
        for(size_t i = 0; i < current_camera_photo_fmt_count; i++) {
            _photoFormatList.append(current_camera_photo_fmt[i].description);
        }
    }
    return _photoFormatList;
}

//-----------------------------------------------------------------------------
QStringList
CameraControl::evList()
{
    if(_evList.size() == 0) {
        for(uint32_t i = 0; i < NUM_EV_VALUES; i++) {
            _evList.append(evOptions[i].description);
        }
    }
    return _evList;
}

//-----------------------------------------------------------------------------
void
CameraControl::setCameraMode(CameraMode mode)
{
    if(mode == CAMERA_MODE_VIDEO) {
        setVideoMode();
    } else if(mode == CAMERA_MODE_PHOTO) {
        setPhotoMode();
    }
}

//-----------------------------------------------------------------------------
void
CameraControl::toggleMode()
{
    if(cameraMode() == CAMERA_MODE_PHOTO) {
        setVideoMode();
    } else if(cameraMode() == CAMERA_MODE_VIDEO) {
        setPhotoMode();
    }
}

//-----------------------------------------------------------------------------
void
CameraControl::toggleVideo()
{
    if(videoStatus() == VIDEO_CAPTURE_STATUS_STOPPED) {
        startVideo();
    } else if(videoStatus() == VIDEO_CAPTURE_STATUS_RUNNING) {
        stopVideo();
    }
}

//-----------------------------------------------------------------------------
quint32
CameraControl::recordTime()
{
    if(_cameraSupported == CAMERA_SUPPORT_YES) {
        return _ambarellaStatus.record_time;
    }
    return 0;
}

//-----------------------------------------------------------------------------
QString
CameraControl::recordTimeStr()
{
    return QTime(0, 0).addMSecs(recordTime()).toString("hh:mm:ss");
}

//-----------------------------------------------------------------------------
void
CameraControl::_updateAspectRatio()
{
    //-- E90 is always 720P regardless
    float aspect = 1280.0f / 720.0f;
    if(!_cameraModel.startsWith("E90")) {
        //-- Photo Mode
        if(_ambarellaSettings.cam_mode == CAMERA_MODE_PHOTO) {
            //-- CGO3+ and E50 are 4x3 in Photo Mode
            aspect = 4.0f / 3.0f;
            qCDebug(YuneecCameraLog) << "Set Photo Aspect Ratio" << aspect;
        //-- Video Mode
        } else if(_ambarellaSettings.cam_mode == CAMERA_MODE_VIDEO) {
            qCDebug(YuneecCameraLog) << "Set Video Aspect Ratio" << aspect;
        }
    }
    qgcApp()->toolbox()->settingsManager()->videoSettings()->aspectRatio()->setRawValue(aspect);
}

//-----------------------------------------------------------------------------
void
CameraControl::_updateShutterLimit()
{
    float cur_shutter = shutterSpeeds[_tempShutter].value;
    _minShutter = 0;
    if(_ambarellaSettings.cam_mode == CAMERA_MODE_VIDEO) {
        //-- Minimum shutter cannot be slower than frame rate
        float curFps = 1.01f / (float)(current_camera_video_res[_currentVideoResIndex].fps);
        for(uint32_t i = 0; i < NUM_SHUTTER_VALUES; i++) {
            if(curFps > shutterSpeeds[i].value) {
                _minShutter = i;
                break;
            }
        }
    }
    //-- Adjust list of shutter speed options
    _shutterList.clear();
    for(uint32_t i = _minShutter; i < NUM_SHUTTER_VALUES; i++) {
        _shutterList.append(shutterSpeeds[i].description);
    }
    emit shutterListChanged();
    if(_currentShutter < _minShutter) {
        _currentShutter = _minShutter;
        _tempShutter    = _minShutter;
        emit currentShutterChanged();
    } else {
        int idx = _findShutterSpeedIndex(cur_shutter);
        if(idx >= 0) {
            _currentShutter = idx;
            _tempShutter    = idx;
            emit currentShutterChanged();
        }
    }
}

//-----------------------------------------------------------------------------
void
CameraControl::_resetCameraValues()
{
    _true_cam_mode = CAMERA_MODE_UNDEFINED;

    _ambarellaSettings.ae_enable        = AE_MODE_UNDEFINED;
    _ambarellaSettings.exposure_value   = 1000.0f;
    _ambarellaSettings.cam_mode         = CAMERA_MODE_UNDEFINED;
    _ambarellaSettings.audio_switch     = false;
    _ambarellaSettings.iq_type          = 1000;
    _ambarellaSettings.photo_format     = 1000;
    _ambarellaSettings.photo_quality    = 1000;
    _ambarellaSettings.white_balance    = 1000;
    _ambarellaSettings.metering_mode    = 1000;

    _ambarellaStatus.image_status       = PHOTO_CAPTURE_STATUS_UNDEFINED;
    _ambarellaStatus.video_status       = VIDEO_CAPTURE_STATUS_UNDEFINED;
    _ambarellaStatus.sdfree             = 0xFFFFFFFF;
    _ambarellaStatus.sdtotal            = 0xFFFFFFFF;
    _ambarellaStatus.record_time        = 0;
}
