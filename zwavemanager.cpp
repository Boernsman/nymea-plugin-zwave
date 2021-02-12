/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2013 - 2020, nymea GmbH
* Contact: contact@nymea.io
*
* This file is part of nymea.
* This project including source code and documentation is protected by
* copyright law, and remains the property of nymea GmbH. All rights, including
* reproduction, publication, editing and translation, are reserved. The use of
* this project is subject to the terms of a license agreement to be concluded
* with nymea GmbH in accordance with the terms of use of nymea GmbH, available
* under https://nymea.io/license
*
* GNU Lesser General Public License Usage
* Alternatively, this project may be redistributed and/or modified under the
* terms of the GNU Lesser General Public License as published by the Free
* Software Foundation; version 3. This project is distributed in the hope that
* it will be useful, but WITHOUT ANY WARRANTY; without even the implied
* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with this project. If not, see <https://www.gnu.org/licenses/>.
*
* For any further details and any questions please contact us under
* contact@nymea.io or see our FAQ/Licensing Information on
* https://nymea.io/license/faq
*
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "zwavemanager.h"
#include "extern-plugininfo.h"
#include "nymeasettings.h"

#include <QDebug>
#include <QSerialPortInfo>
#include <QCoreApplication>

ZwaveManager::ZwaveManager(QObject *parent) :
    QObject(parent)
{
    qRegisterMetaType<DriverEvent>("DriverEvent");
    qRegisterMetaType<ValueEvent>("ValueEvent");
    qRegisterMetaType<NodeEvent>("NodeEvent");

    qCDebug(dcZwave()) << "ZwaveManager: Using Z-Wave library version" << libraryVersion();

    Options::Create(CONFIG_PATH, NymeaSettings::settingsPath().toStdString(), "");

    Options::Get()->AddOptionInt("SaveLogLevel", LogLevel_None );
    Options::Get()->AddOptionInt("QueueLogLevel", LogLevel_None );
    Options::Get()->AddOptionInt("DumpTrigger", LogLevel_None );
    Options::Get()->AddOptionBool("Logging", false);
    Options::Get()->AddOptionBool("ConsoleOutput", false);

    Options::Get()->AddOptionInt("PollInterval", 500);
    Options::Get()->AddOptionBool("IntervalBetweenPolls", true);
    Options::Get()->AddOptionBool("ValidateValueChanges", true);
    Options::Get()->Lock();

    m_manager = Manager::Create();
    connect(this, &ZwaveManager::valueEvent, this, &ZwaveManager::onValueEvent);
    connect(this, &ZwaveManager::nodeEvent, this, &ZwaveManager::onNodeEvent);
}

ZwaveManager::~ZwaveManager()
{
    qCDebug(dcZwave()) << "ZwaveManager: Shutting down Z-Wave manager";
    Options::Destroy();
    if (m_initialized) {
        m_manager->RemoveWatcher(onNotification, this);
    }
    m_manager->Destroy();
}

QString ZwaveManager::libraryVersion() const
{
    return QString::fromStdString(Manager::getVersionAsString());
}

bool ZwaveManager::addDriver(const QString &driverPath)
{
    qCDebug(dcZwave()) << "ZwaveManager: Add driver" << driverPath;

    if (!serialPortAvailable(driverPath)) {
        qCWarning(dcZwave()) << "ZwaveManager: Could not find" << driverPath;
        return false;
    }
    if (!m_manager->AddDriver(driverPath.toStdString())) {
        qCWarning(dcZwave()) << "ZwaveManager: Could not add driver" << driverPath;
        return false;
    }
    return true;
}

bool ZwaveManager::removeDriver(const QString &driverPath)
{
    qCDebug(dcZwave()) << "ZwaveManger: Remove driver" << driverPath;
    return m_manager->RemoveDriver(driverPath.toStdString());
}

QString ZwaveManager::controllerPath(quint32 homeId) const
{
    QString controllerPath = m_manager->GetControllerPath(homeId).c_str();
    qCDebug(dcZwave()) << "ZwaveManager: Controller path for " << homeId << "is" << controllerPath;
    return controllerPath;
}

void ZwaveManager::softResetController(quint32 homeId)
{
    qCDebug(dcZwave()) << "ZwaveManger: Soft reset controller" << homeId;
    m_manager->SoftReset(homeId);
}

void ZwaveManager::hardResetController(quint32 homeId)
{
    qCDebug(dcZwave()) << "ZwaveManger: Hard reset controller" << homeId;
    m_manager->ResetController(homeId);
}

void ZwaveManager::addNode(quint32 homeId)
{
    qCDebug(dcZwave()) << "ZwaveManager: Add node ... starting the inclusion process" << homeId;
    m_manager->AddNode(homeId);
}

void ZwaveManager::removeNode(quint8 nodeId)
{
    Q_UNUSED(nodeId)
    qCDebug(dcZwave()) << "ZwaveManager: Remove node";
    //m_manager->RemoveNode()
}

bool ZwaveManager::init()
{
    qCDebug(dcZwave()) << "ZwaveManager: Init";

    if (!m_manager->AddWatcher(onNotification, this)) {
        qCWarning(dcZwave()) << "ZwaveManager: Could not register notification watcher.";
        return false;
    }

    m_initialized = true;
    return true;
}

QList<ZwaveNode *> ZwaveManager::nodes() const
{
    return m_nodes;
}

ZwaveNode *ZwaveManager::getNode(const quint8 &nodeId)
{
    foreach (ZwaveNode *node, nodes()) {
        if (node->nodeId() == nodeId) {
            return node;
        }
    }
    return NULL;
}

bool ZwaveManager::pressButton(const quint8 &nodeId, const ValueID &valueId)
{
    ZwaveNode *node = getNode(nodeId);
    if (!node)
        return false;

    if (!node->valueIds().contains(valueId))
        return false;

    return Manager::Get()->PressButton(valueId);
}

bool ZwaveManager::releaseButton(const quint8 &nodeId, const ValueID &valueId)
{
    ZwaveNode *node = getNode(nodeId);
    if (!node)
        return false;

    if (!node->valueIds().contains(valueId))
        return false;

    return m_manager->ReleaseButton(valueId);
}

bool ZwaveManager::serialPortAvailable(const QString &driverPath) const
{
    foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts()) {
        if (driverPath == info.systemLocation()) {
            return true;
        }
    }
    return false;
}

ZwaveNode *ZwaveManager::getNode(const Notification *notification)
{
    foreach (ZwaveNode *nodeInfo, m_nodes) {
        if (nodeInfo->homeId() == notification->GetHomeId() && nodeInfo->nodeId() == notification->GetNodeId())
            return nodeInfo;
    }
    return NULL;
}

QVariant ZwaveManager::getValue(const ValueID &valueId)
{
    QVariant value;

    switch (valueId.GetType()) {
    case ValueID::ValueType_Bool: {
        bool boolValue = false;
        Manager::Get()->GetValueAsBool(valueId, &boolValue);
        value = QVariant(boolValue);
        break;
    }
    case ValueID::ValueType_Byte: {
        quint8 byteValue = 0;
        Manager::Get()->GetValueAsByte(valueId, &byteValue);
        value = QVariant(byteValue);
        break;
    }
    case ValueID::ValueType_Decimal: {
        float floatValue = 0;
        Manager::Get()->GetValueAsFloat(valueId, &floatValue);
        value = QVariant(floatValue);
        break;
    }
    case ValueID::ValueType_Int: {
        int intValue = 0;
        Manager::Get()->GetValueAsInt(valueId, &intValue);
        value = QVariant(intValue);
        break;
    }
    case ValueID::ValueType_List: {
        vector<string> stringList;
        Manager::Get()->GetValueListItems(valueId, &stringList);
        //        QVariantList variantList;
        //        foreach (const string &stringItem, stringList) {
        //            qCDebug(dcZwave()) << "string item" << QString::fromStdString(stringItem);
        //        }
        break;
    }

        //        case ValueID::ValueType_Schedule:
        //            break;
        //        case ValueID::ValueType_Short:
        //            break;
    case ValueID::ValueType_Short: {
        short shortValue;
        Manager::Get()->GetValueAsShort(valueId, &shortValue);
        value = QVariant(shortValue);
        break;
    }
    case ValueID::ValueType_String: {
        string stringValue;
        Manager::Get()->GetValueAsString(valueId, &stringValue);
        value = QVariant(QString::fromStdString(stringValue));
        break;
    }

        //        case ValueID::ValueType_Button:
        //            break;
        //        case ValueID::ValueType_Raw:
        //            break;
    default:
        break;
    }
    return value;
}

void ZwaveManager::onNotification(const Notification *notification, void *context)
{
    ZwaveManager *manager = static_cast<ZwaveManager *>(context);

    switch(notification->GetType()) {
    /***********************************
     *          DRIVER EVENTS
     **********************************/
    case Notification::Type_DriverReady: {
        quint32 homeId = notification->GetHomeId();
        qCDebug(dcZwave()) << "ZwaveManager: Notification: Driver ready" << homeId;
        emit manager->driverEvent(homeId, DriverEventReady);
        break;
    }
    case Notification::Type_DriverFailed: {
        quint32 homeId = notification->GetHomeId();
        qCDebug(dcZwave()) << "ZwaveManager: Notification: Driver failed" << homeId;
        emit manager->driverEvent(homeId, DriverEventFailed);
        break;
    }
    case Notification::Type_DriverReset: {
        quint32 homeId = notification->GetHomeId();
        qCDebug(dcZwave()) << "ZwaveManager: Notification: Driver reset" << homeId;
        emit manager->driverEvent(homeId, DriverEventReset);
        break;
    }
    case Notification::Type_DriverRemoved: {
        quint32 homeId = notification->GetHomeId();
        qCDebug(dcZwave()) << "Notification: Driver removed" << homeId;
        emit manager->driverEvent(homeId, DriverEventRemoved);
        break;
    }
        /***********************************
         *          VALUE EVENTS
         **********************************/
    case Notification::Type_ValueAdded: {
        emit manager->valueEvent(notification->GetHomeId(), notification->GetNodeId(), notification->GetValueID().GetId(), ValueEventAdded);
        break;
    }
    case Notification::Type_ValueRemoved: {
        emit manager->valueEvent(notification->GetHomeId(), notification->GetNodeId(), notification->GetValueID().GetId(), ValueEventRemoved);
        break;
    }
    case Notification::Type_ValueChanged: {
        emit manager->valueEvent(notification->GetHomeId(), notification->GetNodeId(), notification->GetValueID().GetId(), ValueEventChanged);
        break;
    }
    case Notification::Type_ValueRefreshed: {
        emit manager->valueEvent(notification->GetHomeId(), notification->GetNodeId(), notification->GetValueID().GetId(), ValueEventRefreshed);
        break;
    }
        /***********************************
         *          NODE EVENTS
         **********************************/
    case Notification::Type_NodeNew: {
        qCDebug(dcZwave()) << "ZwaveManager: Notification: New node";
        emit manager->nodeEvent(notification->GetHomeId(), notification->GetNodeId(), NodeEventNew);
        break;
    }
    case Notification::Type_NodeAdded: {
        qCDebug(dcZwave()) << "ZwaveManager: Notification: Node added";
        emit manager->nodeEvent(notification->GetHomeId(), notification->GetNodeId(), NodeEventAdded);
        break;
    }
    case Notification::Type_NodeRemoved: {
        qCDebug(dcZwave()) << "ZwaveManager: Notification: Node removed";


        break;
    }
    case Notification::Type_NodeProtocolInfo: {
        //qCDebug(dcZwave()) << "ZwaveManager: Notification: Node protocol info";
        break;
    }
    case Notification::Type_NodeNaming: {
        //qCDebug(dcZwave()) << "ZwaveManager: Notification: Node naming";
        break;
    }
    case Notification::Type_NodeEvent: {
        //qCDebug(dcZwave()) << "ZwaveManager: Notification: Node event";
        break;
    }
    case Notification::Type_PollingDisabled: {
        //qCDebug(dcZwave()) << "ZwaveManager: Notification: Polling disabled";
        break;
    }
    case Notification::Type_PollingEnabled: {
        //qCDebug(dcZwave()) << "ZwaveManager: Notification: Polling enabled";
        break;
    }
    case Notification::Type_SceneEvent: {
        //qCDebug(dcZwave()) << "ZwaveManager: Notification: Scene event";
        break;
    }
    case Notification::Type_CreateButton: {
        //qCDebug(dcZwave()) << "ZwaveManager: Notification: Create button";
        break;
    }
    case Notification::Type_DeleteButton: {
        //qCDebug(dcZwave()) << "ZwaveManager: Notification: Delete button";
        break;
    }
    case Notification::Type_ButtonOn: {
        qCDebug(dcZwave()) << "ZwaveManager: Notification: Button on";
        break;
    }
    case Notification::Type_ButtonOff: {
        qCDebug(dcZwave()) << "ZwaveManager: Notification: Button off";
        break;
    }

    case Notification::Type_EssentialNodeQueriesComplete: {
        qCDebug(dcZwave()) << "ZwaveManager: Notification: Essential node queries complete";
        break;
    }
    case Notification::Type_NodeQueriesComplete: {
        qCDebug(dcZwave()) << "ZwaveManager: Notification: Node queries complete";
        break;
    }
    case Notification::Type_AwakeNodesQueried: {
        //qCDebug(dcZwave()) << "ZwaveManager: Notification: Awake nodes queried";
        break;
    }
    case Notification::Type_AllNodesQueriedSomeDead: {
        //qCDebug(dcZwave()) << "ZwaveManager: Notification: All nodes queried some dead";
        emit manager->initialized();
        break;
    }
    case Notification::Type_AllNodesQueried: {
        qCDebug(dcZwave()) << "ZwaveManager: Notification: All nodes queried";

        foreach (ZwaveNode *nodeInfo, manager->nodes()) {
            nodeInfo->m_name = QString::fromStdString(Manager::Get()->GetNodeName(nodeInfo->m_homeId, nodeInfo->m_nodeId));
            nodeInfo->m_manufacturerName = QString::fromStdString(Manager::Get()->GetNodeManufacturerName(nodeInfo->m_homeId, nodeInfo->m_nodeId));
            nodeInfo->m_productName = QString::fromStdString(Manager::Get()->GetNodeProductName(nodeInfo->m_homeId, nodeInfo->m_nodeId));
            nodeInfo->m_deviceTypeString = QString::fromStdString(Manager::Get()->GetNodeDeviceTypeString(nodeInfo->m_homeId, nodeInfo->m_nodeId));
            nodeInfo->m_deviceType = Manager::Get()->GetNodeDeviceType(nodeInfo->m_homeId, nodeInfo->m_nodeId);
        }

        emit manager->initialized();


        foreach (ZwaveNode *nodeInfo, manager->nodes()) {
            qCDebug(dcZwave()) << "-----------------------------------------------";
            qCDebug(dcZwave()) << "Node name       :" << Manager::Get()->GetNodeName(nodeInfo->m_homeId, nodeInfo->m_nodeId).c_str();
            qCDebug(dcZwave()) << "Manufaturer name:" << Manager::Get()->GetNodeManufacturerName(nodeInfo->m_homeId, nodeInfo->m_nodeId).c_str();
            qCDebug(dcZwave()) << "Product name    :" << Manager::Get()->GetNodeProductName(nodeInfo->m_homeId, nodeInfo->m_nodeId).c_str();
            qCDebug(dcZwave()) << "Device type     :" << Manager::Get()->GetNodeDeviceTypeString(nodeInfo->m_homeId, nodeInfo->m_nodeId).c_str();
            qCDebug(dcZwave()) << "Value count     :" << nodeInfo->valueIds().count();
            foreach (const ValueID &valueId, nodeInfo->valueIds()) {
                qCDebug(dcZwave()) << "-------------------------";
                qCDebug(dcZwave()) << "Value:" << Manager::Get()->GetValueLabel(valueId).c_str() << "("  << manager->valueTypeToString(valueId) <<  ")";
                qCDebug(dcZwave()) << "\tValue" << manager->getValue(valueId);
                qCDebug(dcZwave()) << "\tCommand class" << valueId.GetCommandClassId();
                qCDebug(dcZwave()) << "\tHelp" << Manager::Get()->GetValueHelp(valueId).c_str();
                qCDebug(dcZwave()) << "\tUnits" << Manager::Get()->GetValueUnits(valueId).c_str();
                qCDebug(dcZwave()) << "\tMin" << Manager::Get()->GetValueMin(valueId);
                qCDebug(dcZwave()) << "\tMax" << Manager::Get()->GetValueMax(valueId);
            }
            qCDebug(dcZwave()) << "-----------------------------------------------";
        }
        break;
    }
    case Notification::Type_Notification: {
        qCDebug(dcZwave()) << "ZwaveManager: Notification: Notification";
        break;
    }
    case Notification::Type_ControllerCommand: {
        qCDebug(dcZwave()) << "ZwaveManager: Notification: Controller command";
        break;
    }
    default: {
        qCWarning(dcZwave()) << "ZwaveManager: Unhaldled notification received" << notification->GetType() << QString::fromStdString(notification->GetAsString());
    }

    }
}

QString ZwaveManager::valueTypeToString(const ValueID &valueId)
{
    switch (valueId.GetType()) {
    case ValueID::ValueType_Bool:
        return "Bool";
    case ValueID::ValueType_Byte:
        return "Byte";
    case ValueID::ValueType_Decimal:
        return "Double";
    case ValueID::ValueType_Int:
        return "Int";
    case ValueID::ValueType_List:
        return "List";
    case ValueID::ValueType_Schedule:
        return "Schedule";
    case ValueID::ValueType_Short:
        return "Short";
    case ValueID::ValueType_String:
        return "String";
    case ValueID::ValueType_Button:
        return "Button";
    case ValueID::ValueType_Raw:
        return "Raw";
    default:
        return "-";
        break;
    }
}

void ZwaveManager::onNodeEvent(quint32 homeId, quint8 nodeId, NodeEvent event)
{
    switch (event) {
    case NodeEventAdded: {

        ZwaveNode *nodeInfo = new ZwaveNode(this);
        nodeInfo->m_homeId = homeId;
        nodeInfo->m_nodeId = nodeId;
        nodeInfo->m_polled = false;
        nodeInfo->m_name = QString::fromStdString(m_manager->GetNodeName(nodeInfo->m_homeId, nodeInfo->m_nodeId));
        nodeInfo->m_manufacturerName = QString::fromStdString(m_manager->GetNodeManufacturerName(nodeInfo->m_homeId, nodeInfo->m_nodeId));
        nodeInfo->m_productName = QString::fromStdString(m_manager->GetNodeProductName(nodeInfo->m_homeId, nodeInfo->m_nodeId));
        nodeInfo->m_deviceTypeString = QString::fromStdString(m_manager->GetNodeDeviceTypeString(nodeInfo->m_homeId, nodeInfo->m_nodeId));
        nodeInfo->m_deviceType = m_manager->GetNodeDeviceType(nodeInfo->m_homeId, nodeInfo->m_nodeId);
        nodeInfo->m_manufacturerId =  QString::fromStdString(m_manager->GetNodeManufacturerId(nodeInfo->m_homeId, nodeInfo->m_nodeId));

        m_nodes.append(nodeInfo);
        emit nodeAdded(nodeInfo);
        break;
    }
    case NodeEventRemoved: {
        foreach (ZwaveNode *nodeInfo, m_nodes) {
            if (nodeInfo->homeId() == homeId && nodeInfo->nodeId() == nodeId) {
                m_nodes.removeOne(nodeInfo);
                emit nodeRemoved(nodeInfo->nodeId());
                nodeInfo->deleteLater();
            }
        }
        emit nodeRemoved(nodeId);
    }
    default:
        break;
    }
}

void ZwaveManager::onValueEvent(quint32 homeId, quint8 nodeId,  quint64 valueId, ValueEvent event)
{
    Q_UNUSED(homeId)
    Q_UNUSED(nodeId)
    Q_UNUSED(valueId)
    Q_UNUSED(event)
    ValueID vid(homeId, valueId);
    /*if (!m_manager->IsValueSet(vid)) {
        qCWarning(dcZwave()) << "ZWaveManager: valueId is not set" << valueId;
        return;
    }*/

    switch (event) {
    case ValueEventAdded: {
        qCDebug(dcZwave()) << "ZwaveManager: Value added" << nodeId << valueId << m_manager->GetValueHelp(vid).c_str();
        break;
    }
    case ValueEventChanged: {
        qCDebug(dcZwave()) << "ZwaveManager: Value changed";
        break;
    }
    case ValueEventRemoved: {
        qCDebug(dcZwave()) << "ZwaveManager: Value removed";
        break;
    }
    default:
        break;
    }

    //        ZwaveNode *nodeInfo = manager->getNode(notification);
    //        if (!nodeInfo) {
    //            qCWarning(dcZwave()) << "Could not find node for new value";
    //            break;
    //        }

    //        ValueID valueId = notification->GetValueID();
    //        nodeInfo->m_valueIds.append(valueId);

    //    ZwaveNode *nodeInfo = manager->getNode(notification);
    //    if (!nodeInfo)
    //        break;

    //    ValueID valueId = notification->GetValueID();

    //    qCDebug(dcZwave()) << "Value changed:" << Manager::Get()->GetValueLabel(valueId).c_str() << ":" << manager->getValue(valueId);


}
