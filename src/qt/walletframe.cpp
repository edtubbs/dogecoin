// Copyright (c) 2011-2016 The Bitcoin Core developers
// Copyright (c) 2021-2022 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "walletframe.h"

#include "bitcoingui.h"
#include "dashb0rd.h"
#include "walletview.h"

#include <cstdio>
#include <iostream>

#include <QHBoxLayout>
#include <QLabel>

WalletFrame::WalletFrame(const PlatformStyle *_platformStyle, BitcoinGUI *_gui) :
    QFrame(_gui),
    gui(_gui),
    platformStyle(_platformStyle),
    dashb0rd(nullptr)
{
    // Leave HBox hook for adding a list view later
    QHBoxLayout *walletFrameLayout = new QHBoxLayout(this);
    setContentsMargins(0,0,0,0);
    walletStack = new QStackedWidget(this);
    walletFrameLayout->setContentsMargins(0,0,0,0);
    walletFrameLayout->addWidget(walletStack);

    QLabel *noWallet = new QLabel(tr("No wallet has been loaded."));
    noWallet->setAlignment(Qt::AlignCenter);
    walletStack->addWidget(noWallet);
    
    // Create dashboard widget
    dashb0rd = new Dashb0rd(platformStyle, this);
    walletStack->addWidget(dashb0rd);
}

WalletFrame::~WalletFrame()
{
}

void WalletFrame::setClientModel(ClientModel *_clientModel)
{
    this->clientModel = _clientModel;
    
    // Set client model for dashboard
    if (dashb0rd) {
        dashb0rd->setClientModel(_clientModel);
    }
}

bool WalletFrame::addWallet(const QString& name, WalletModel *walletModel)
{
    if (!gui || !clientModel || !walletModel || mapWalletViews.count(name) > 0)
        return false;

    WalletView *walletView = new WalletView(platformStyle, this);
    walletView->setBitcoinGUI(gui);
    walletView->setClientModel(clientModel);
    walletView->setWalletModel(walletModel);
    walletView->showOutOfSyncWarning(bOutOfSync);

     /* TODO we should goto the currently selected page once dynamically adding wallets is supported */
    walletView->gotoOverviewPage();
    walletStack->addWidget(walletView);
    mapWalletViews[name] = walletView;

    // Ensure a walletView is able to show the main window
    connect(walletView, SIGNAL(showNormalIfMinimized()), gui, SLOT(showNormalIfMinimized()));

    connect(walletView, SIGNAL(outOfSyncWarningClicked()), this, SLOT(outOfSyncWarningClicked()));

    return true;
}

bool WalletFrame::setCurrentWallet(const QString& name)
{
    if (mapWalletViews.count(name) == 0)
        return false;

    WalletView *walletView = mapWalletViews.value(name);
    walletStack->setCurrentWidget(walletView);
    walletView->updateEncryptionStatus();
    return true;
}

bool WalletFrame::removeWallet(const QString &name)
{
    if (mapWalletViews.count(name) == 0)
        return false;

    WalletView *walletView = mapWalletViews.take(name);
    walletStack->removeWidget(walletView);
    return true;
}

void WalletFrame::removeAllWallets()
{
    QMap<QString, WalletView*>::const_iterator i;
    for (i = mapWalletViews.constBegin(); i != mapWalletViews.constEnd(); ++i)
        walletStack->removeWidget(i.value());
    mapWalletViews.clear();
}

bool WalletFrame::handlePaymentRequest(const SendCoinsRecipient &recipient)
{
    WalletView *walletView = qobject_cast<WalletView*>(walletStack->currentWidget());
    if (!walletView) {
        QMap<QString, WalletView*>::const_iterator it = mapWalletViews.constBegin();
        if (it != mapWalletViews.constEnd())
            walletView = it.value();
    }
    if (!walletView)
        return false;

    return walletView->handlePaymentRequest(recipient);
}

void WalletFrame::showOutOfSyncWarning(bool fShow)
{
    bOutOfSync = fShow;
    QMap<QString, WalletView*>::const_iterator i;
    for (i = mapWalletViews.constBegin(); i != mapWalletViews.constEnd(); ++i)
        i.value()->showOutOfSyncWarning(fShow);
}

void WalletFrame::gotoOverviewPage()
{
    WalletView *walletView = qobject_cast<WalletView*>(walletStack->currentWidget());
    if (!walletView) {
        QMap<QString, WalletView*>::const_iterator it = mapWalletViews.constBegin();
        if (it != mapWalletViews.constEnd())
            walletView = it.value();
    }
    if (walletView) {
        walletView->gotoOverviewPage();
    }
}

void WalletFrame::gotoHistoryPage()
{
    WalletView *walletView = qobject_cast<WalletView*>(walletStack->currentWidget());
    if (!walletView) {
        QMap<QString, WalletView*>::const_iterator it = mapWalletViews.constBegin();
        if (it != mapWalletViews.constEnd())
            walletView = it.value();
    }
    if (walletView) {
        walletView->gotoHistoryPage();
    }
}

void WalletFrame::gotoReceiveCoinsPage()
{
    WalletView *walletView = qobject_cast<WalletView*>(walletStack->currentWidget());
    if (!walletView) {
        QMap<QString, WalletView*>::const_iterator it = mapWalletViews.constBegin();
        if (it != mapWalletViews.constEnd())
            walletView = it.value();
    }
    if (walletView) {
        walletView->gotoReceiveCoinsPage();
    }
}

void WalletFrame::gotoSendCoinsPage(QString addr)
{
    WalletView *walletView = qobject_cast<WalletView*>(walletStack->currentWidget());
    if (!walletView) {
        QMap<QString, WalletView*>::const_iterator it = mapWalletViews.constBegin();
        if (it != mapWalletViews.constEnd())
            walletView = it.value();
    }
    if (walletView) {
        walletView->gotoSendCoinsPage(addr);
    }
}

void WalletFrame::gotoSignMessageTab(QString addr)
{
    WalletView *walletView = qobject_cast<WalletView*>(walletStack->currentWidget());
    if (!walletView) {
        QMap<QString, WalletView*>::const_iterator it = mapWalletViews.constBegin();
        if (it != mapWalletViews.constEnd())
            walletView = it.value();
    }
    if (walletView)
        walletView->gotoSignMessageTab(addr);
}

void WalletFrame::gotoVerifyMessageTab(QString addr)
{
    WalletView *walletView = qobject_cast<WalletView*>(walletStack->currentWidget());
    if (!walletView) {
        QMap<QString, WalletView*>::const_iterator it = mapWalletViews.constBegin();
        if (it != mapWalletViews.constEnd())
            walletView = it.value();
    }
    if (walletView)
        walletView->gotoVerifyMessageTab(addr);
}

void WalletFrame::gotoDashb0rdPage()
{
    if (dashb0rd)
        walletStack->setCurrentWidget(dashb0rd);
}

void WalletFrame::encryptWallet(bool status)
{
    WalletView *walletView = qobject_cast<WalletView*>(walletStack->currentWidget());
    if (!walletView) {
        QMap<QString, WalletView*>::const_iterator it = mapWalletViews.constBegin();
        if (it != mapWalletViews.constEnd())
            walletView = it.value();
    }
    if (walletView)
        walletView->encryptWallet(status);
}

void WalletFrame::backupWallet()
{
    WalletView *walletView = qobject_cast<WalletView*>(walletStack->currentWidget());
    if (!walletView) {
        QMap<QString, WalletView*>::const_iterator it = mapWalletViews.constBegin();
        if (it != mapWalletViews.constEnd())
            walletView = it.value();
    }
    if (walletView)
        walletView->backupWallet();
}

void WalletFrame::changePassphrase()
{
    WalletView *walletView = qobject_cast<WalletView*>(walletStack->currentWidget());
    if (!walletView) {
        QMap<QString, WalletView*>::const_iterator it = mapWalletViews.constBegin();
        if (it != mapWalletViews.constEnd())
            walletView = it.value();
    }
    if (walletView)
        walletView->changePassphrase();
}

void WalletFrame::unlockWallet()
{
    WalletView *walletView = qobject_cast<WalletView*>(walletStack->currentWidget());
    if (!walletView) {
        QMap<QString, WalletView*>::const_iterator it = mapWalletViews.constBegin();
        if (it != mapWalletViews.constEnd())
            walletView = it.value();
    }
    if (walletView)
        walletView->unlockWallet();
}

void WalletFrame::printPaperWallets()
{
    WalletView *walletView = qobject_cast<WalletView*>(walletStack->currentWidget());
    if (!walletView) {
        QMap<QString, WalletView*>::const_iterator it = mapWalletViews.constBegin();
        if (it != mapWalletViews.constEnd())
            walletView = it.value();
    }
    if (walletView)
        walletView->printPaperWallets();
}

void WalletFrame::importPrivateKey()
{
    WalletView *walletView = qobject_cast<WalletView*>(walletStack->currentWidget());
    if (!walletView) {
        QMap<QString, WalletView*>::const_iterator it = mapWalletViews.constBegin();
        if (it != mapWalletViews.constEnd())
            walletView = it.value();
    }
    if (walletView)
        walletView->importPrivateKey();
}

void WalletFrame::usedSendingAddresses()
{
    WalletView *walletView = qobject_cast<WalletView*>(walletStack->currentWidget());
    if (!walletView) {
        QMap<QString, WalletView*>::const_iterator it = mapWalletViews.constBegin();
        if (it != mapWalletViews.constEnd())
            walletView = it.value();
    }
    if (walletView)
        walletView->usedSendingAddresses();
}

void WalletFrame::usedReceivingAddresses()
{
    WalletView *walletView = qobject_cast<WalletView*>(walletStack->currentWidget());
    if (!walletView) {
        QMap<QString, WalletView*>::const_iterator it = mapWalletViews.constBegin();
        if (it != mapWalletViews.constEnd())
            walletView = it.value();
    }
    if (walletView)
        walletView->usedReceivingAddresses();
}

WalletView *WalletFrame::currentWalletView()
{
    return qobject_cast<WalletView*>(walletStack->currentWidget());
}

void WalletFrame::outOfSyncWarningClicked()
{
    Q_EMIT requestedSyncWarningInfo();
}
