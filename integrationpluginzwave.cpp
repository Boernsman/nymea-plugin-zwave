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

}

void IntegrationPluginZwave::discoverThings(ThingDiscoveryInfo *info)
{
    qCDebug(dcZwave()) << "Discover things";
    Q_FOREACH (QSerialPortInfo port, QSerialPortInfo::availablePorts()) {

        qCDebug(dcZwave()) << "Found serial port:" << port.portName();
        QString description = port.manufacturer() + " " + port.description() + " " + port.serialNumber();
        ThingDescriptor thingDescriptor(info->thingClassId(), port.portName(), description);
        ParamList params;
        foreach (Thing *existingThing, myThings()) {
            if (existingThing->paramValue().toString() == port.serialNumber()) {
                thingDescriptor.setThingId(existingThing->id());
                break;
            }
        }
        params.append(Param(stickThingPathParamTypeId, port.portName()));
        thingDescriptor.setParams(params);
        info->addThingDescriptor(thingDescriptor);
    }
    info->finish(Thing::ThingErrorNoError);
}

void IntegrationPluginZwave::setupThing(ThingSetupInfo *info)
{
    qCDebug(dcZwave()) << "Setup thing" << info->thing()->name();
    Thing *thing = info->thing();

    //device->setStateValue(availableStateTypeId, true);
    if (thing->thingClassId() == stickThingClassId) {
        qCDebug(dcZwave()) << "Setting up z-wave manager";

        Q_FOREACH (Thing *existingThing, myThings().filterByThingClassId(stickThingClassId)) {
            if ()
        }
        if (m_zwaveManager) {
            qCWarning(dcZwave()) << "Only one Z-Wave interface supported";
            return info->finish(Thing::ThingErrorHardwareFailure, "Only one interface supported");
        }
            m_zwaveManager = new ZwaveManager(this);
            connect(info, &ThingSetupInfo::aborted, m_zwaveManager, &ZwaveManager::deleteLater);
            if (!m_zwaveManager->init()) {
                qCWarning(dcZwave()) << "Could not enable z-wave";
                return info->finish(Thing::ThingErrorHardwareNotAvailable);
            }
        }

        //TODO find serial port by serial number
        connect(m_zwaveManager, &ZwaveManager::driverReadyChanged, this, &IntegrationPluginZwave::onDriverReadyChanged);

        connect(m_zwaveManager, &ZwaveManager::driverReadyChanged, this, &IntegrationPluginZwave::onDriverReadyChanged);
        connect(m_zwaveManager, &ZwaveManager::initialized, this, &IntegrationPluginZwave::onInitialized);
        connect(m_zwaveManager, &ZwaveManager::nodeAdded, this, &IntegrationPluginZwave::onNodeAdded);
        connect(m_zwaveManager, &ZwaveManager::noderRemoved, this, &IntegrationPluginZwave::onNodeRemoved);

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
}

void IntegrationPluginZwave::executeAction(ThingActionInfo *info)
{
    Thing *thing = info->thing();
    Action action = info->action();

    if (thing->thingClassId() == stickThingClassId) {

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
    if (thing->thingClassId() == stickThingClassId) {

    } else if (thing->thingClassId() == shutterThingClassId) {

    }

    if (myThings().isEmpty()) {
        qCDebug(dcZwave()) << "Deleting Z-Wave manager";
        m_zwaveManager->deleteLater();
        m_zwaveManager = nullptr;
        qCDebug(dcZwave()) << "Stopping timer";
    }
}

bool IntegrationPluginZwave::alreadyAdded(const quint8 &nodeId)
{
    foreach (Thing *thing, myThings()) {
        if (thing->thingClassId() == shutterThingClassId) {
            if (nodeId == (quint8)thing->paramValue(shutterThingIdParamTypeId).toUInt()){
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

void IntegrationPluginZwave::onDriverReadyChanged()
{
    qCDebug(dcZwave()) << "On driver ready changed";
}

void IntegrationPluginZwave::onInitialized()
{
    ZwaveManager *manager = static_cast<ZwaveManager *>(sender());
    ThingDescriptors descriptorList;

    foreach (ZwaveNode *node, manager->nodes()) {
        //qCDebug(dcZwave()) << "+" << node->name() << node->manufacturerName() << node->productName() << node->deviceType();

        foreach (Thing *thing, myThings()) {
            if (thing->thingClassId() == shutterThingClassId) {
                if ((quint8)thing->paramValue(shutterThingIdParamTypeId).toUInt() == node->nodeId()) {
                    thing->setStateValue(shutterConnectedStateTypeId, true);
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
    qCDebug(dcZwave()) << "+" << node->name() << node->manufacturerName() << node->productName() << node->deviceType();

    foreach (Thing *thing, myThings()) {
        if (thing->thingClassId() == shutterThingClassId) {
            if ((quint8)thing->paramValue(shutterThingIdParamTypeId).toUInt() == node->nodeId())
                thing->setStateValue(shutterConnectedStateTypeId, true);

        }
    }
}

void IntegrationPluginZwave::onNodeRemoved(const quint8 &nodeId)
{
    qCDebug(dcZwave()) << "Node removed: " << nodeId;

    foreach (Thing *thing, myThings()) {
        if (thing->thingClassId() == shutterThingClassId) {
            if ((quint8)thing->paramValue(shutterThingIdParamTypeId).toUInt() == nodeId) {
                emit autoThingDisappeared(thing->id());
            }
        }
    }
}
