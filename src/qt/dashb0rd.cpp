// Copyright (c) 2026
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "dashb0rd.h"

#include "dashb0rdpage.h"

#include <QVBoxLayout>

Dashb0rd::Dashb0rd(const PlatformStyle* platformStyle, QWidget* parent)
    : QWidget(parent),
      m_platformStyle(platformStyle),
      m_page(nullptr)
{
    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // FIX: pass PlatformStyle first, parent second
    m_page = new Dashb0rdPage(m_platformStyle, this);
    root->addWidget(m_page);
}

Dashb0rd::~Dashb0rd()
{
}

void Dashb0rd::setClientModel(ClientModel* model)
{
    if (m_page) m_page->setClientModel(model);
}

void Dashb0rd::setWalletModel(WalletModel* model)
{
    if (m_page) m_page->setWalletModel(model);
}
