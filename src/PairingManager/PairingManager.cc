/****************************************************************************
 *
 *   (c) 2019 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "PairingManager.h"
#include "SettingsManager.h"
#include "MicrohardManager.h"
#include "QGCApplication.h"
#include "QGCCorePlugin.h"

#include <QSettings>
#include <QJsonObject>
#include <QStandardPaths>
#include <QMutexLocker>

QGC_LOGGING_CATEGORY(PairingManagerLog, "PairingManagerLog")

static const char* jsonFileName = "pairing.json";

//-----------------------------------------------------------------------------
static QString
random_string(uint length)
{
    auto randchar = []() -> char
    {
        const char charset[] =
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";
        const uint max_index = (sizeof(charset) - 1);
        return charset[static_cast<uint>(rand()) % max_index];
    };
    std::string str(length, 0);
    std::generate_n(str.begin(), length, randchar);
    return QString::fromStdString(str);
}

//-----------------------------------------------------------------------------
PairingManager::PairingManager(QGCApplication* app, QGCToolbox* toolbox)
    : QGCTool(app, toolbox)
    , _aes("J6+KuWh9K2!hG(F'", 0x368de30e8ec063ce)
{
    _jsonFileName = QDir::temp().filePath(jsonFileName);
    connect(this, &PairingManager::parsePairingJson, this, &PairingManager::_parsePairingJson);
    connect(this, &PairingManager::setPairingStatus, this, &PairingManager::_setPairingStatus);
    connect(this, &PairingManager::startUpload, this, &PairingManager::_startUpload);
}

//-----------------------------------------------------------------------------
PairingManager::~PairingManager()
{
}

//-----------------------------------------------------------------------------

void
PairingManager::setToolbox(QGCToolbox *toolbox)
{
    QGCTool::setToolbox(toolbox);
    _updatePairedDeviceNameList();
    emit pairedListChanged();
}

//-----------------------------------------------------------------------------
void
PairingManager::_pairingCompleted(QString name, QString connectionKey)
{
    QJsonObject jsonObj = _jsonDoc.object();
    jsonObj.insert("EK", connectionKey);
    _jsonDoc.setObject(jsonObj);
    _writeJson(_jsonDoc, _pairingCacheFile(name));
    _remotePairingMap["NM"] = name;
    _lastPaired = name;
    _updatePairedDeviceNameList();
    emit pairedListChanged();
    emit pairedVehicleChanged();
    //_app->informationMessageBoxOnMainThread("", tr("Paired with %1").arg(name));
    setPairingStatus(PairingSuccess, tr("Pairing Successfull"));
    _toolbox->microhardManager()->switchToConnectionEncryptionKey(connectionKey);
}

//-----------------------------------------------------------------------------
void
PairingManager::_connectionCompleted(QString /*name*/)
{
    _app->informationMessageBoxOnMainThread("", tr("Connected to %1").arg(name));
    setPairingStatus(PairingConnected, tr("Connection Successfull"));
}

//-----------------------------------------------------------------------------
void
PairingManager::_startUpload(QString pairURL, QJsonDocument jsonDoc)
{
    QMutexLocker lock(&_uploadMutex);
    if (_uploadManager != nullptr) {
        return;
    }
    _uploadManager = new QNetworkAccessManager(this);

    QString str = jsonDoc.toJson(QJsonDocument::JsonFormat::Compact);
    qCDebug(PairingManagerLog) << "Starting upload to: " << pairURL << " " << str;
    _uploadData = QString::fromStdString(_aes.encrypt(str.toStdString()));
    _uploadURL = pairURL;
    _startUploadRequest();
}

//-----------------------------------------------------------------------------
void
PairingManager::_startUploadRequest()
{
    QNetworkRequest req;
    req.setUrl(QUrl(_uploadURL));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    QNetworkReply *reply = _uploadManager->post(req, _uploadData.toUtf8());
    connect(reply, &QNetworkReply::finished, this, &PairingManager::_uploadFinished);
}

//-----------------------------------------------------------------------------
void
PairingManager::_stopUpload()
{
    QMutexLocker lock(&_uploadMutex);
    if (_uploadManager != nullptr) {
        delete _uploadManager;
        _uploadManager = nullptr;
    }
}

//-----------------------------------------------------------------------------
void
PairingManager::_uploadFinished()
{
    QMutexLocker lock(&_uploadMutex);
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(QObject::sender());
    if (reply) {
        if (_uploadManager != nullptr) {
            if (reply->error() == QNetworkReply::NoError) {
                _uploadManager->deleteLater();
                _uploadManager = nullptr;
                _uploadMutex.unlock();
                qCDebug(PairingManagerLog) << "Upload finished.";
                QByteArray bytes = reply->readAll();
                QString str = QString::fromUtf8(bytes.data(), bytes.size());
                qCDebug(PairingManagerLog) << "Reply: " << str;
                auto a = str.split(QRegExp("\\s+"));
                if (a[0] == "Accepted" && a.length() > 2) {
                    _pairingCompleted(a[1], a[2]);
                } else if (a[0] == "Connected" && a.length() > 1) {
                    _connectionCompleted(a[1]);
                } else if (a[0] == "Connection" && a.length() > 1) {
                    setPairingStatus(PairingConnectionRejected, tr("Connection Rejected"));
                    qCDebug(PairingManagerLog) << "Connection error: " << str;
                } else {
                    setPairingStatus(PairingRejected, tr("Pairing Rejected"));
                    qCDebug(PairingManagerLog) << "Pairing error: " << str;
                }
            } else {
                if(++_pairRetryCount > 3) {
                    qCDebug(PairingManagerLog) << "Giving up";
                    setPairingStatus(PairingError, tr("No Response From Vehicle"));
                    _uploadManager->deleteLater();
                    _uploadManager = nullptr;
                } else {
                    qCDebug(PairingManagerLog) << "Upload error: " + reply->errorString();
                    _startUploadRequest();
                }
            }
        }
    }
}

//-----------------------------------------------------------------------------
void
PairingManager::_parsePairingJsonFile()
{
    QFile file(_jsonFileName);
    file.open(QIODevice::ReadOnly | QIODevice::Text);
    QString json = file.readAll();
    file.remove();
    file.close();

    jsonReceived(json);
}

//-----------------------------------------------------------------------------
void
PairingManager::connectToPairedDevice(QString name)
{
    setPairingStatus(PairingConnecting, tr("Connecting to %1").arg(name));
    QFile file(_pairingCacheFile(name));
    file.open(QIODevice::ReadOnly | QIODevice::Text);
    QString json = file.readAll();
    jsonReceived(json);
}

//-----------------------------------------------------------------------------
void
PairingManager::removePairedDevice(QString name)
{
    QFile file(_pairingCacheFile(name));
    file.remove();
    _updatePairedDeviceNameList();
    emit pairedListChanged();
}

//-----------------------------------------------------------------------------
void
PairingManager::_updatePairedDeviceNameList()
{
    _deviceList.clear();
    QDirIterator it(_pairingCacheDir().absolutePath(), QDir::Files);
    while (it.hasNext()) {
        QFileInfo fileInfo(it.next());
        _deviceList.append(fileInfo.fileName());
        qCDebug(PairingManagerLog) << "Listing: " << fileInfo.fileName();
    }
}

//-----------------------------------------------------------------------------
QString
PairingManager::_assumeMicrohardPairingJson()
{
    QJsonDocument json;
    QJsonObject   jsonObject;

    jsonObject.insert("LT", "MH");

    QString remoteIPAddr = _toolbox->microhardManager()->remoteIPAddr();
    QString ipPrefix = remoteIPAddr.left(remoteIPAddr.lastIndexOf('.'));
    jsonObject.insert("IP", ipPrefix + ".10");
    jsonObject.insert("AIP", remoteIPAddr);
    jsonObject.insert("CU",  _toolbox->microhardManager()->configUserName());
    jsonObject.insert("CP",  _toolbox->microhardManager()->configPassword());
    jsonObject.insert("EK",  _toolbox->microhardManager()->encryptionKey());
    json.setObject(jsonObject);

    return QString(json.toJson(QJsonDocument::Compact));
}

//-----------------------------------------------------------------------------
void
PairingManager::_parsePairingJson(QString jsonEnc)
{
    QString json = QString::fromStdString(_aes.decrypt(jsonEnc.toStdString()));
    if (json == "") {
        json = jsonEnc;
    }
    qCDebug(PairingManagerLog) << "Parsing JSON: " << json;

    _jsonDoc = QJsonDocument::fromJson(json.toUtf8());

    if (_jsonDoc.isNull()) {
        setPairingStatus(PairingError, tr("Invalid Pairing File"));
        qCDebug(PairingManagerLog) << "Failed to create Pairing JSON doc.";
        return;
    }
    if (!_jsonDoc.isObject()) {
        setPairingStatus(PairingError, tr("Error Parsing Pairing File"));
        qCDebug(PairingManagerLog) << "Pairing JSON is not an object.";
        return;
    }

    QJsonObject jsonObj = _jsonDoc.object();

    if (jsonObj.isEmpty()) {
        setPairingStatus(PairingError, tr("Error Parsing Pairing File"));
        qCDebug(PairingManagerLog) << "Pairing JSON object is empty.";
        return;
    }

    _remotePairingMap = jsonObj.toVariantMap();
    QString linkType  = _remotePairingMap["LT"].toString();
    QString pport     = _remotePairingMap["PP"].toString();
    if (pport.length()==0) {
        pport = "29351";
    }

    if (linkType.length()==0) {
        setPairingStatus(PairingError, tr("Error Parsing Pairing File"));
        qCDebug(PairingManagerLog) << "Pairing JSON is malformed.";
        return;
    }

    QString pairURL = "http://" + _remotePairingMap["IP"].toString() + ":" + pport;
    bool connecting = jsonObj.contains("CONNECTING");
    QJsonDocument jsonDoc;

    if (!connecting) {
        pairURL +=  + "/pair";
        jsonObj.insert("CONNECTING", "true");
        _jsonDoc.setObject(jsonObj);
        if (linkType == "ZT") {
            jsonDoc = _createZeroTierPairingJson();
        } else if (linkType == "MH") {
            jsonDoc = _createMicrohardPairingJson();
        }
    } else {
        pairURL +=  + "/connect";
        if (linkType == "ZT") {
            jsonDoc = _createZeroTierConnectJson();
        } else if (linkType == "MH") {
            jsonDoc = _createMicrohardConnectJson();
        }
    }

    if (linkType == "ZT") {
        _toolbox->settingsManager()->appSettings()->enableMicrohard()->setRawValue(false);
        _toolbox->settingsManager()->appSettings()->enableTaisync()->setRawValue(false);
        emit startUpload(pairURL, jsonDoc);
    } else if (linkType == "MH") {
        _toolbox->settingsManager()->appSettings()->enableMicrohard()->setRawValue(true);
        _toolbox->settingsManager()->appSettings()->enableTaisync()->setRawValue(false);
        if (_remotePairingMap.contains("AIP")) {
            _toolbox->microhardManager()->setRemoteIPAddr(_remotePairingMap["AIP"].toString());
        }
        if (_remotePairingMap.contains("CU")) {
            _toolbox->microhardManager()->setConfigUserName(_remotePairingMap["CU"].toString());
        }
        if (_remotePairingMap.contains("CP")) {
            _toolbox->microhardManager()->setConfigPassword(_remotePairingMap["CP"].toString());
        }
        if (!connecting) {
            _toolbox->microhardManager()->switchToPairingEncryptionKey();
        } else if (_remotePairingMap.contains("EK")) {
            _toolbox->microhardManager()->switchToConnectionEncryptionKey(_remotePairingMap["EK"].toString());
        }

        _toolbox->microhardManager()->updateSettings();
        emit startUpload(pairURL, jsonDoc);
    }
}

//-----------------------------------------------------------------------------
QString
PairingManager::_getLocalIPInNetwork(QString remoteIP, int num)
{
    QStringList pieces = remoteIP.split(".");
    QString ipPrefix = "";
    for (int i = 0; i<num && i<pieces.length(); i++) {
        ipPrefix += pieces[i] + ".";
    }

    const QHostAddress &localhost = QHostAddress(QHostAddress::LocalHost);
    for (const QHostAddress &address: QNetworkInterface::allAddresses()) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol && address != localhost) {
            if (address.toString().startsWith(ipPrefix)) {
                return address.toString();
            }
        }
    }

    return "";
}

//-----------------------------------------------------------------------------
QDir
PairingManager::_pairingCacheDir()
{
    const QString spath(QFileInfo(QSettings().fileName()).dir().absolutePath());
    QDir dir = spath + QDir::separator() + "PairingCache";
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    return dir;
}

//-----------------------------------------------------------------------------
QString
PairingManager::_pairingCacheFile(QString uavName)
{
    return _pairingCacheDir().filePath(uavName);
}

//-----------------------------------------------------------------------------
void
PairingManager::_writeJson(QJsonDocument &jsonDoc, QString fileName)
{
    QString val = jsonDoc.toJson(QJsonDocument::JsonFormat::Compact);
    qCDebug(PairingManagerLog) << "Write json " << val;
    QString enc = QString::fromStdString(_aes.encrypt(val.toStdString()));

    QFile file(fileName);
    file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate);
    file.write(enc.toUtf8());
    file.close();
}

//-----------------------------------------------------------------------------
QJsonDocument
PairingManager::_createZeroTierPairingJson()
{
    QString localIP = _getLocalIPInNetwork(_remotePairingMap["IP"].toString(), 2);

    QJsonObject jsonObj;
    jsonObj.insert("LT", "ZT");
    jsonObj.insert("IP", localIP);
    jsonObj.insert("P", 14550);
    return QJsonDocument(jsonObj);
}

//-----------------------------------------------------------------------------
QJsonDocument
PairingManager::_createMicrohardPairingJson()
{
    QString localIP = _getLocalIPInNetwork(_remotePairingMap["IP"].toString(), 3);

    QJsonObject jsonObj;
    jsonObj.insert("LT", "MH");
    jsonObj.insert("IP", localIP);
    jsonObj.insert("P", 14550);
    return QJsonDocument(jsonObj);
}

//-----------------------------------------------------------------------------
QJsonDocument
PairingManager::_createZeroTierConnectJson()
{
    QString localIP = _getLocalIPInNetwork(_remotePairingMap["IP"].toString(), 2);

    QJsonObject jsonObj;
    jsonObj.insert("LT", "ZT");
    jsonObj.insert("IP", localIP);
    jsonObj.insert("P", 14550);
    return QJsonDocument(jsonObj);
}

//-----------------------------------------------------------------------------
QJsonDocument
PairingManager::_createMicrohardConnectJson()
{
    QString localIP = _getLocalIPInNetwork(_remotePairingMap["IP"].toString(), 3);

    QJsonObject jsonObj;
    jsonObj.insert("LT", "MH");
    jsonObj.insert("IP", localIP);
    jsonObj.insert("P", 14550);
    return QJsonDocument(jsonObj);
}

//-----------------------------------------------------------------------------

QStringList
PairingManager::pairingLinkTypeStrings()
{
    //-- Must follow same order as enum LinkType in LinkConfiguration.h
    static QStringList list;
    int i = 0;
    if (!list.size()) {
#if defined QGC_ENABLE_NFC || defined QGC_ENABLE_QTNFC
        list += tr("NFC");
        _nfcIndex = i++;
#endif
#if defined QGC_GST_MICROHARD_ENABLED
        list += tr("Microhard");
        _microhardIndex = i++;
#endif
    }
    return list;
}

//-----------------------------------------------------------------------------
void
PairingManager::_setPairingStatus(PairingStatus status, QString statusStr)
{
    _status = status;
    _statusString = statusStr;
    emit pairingStatusChanged();
}

//-----------------------------------------------------------------------------
QString
PairingManager::pairingStatusStr() const
{
    return _statusString;
}

#if QGC_GST_MICROHARD_ENABLED
//-----------------------------------------------------------------------------
void
PairingManager::startMicrohardPairing()
{
    stopPairing();
    _pairRetryCount = 0;
    setPairingStatus(PairingActive, tr("Pairing..."));
    _parsePairingJson(_assumeMicrohardPairingJson());
}
#endif

//-----------------------------------------------------------------------------
void
PairingManager::stopPairing()
{
#if defined QGC_ENABLE_NFC || defined QGC_ENABLE_QTNFC
    pairingNFC.stop();
#endif
    _stopUpload();
    setPairingStatus(PairingIdle, "");
}

#if defined QGC_ENABLE_NFC || defined QGC_ENABLE_QTNFC
//-----------------------------------------------------------------------------
void
PairingManager::startNFCScan()
{
    stopPairing();
    setPairingStatus(PairingActive, tr("Pairing..."));
    pairingNFC.start();
}

#endif

//-----------------------------------------------------------------------------
#ifdef __android__
static const char kJniClassName[] {"org/mavlink/qgroundcontrol/QGCActivity"};

//-----------------------------------------------------------------------------
static void jniNFCTagReceived(JNIEnv *envA, jobject thizA, jstring messageA)
{
    Q_UNUSED(thizA);

    const char *stringL = envA->GetStringUTFChars(messageA, nullptr);
    QString ndef = QString::fromUtf8(stringL);
    envA->ReleaseStringUTFChars(messageA, stringL);
    if (envA->ExceptionCheck())
        envA->ExceptionClear();
    qCDebug(PairingManagerLog) << "NDEF Tag Received: " << ndef;
    qgcApp()->toolbox()->pairingManager()->jsonReceived(ndef);
}

//-----------------------------------------------------------------------------
void PairingManager::setNativeMethods(void)
{
    //  REGISTER THE C++ FUNCTION WITH JNI
    JNINativeMethod javaMethods[] {
        {"nativeNFCTagReceived", "(Ljava/lang/String;)V", reinterpret_cast<void *>(jniNFCTagReceived)}
    };

    QAndroidJniEnvironment jniEnv;
    if (jniEnv->ExceptionCheck()) {
        jniEnv->ExceptionDescribe();
        jniEnv->ExceptionClear();
    }

    jclass objectClass = jniEnv->FindClass(kJniClassName);
    if(!objectClass) {
        qWarning() << "Couldn't find class:" << kJniClassName;
        return;
    }

    jint val = jniEnv->RegisterNatives(objectClass, javaMethods, sizeof(javaMethods) / sizeof(javaMethods[0]));

    if (val < 0) {
        qWarning() << "Error registering methods: " << val;
    } else {
        qCDebug(PairingManagerLog) << "Native Functions Registered";
    }

    if (jniEnv->ExceptionCheck()) {
        jniEnv->ExceptionDescribe();
        jniEnv->ExceptionClear();
    }
}
#endif
//-----------------------------------------------------------------------------
