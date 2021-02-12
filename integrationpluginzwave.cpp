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

#include "integrationpluginzwave.h"
#include "plugininfo.h"

#include <QSerialPortInfo>

using namespace OpenZWave;

IntegrationPluginZwave::IntegrationPluginZwave()
{
    m_nodeIdParamTypeIds.insert(plugThingClassId, plugThingIdParamTypeId);
    m_nodeIdParamTypeIds.insert(shutterThingClassId, shutterThingIdParamTypeId);
    m_nodeIdParamTypeIds.insert(motionSensorThingClassId, motionSensorThingIdParamTypeId);

    m_connectedStateTypeIds.insert(interfaceThingClassId, interfaceConnectedStateTypeId);
    m_connectedStateTypeIds.insert(plugThingClassId, plugConnectedStateTypeId);
    m_connectedStateTypeIds.insert(shutterThingClassId, shutterConnectedStateTypeId);
    m_connectedStateTypeIds.insert(motionSensorThingClassId, motionSensorThingClassId);

    m_removeNodeActionTypeIds.insert(plugThingClassId, plugRemoveNodeActionTypeId);
    m_removeNodeActionTypeIds.insert(shutterThingClassId, shutterRemoveNodeActionTypeId);
    m_removeNodeActionTypeIds.insert(motionSensorThingClassId, motionSensorRemoveNodeActionTypeId);
}

void IntegrationPluginZwave::discoverThings(ThingDiscoveryInfo *info)
{
    qCDebug(dcZwave()) << "Discover things";
    Q_FOREACH (QSerialPortInfo port, QSerialPortInfo::availablePorts()) {

        qCDebug(dcZwave()) << "Found serial port:" << port.portName();
        qCDebug(dcZwave()) << "     - Manufacturer:" << port.manufacturer();
        qCDebug(dcZwave()) << "     - Description:" << port.description();
        qCDebug(dcZwave()) << "     - Serial number:" << port.serialNumber();
        qCDebug(dcZwave()) << "     - Location:" << port.systemLocation();
        qCDebug(dcZwave()) << "     - Is busy:" << port.isBusy();

        if (port.isBusy())
            continue;

        QString serialnumber =  port.serialNumber();
        if (serialnumber.isEmpty()) {
            // User model identification instead of the serial number
            // This won't work properly with multible interface of the same model
            serialnumber = port.manufacturer() + port.description();
        }
        ThingDescriptor thingDescriptor(info->thingClassId(), port.portName(), serialnumber);
        ParamList params;
        if (!serialnumber.isEmpty()) {
            // Some serial interfaces don't have a serial number
            foreach (Thing *existingThing, myThings()) {
                if (existingThing->paramValue(interfaceThingSerialNumberParamTypeId).toString() == serialnumber) {
                    thingDescriptor.setThingId(existingThing->id());
                    break;
                }
            }
        }
        params.append(Param(interfaceThingPathParamTypeId, port.systemLocation()));
        params.append(Param(interfaceThingSerialNumberParamTypeId, serialnumber));
        thingDescriptor.setParams(params);
        info->addThingDescriptor(thingDescriptor);
    }
    info->finish(Thing::ThingErrorNoError);
}

void IntegrationPluginZwave::setupThing(ThingSetupInfo *info)
{
    qCDebug(dcZwave()) << "Setup thing" << info->thing()->name();
    Thing *thing = info->thing();

    if (thing->thingClassId() == interfaceThingClassId) {

        if (!m_zwaveManager) {
            m_zwaveManager = new ZwaveManager(this);
            connect(info, &ThingSetupInfo::aborted, m_zwaveManager, &ZwaveManager::deleteLater);

            if (!m_zwaveManager->init()) {
                qCWarning(dcZwave()) << "Could not init Z-Wave manager";
                return info->finish(Thing::ThingErrorHardwareNotAvailable);
            }
        }

        //TODO cleanup after reconfiguration

        QString serialNumber = thing->paramValue(interfaceThingSerialNumberParamTypeId).toString();
        QString path;
        if (serialNumber.isEmpty()) {
            path = thing->paramValue(interfaceThingPathParamTypeId).toString();
        } else {
            path = findSerialPortPathBySerialnumber(serialNumber);
            thing->setParamValue(interfaceThingPathParamTypeId, path);
        }

        if (!m_zwaveManager->addDriver(path)) {
            qCWarning(dcZwave()) << "Could not add driver";
            return info->finish(Thing::ThingErrorHardwareNotAvailable);
        }

        connect(m_zwaveManager, &ZwaveManager::driverEvent, info, [info, this] (quint32 homeID, ZwaveManager::DriverEvent event) {
            // TODOs for multi driver:
            // Check if the homeId is already in the system
            // If not then it is probably the newly added driver
            QString controllerPath = info->thing()->paramValue(interfaceThingPathParamTypeId).toString();
            if (controllerPath == m_zwaveManager->controllerPath(homeID)) {
                if (event == ZwaveManager::DriverEventReady) {
                    qCDebug(dcZwave()) << "Driver ready for" << info->thing()->name();
                    info->thing()->setStateValue(interfaceHomeIdStateTypeId, homeID);
                    info->thing()->setStateValue(interfaceConnectedStateTypeId, true);
                    info->finish(Thing::ThingErrorNoError);
                } else if (event == ZwaveManager::DriverEventFailed) {
                    info->finish(Thing::ThingErrorHardwareFailure);
                }
            }
        });

        connect(m_zwaveManager, &ZwaveManager::driverEvent, this, &IntegrationPluginZwave::onDriverEvent);
        connect(m_zwaveManager, &ZwaveManager::initialized, this, &IntegrationPluginZwave::onInitialized);
        connect(m_zwaveManager, &ZwaveManager::nodeAdded, this, &IntegrationPluginZwave::onNodeAdded);
        connect(m_zwaveManager, &ZwaveManager::nodeRemoved, this, &IntegrationPluginZwave::onNodeRemoved);

        //connect(manager, &ZwaveManager::destroyed, this, [manager, this]{
        //    m_asyncSetup.remove(manager);
        //});
    } else if (thing->thingClassId() == shutterThingClassId) {
        return info->finish(Thing::ThingErrorNoError);
    } else if (thing->thingClassId() == plugThingClassId) {
        return info->finish(Thing::ThingErrorNoError);
    } else if (thing->thingClassId() == motionSensorThingClassId) {
        return info->finish(Thing::ThingErrorNoError);
    } else {
        return info->finish(Thing::ThingErrorThingClassNotFound);
    }

}

void IntegrationPluginZwave::postSetupThing(Thing *thing)
{
    qCDebug(dcZwave()) << "Post setup thing" << thing->name();

    if (thing->thingClassId() == interfaceThingClassId) {

    } else if (thing->thingClassId() == shutterThingClassId) {

    } else if (thing->thingClassId() == plugThingClassId) {

    } else if (thing->thingClassId() == motionSensorThingClassId) {

    } else {
        qCWarning(dcZwave()) << "Post setup thing: thing class not found" << thing->name() << thing->thingClassId();
    }
}

void IntegrationPluginZwave::executeAction(ThingActionInfo *info)
{
    Thing *thing = info->thing();
    Action action = info->action();

    if (m_removeNodeActionTypeIds.contains(thing->thingClassId())) {
        if (action.actionTypeId() == m_removeNodeActionTypeIds.value(thing->thingClassId())) {
            quint8 nodeId = static_cast<quint8>(thing->paramValue(m_nodeIdParamTypeIds.value(thing->thingClassId())).toUInt());
            m_zwaveManager->removeNode(nodeId);
            return info->finish(Thing::ThingErrorNoError);
        }
    }

    if (thing->thingClassId() == interfaceThingClassId) {

        quint32 homeId = thing->stateValue(interfaceHomeIdStateTypeId).toUInt();
        if (action.actionTypeId() == interfaceSoftResetActionTypeId) {
            m_zwaveManager->softResetController(homeId);
            return info->finish(Thing::ThingErrorNoError);

        } else if (action.actionTypeId() == interfaceHardResetActionTypeId) {
            m_zwaveManager->hardResetController(homeId);
            return info->finish(Thing::ThingErrorNoError);
        } else if (action.actionTypeId() == interfaceHardResetActionTypeId) {
            m_zwaveManager->hardResetController(homeId);
            return info->finish(Thing::ThingErrorNoError);
        } else if (action.actionTypeId() == interfaceAddNodeActionTypeId) {
            m_zwaveManager->addNode(homeId);
            return info->finish(Thing::ThingErrorNoError);
        } else {
            return info->finish(Thing::ThingErrorActionTypeNotFound);
        }

    } else if (thing->thingClassId() == shutterThingClassId) {

        quint8 nodeId = (quint8) thing->paramValue(shutterThingIdParamTypeId).toUInt();
        if (!m_zwaveManager)
            return;
        ZwaveNode *node = m_zwaveManager->getNode(nodeId);
        if (!node) {
            qCWarning(dcZwave()) << "Could not find note with id" << nodeId;
            return info->finish(Thing::ThingErrorHardwareNotAvailable);
        }

        if (action.actionTypeId() == shutterOpenActionTypeId) {
            foreach (const ValueID &valueId, node->valueIds()) {
                if (valueId.GetType() == ValueID::ValueType_Button && QString::fromStdString(Manager::Get()->GetValueLabel(valueId)) == "Up") {
                    if (!m_zwaveManager->pressButton(nodeId, valueId)) {
                        return;
                    }
                }
            }
        } else if (action.actionTypeId() == shutterCloseActionTypeId) {
            foreach (const ValueID &valueId, node->valueIds()) {
                if (valueId.GetType() == ValueID::ValueType_Button && QString::fromStdString(Manager::Get()->GetValueLabel(valueId)) == "Down") {
                    if (!m_zwaveManager->pressButton(nodeId, valueId)) {
                        return;
                    }
                }
            }
        } else if (action.actionTypeId() == shutterStopActionTypeId) {
            foreach (const ValueID &valueId, node->valueIds()) {
                if (valueId.GetType() == ValueID::ValueType_Button && QString::fromStdString(Manager::Get()->GetValueLabel(valueId)) == "Up") {
                    if (!m_zwaveManager->releaseButton(nodeId, valueId)) {
                        return;
                    }
                }
                if (valueId.GetType() == ValueID::ValueType_Button && QString::fromStdString(Manager::Get()->GetValueLabel(valueId)) == "Down") {
                    if (!m_zwaveManager->releaseButton(nodeId, valueId)) {
                        return;
                    }
                }
            }
        } else {
            return info->finish(Thing::ThingErrorActionTypeNotFound);
        }
    } else if (thing->thingClassId() == plugThingClassId) {
        if (action.actionTypeId() == plugPowerStateTypeId) {

        } else {
            qCWarning(dcZwave()) << "Execute action: action type id not found" << thing->name() << action.actionTypeId();
            return info->finish(Thing::ThingErrorActionTypeNotFound);
        }
    } else {
        qCWarning(dcZwave()) << "Execute action: thing class not found" << thing->name() << thing->thingClassId();
        return info->finish(Thing::ThingErrorThingClassNotFound);
    }
}

void IntegrationPluginZwave::thingRemoved(Thing *thing)
{
    qCDebug(dcZwave()) << "Delete" << thing->name();
    if (thing->thingClassId() == interfaceThingClassId) {
        qCDebug(dcZwave()) << "Deleting Z-Wave manager";
        m_zwaveManager->deleteLater();
        m_zwaveManager = nullptr;
    } else if (thing->thingClassId() == shutterThingClassId) {

    }

    if (myThings().isEmpty()) {

        qCDebug(dcZwave()) << "Stopping timer";
    }
}

QString IntegrationPluginZwave::findSerialPortPathBySerialnumber(const QString &serialNumber) const
{
    Q_FOREACH (QSerialPortInfo port, QSerialPortInfo::availablePorts()) {

        QString portSerialNumber =  port.serialNumber();
        if (portSerialNumber.isEmpty()) {
            portSerialNumber = port.manufacturer() + port.description();
        }
        if (portSerialNumber == serialNumber) {
            return port.systemLocation();
        }
    }
    return "";
}

bool IntegrationPluginZwave::alreadyAdded(const quint8 &nodeId)
{
    foreach (Thing *thing, myThings()) {
        if (m_nodeIdParamTypeIds.contains(thing->thingClassId())) {
            if (nodeId == (quint8)thing->paramValue(m_nodeIdParamTypeIds.value(thing->thingClassId())).toUInt()) {
                return true;
            }
        }
    }
    return false;
}

bool IntegrationPluginZwave::setCalibrationMode(const quint8 &nodeId, const bool &calibrationMode)
{
    Q_UNUSED(nodeId)
    Q_UNUSED(calibrationMode)
    return true;
}

void IntegrationPluginZwave::onDriverEvent(quint32 homeID, ZwaveManager::DriverEvent event)
{
    QString path = m_zwaveManager->controllerPath(homeID);
    qCDebug(dcZwave()) << "On driver event" << homeID << event << path;

    Q_FOREACH(Thing *thing, myThings().filterByThingClassId(interfaceThingClassId)) {
        if (thing->stateValue(interfaceHomeIdStateTypeId).toUInt() == homeID) {
            if (event == ZwaveManager::DriverEventReady) {
                thing->setStateValue(interfaceConnectedStateTypeId, true);
            } else if (event == ZwaveManager::DriverEventFailed) {
                thing->setStateValue(interfaceConnectedStateTypeId, false);
            }
            return;
        }
    }
    // After a hard reset the homeId changes
    // Both search methods are required
    Q_FOREACH(Thing *thing, myThings().filterByThingClassId(interfaceThingClassId)) {
        if (thing->paramValue(interfaceThingPathParamTypeId).toString() == path) {
            thing->setStateValue(interfaceHomeIdStateTypeId, homeID);
            if (event == ZwaveManager::DriverEventReady) {
                thing->setStateValue(interfaceConnectedStateTypeId, true);
            } else if (event == ZwaveManager::DriverEventFailed) {
                thing->setStateValue(interfaceConnectedStateTypeId, false);
            }
            return;
        }
    }
}

void IntegrationPluginZwave::onInitialized()
{
    ZwaveManager *manager = static_cast<ZwaveManager *>(sender());
    ThingDescriptors descriptorList;

    foreach (ZwaveNode *node, manager->nodes()) {
        //qCDebug(dcZwave()) << "+" << node->name() << node->manufacturerName() << node->productName() << node->deviceType();

        foreach (Thing *thing, myThings()) {
            if (m_nodeIdParamTypeIds.contains(thing->thingClassId())) {
                if ((quint8)thing->paramValue(m_nodeIdParamTypeIds.value(thing->thingClassId())).toUInt() == node->nodeId()) {
                    thing->setStateValue(m_connectedStateTypeIds.value(thing->thingClassId()), true);
                }
            }
        }

        // Check if we have found the Qubino shutter
        if (node->deviceType() == 6656 && !alreadyAdded(node->nodeId())) {
            ThingDescriptor descriptor(shutterThingClassId);
            ParamList params;
            params.append(Param(shutterThingIdParamTypeId, node->nodeId()));
            descriptor.setParams(params);
            descriptorList.append(descriptor);
        }
    }
    emit autoThingsAppeared(descriptorList);
}

void IntegrationPluginZwave::onNodesChanged()
{
    qCDebug(dcZwave()) << "Nodes changed";
}

void IntegrationPluginZwave::onNodeAdded(ZwaveNode *node)
{
    qCDebug(dcZwave()) << "On node added " << node->name();
    qCDebug(dcZwave()) << "     - Manufacturer:" << node->manufacturerName();
    qCDebug(dcZwave()) << "     - Product name:" << node->productName();
    qCDebug(dcZwave()) << "     - Device type Id:" << node->deviceType();
    qCDebug(dcZwave()) << "     - Device type:" << node->deviceTypeString();

    foreach (Thing *thing, myThings()) {
        if (m_nodeIdParamTypeIds.contains(thing->thingClassId())) {
            if ((quint8)thing->paramValue(m_nodeIdParamTypeIds.value(thing->thingClassId())).toUInt() == node->nodeId())
                thing->setStateValue(m_connectedStateTypeIds.value(thing->thingClassId()), true);

        }
    }
}

void IntegrationPluginZwave::onNodeRemoved(const quint8 &nodeId)
{
    qCDebug(dcZwave()) << "Node removed: " << nodeId;

    foreach (Thing *thing, myThings()) {
        if (m_nodeIdParamTypeIds.contains(thing->thingClassId())) {
            if ((quint8)thing->paramValue(m_nodeIdParamTypeIds.value(thing->thingClassId())).toUInt() == nodeId) {
                emit autoThingDisappeared(thing->id());
            }
        }
    }
}
