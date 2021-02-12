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

ZwaveManager::ZwaveManager(QObject *parent) :
    QObject(parent),
    m_initialized(false),
    m_homeId(0)
{
    qCDebug(dcZwave()) << "ZwaveManager: Using Z-Wave library version" << libraryVersion();
    m_manager = Manager::Create();
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
    return m_manager->RemoveDriver(driverPath.toStdString());
}

bool ZwaveManager::init()
{
    qCDebug(dcZwave()) << "ZwaveManager: Init";

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
    case Notification::Type_ValueAdded: {
        //qCDebug(dcZwave()) << "Notification: Value added";
        ZwaveNode *nodeInfo = manager->getNode(notification);
        if (!nodeInfo) {
            qCWarning(dcZwave()) << "Could not find node for new value";
            break;
        }

        ValueID valueId = notification->GetValueID();
        nodeInfo->m_valueIds.append(valueId);

        break;
    }
    case Notification::Type_ValueRemoved: {
        //qCDebug(dcZwave()) << "Notification: Value removed";
        ZwaveNode *nodeInfo = manager->getNode(notification);
        if (!nodeInfo)
            break;

        foreach (const ValueID &valueId, nodeInfo->valueIds()) {
            if (valueId == notification->GetValueID()) {
                nodeInfo->m_valueIds.removeOne(valueId);
                break;
            }
        }
        break;
    }
    case Notification::Type_ValueChanged: {
        //qCDebug(dcZwave()) << "Notification: Value changed";
        ZwaveNode *nodeInfo = manager->getNode(notification);
        if (!nodeInfo)
            break;

        ValueID valueId = notification->GetValueID();

        qCDebug(dcZwave()) << "Value changed:" << Manager::Get()->GetValueLabel(valueId).c_str() << ":" << manager->getValue(valueId);

        break;
    }
    case Notification::Type_ValueRefreshed: {
        //qCDebug(dcZwave()) << "Notification: Value refreshed";
        break;
    }
    case Notification::Type_Group: {
        // The associations for the node have changed. The application should rebuild any group information it holds about the node.
        //qCDebug(dcZwave()) << "Notification: Group";
        break;
    }
    case Notification::Type_NodeNew: {
        //qCDebug(dcZwave()) << "Notification: New node";
        break;
    }
    case Notification::Type_NodeAdded: {
        //qCDebug(dcZwave()) << "Notification: Node added";

        ZwaveNode *nodeInfo = new ZwaveNode();
        nodeInfo->m_homeId = notification->GetHomeId();
        nodeInfo->m_nodeId = notification->GetNodeId();
        nodeInfo->m_polled = false;
        nodeInfo->m_name = QString::fromStdString(Manager::Get()->GetNodeName(nodeInfo->m_homeId, nodeInfo->m_nodeId));
        nodeInfo->m_manufacturerName = QString::fromStdString(Manager::Get()->GetNodeManufacturerName(nodeInfo->m_homeId, nodeInfo->m_nodeId));
        nodeInfo->m_productName = QString::fromStdString(Manager::Get()->GetNodeProductName(nodeInfo->m_homeId, nodeInfo->m_nodeId));
        nodeInfo->m_deviceTypeString = QString::fromStdString(Manager::Get()->GetNodeDeviceTypeString(nodeInfo->m_homeId, nodeInfo->m_nodeId));
        nodeInfo->m_deviceType = Manager::Get()->GetNodeDeviceType(nodeInfo->m_homeId, nodeInfo->m_nodeId);

        manager->m_nodes.append(nodeInfo);
        emit manager->nodeAdded(nodeInfo);

        break;
    }
    case Notification::Type_NodeRemoved: {
        //qCDebug(dcZwave()) << "Notification: Node removed";

        foreach (ZwaveNode *nodeInfo, manager->m_nodes) {
            if (nodeInfo->homeId() == notification->GetHomeId() && nodeInfo->nodeId() == notification->GetNodeId()) {
                manager->m_nodes.removeOne(nodeInfo);
                emit manager->noderRemoved(nodeInfo->nodeId());
                nodeInfo->deleteLater();
            }
        }
        break;
    }
    case Notification::Type_NodeProtocolInfo: {
        //qCDebug(dcZwave()) << "Notification: Node protocol info";
        break;
    }
    case Notification::Type_NodeNaming: {
        //qCDebug(dcZwave()) << "Notification: Node naming";
        break;
    }
    case Notification::Type_NodeEvent: {
        //qCDebug(dcZwave()) << "Notification: Node event";
        break;
    }
    case Notification::Type_PollingDisabled: {
        //qCDebug(dcZwave()) << "Notification: Polling disabled";
        break;
    }
    case Notification::Type_PollingEnabled: {
        //qCDebug(dcZwave()) << "Notification: Polling enabled";
        break;
    }
    case Notification::Type_SceneEvent: {
        //qCDebug(dcZwave()) << "Notification: Scene event";
        break;
    }
    case Notification::Type_CreateButton: {
        //qCDebug(dcZwave()) << "Notification: Create button";
        break;
    }
    case Notification::Type_DeleteButton: {
        //qCDebug(dcZwave()) << "Notification: Delete button";
        break;
    }
    case Notification::Type_ButtonOn: {
        qCDebug(dcZwave()) << "Notification: Button on";
        break;
    }
    case Notification::Type_ButtonOff: {
        qCDebug(dcZwave()) << "Notification: Button off";
        break;
    }
    case Notification::Type_DriverReady: {
        qCDebug(dcZwave()) << "ZwaveManager: Notification: Driver ready";

        emit manager->driverReadyChanged(notification->GetHomeId());
        break;
    }
    case Notification::Type_DriverFailed: {
        qCDebug(dcZwave()) << "ZwaveManager: Notification: Driver failed";
        emit manager->driverReadyChanged(notification->GetHomeId());
        break;
    }
    case Notification::Type_DriverReset: {
        qCDebug(dcZwave()) << "ZwaveManager: Notification: Driver reset";
        emit manager->driverReadyChanged(notification->GetHomeId());
        break;
    }
    case Notification::Type_EssentialNodeQueriesComplete: {
        //qCDebug(dcZwave()) << "Notification: Essential node queries compete";
        break;
    }
    case Notification::Type_NodeQueriesComplete: {
        //qCDebug(dcZwave()) << "Notification: Node queries compete";
        break;
    }
    case Notification::Type_AwakeNodesQueried: {
        //qCDebug(dcZwave()) << "Notification: Awake nodes queried";
        break;
    }
    case Notification::Type_AllNodesQueriedSomeDead: {
        //qCDebug(dcZwave()) << "Notification: Awake nodes queried some dead";
        emit manager->initialized();
        break;
    }
    case Notification::Type_AllNodesQueried: {
        qCDebug(dcZwave()) << "Notification: All nodes queried";

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
        //qCDebug(dcZwave()) << "Notification: Notification";
        break;
    }
    case Notification::Type_DriverRemoved: {
        qCDebug(dcZwave()) << "Notification: Driver removed";
        notification->GetHomeId();
        emit manager->driverReadyChanged();
        break;
    }
    case Notification::Type_ControllerCommand: {
        qCDebug(dcZwave()) << "Notification: Controller command";
        break;
    }
    default: {
        qCWarning(dcZwave()) << "Unhaldled notification received" << notification->GetType() << QString::fromStdString(notification->GetAsString());
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

