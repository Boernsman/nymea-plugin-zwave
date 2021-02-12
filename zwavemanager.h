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

#ifndef ZWAVEMANAGER_H
#define ZWAVEMANAGER_H

#include <QObject>

#include "openzwave/Options.h"
#include "openzwave/Manager.h"
#include "openzwave/Driver.h"
#include "openzwave/Node.h"
#include "openzwave/Group.h"
#include "openzwave/Defs.h"
#include "openzwave/Notification.h"
#include "openzwave/platform/Log.h"
#include "openzwave/value_classes/Value.h"

#include "zwavenode.h"

using namespace OpenZWave;

class ZwaveManager : public QObject
{
    Q_OBJECT
public:
    explicit ZwaveManager(QObject *parent = 0);
    ~ZwaveManager();

    QString libraryVersion() const;
    QString driverPath() const;
    bool driverReady() const;

    bool init();
    bool addDriver(const QString &driverPath = "/dev/ttyACM0");
    bool removeDriver(const QString &driverPath = "/dev/ttyACM0");
    void disable();

    QList<ZwaveNode *> nodes() const;
    ZwaveNode *getNode(const quint8 &nodeId);

    bool pressButton(const quint8 &nodeId, const ValueID &valueId);
    bool releaseButton(const quint8 &nodeId, const ValueID &valueId);

signals:
    void driverReadyChanged(uint32 homeId);
    void initialized();
    void nodeDiscoveryFinished();

    void nodeAdded(ZwaveNode *node);
    void noderRemoved(const quint8 &nodeId);

private:
    Manager *m_manager = nullptr;

    bool m_initialized;

    quint32 m_homeId;

    QList<ZwaveNode *> m_nodes;

    bool serialPortAvailable(const QString &driverPath) const;
    ZwaveNode *getNode(const Notification *notification);

    QVariant getValue(const ValueID &valueId);

    static void onNotification(const Notification *notification, void* context);
    QString valueTypeToString(const ValueID &valueId);

};

#endif // ZWAVEMANAGER_H
