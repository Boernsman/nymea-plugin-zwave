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

#ifndef INTEGRATIONPLUGINZWAVE_H
#define INTEGRATIONPLUGINZWAVE_H

#include "integrations/thingmanager.h"

#include <QObject>

#include "zwavemanager.h"

class IntegrationPluginZwave : public IntegrationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "io.nymea.IntegrationPlugin" FILE "integrationpluginzwave.json")
    Q_INTERFACES(IntegrationPlugin)

public:
    IntegrationPluginZwave();

    void discoverThings(ThingDiscoveryInfo *info) override;
    void setupThing(ThingSetupInfo *info) override;
    void postSetupThing(Thing *thing) override;
    void executeAction(ThingActionInfo *info) override;
    void thingRemoved(Thing *thing) override;

private:
    QHash<ThingClassId, ParamTypeId> m_nodeIdParamTypeIds;
    QHash<ThingClassId, StateTypeId> m_connectedStateTypeIds;
    QHash<ThingClassId, ActionTypeId> m_removeNodeActionTypeIds;

    ZwaveManager *m_zwaveManager = nullptr;
    QHash<ZwaveManager *, ThingSetupInfo *> m_asyncSetup;

    QString findSerialPortPathBySerialnumber(const QString &serialNumber) const;
    bool alreadyAdded(const quint8 &nodeId);
    bool setCalibrationMode(const quint8 &nodeId, const bool &calibrationMode);

private slots:
    void onDriverEvent(quint32 homeId, ZwaveManager::DriverEvent event);
    void onInitialized();
    void onNodesChanged();

    void onNodeAdded(ZwaveNode *node);
    void onNodeRemoved(const quint8 &nodeId);
};

#endif // INTEGRATIONPLUGINZWAVE_H
