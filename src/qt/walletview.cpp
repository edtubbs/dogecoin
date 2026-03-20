// Copyright (c) 2011-2016 The Bitcoin Core developers
// Copyright (c) 2021-2022 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "walletview.h"

#include "addressbookpage.h"
#include "askpassphrasedialog.h"
#include "bitcoingui.h"
#include "clientmodel.h"
#include "guiutil.h"
#include "importkeysdialog.h"
#include "optionsmodel.h"
#include "overviewpage.h"
#include "platformstyle.h"
#include "receivecoinsdialog.h"
#include "sendcoinsdialog.h"
#include "signverifymessagedialog.h"
#include "transactiontablemodel.h"
#include "transactionview.h"
#include "walletmodel.h"
#include "utilitydialog.h"

#include "ui_interface.h"
#include "wallet/crypter.h"

#include "crypto/sha256.h"
#include "random.h"
#include "support/cleanse.h"

#include <QAction>
#include <QActionGroup>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QFile>
#include <QProgressDialog>
#include <QPushButton>
#include <QVBoxLayout>

WalletView::WalletView(const PlatformStyle *_platformStyle, QWidget *parent):
    QStackedWidget(parent),
    clientModel(0),
    walletModel(0),
    platformStyle(_platformStyle)
{
    // Create tabs
    overviewPage = new OverviewPage(platformStyle);

    transactionsPage = new QWidget(this);
    QVBoxLayout *vbox = new QVBoxLayout();
    QHBoxLayout *hbox_buttons = new QHBoxLayout();
    transactionView = new TransactionView(platformStyle, this);
    vbox->addWidget(transactionView);
    QPushButton *exportButton = new QPushButton(tr("&Export"), this);
    exportButton->setToolTip(tr("Export the data in the current tab to a file"));
    if (platformStyle->getImagesOnButtons()) {
        exportButton->setIcon(platformStyle->SingleColorIcon(":/icons/export"));
    }
    hbox_buttons->addStretch();
    hbox_buttons->addWidget(exportButton);
    vbox->addLayout(hbox_buttons);
    transactionsPage->setLayout(vbox);

    receiveCoinsPage = new ReceiveCoinsDialog(platformStyle);
    sendCoinsPage = new SendCoinsDialog(platformStyle);

    usedSendingAddressesPage = new AddressBookPage(platformStyle, AddressBookPage::ForEditing, AddressBookPage::SendingTab, this);
    usedReceivingAddressesPage = new AddressBookPage(platformStyle, AddressBookPage::ForEditing, AddressBookPage::ReceivingTab, this);

    addWidget(overviewPage);
    addWidget(transactionsPage);
    addWidget(receiveCoinsPage);
    addWidget(sendCoinsPage);

    importKeysDialog = new ImportKeysDialog(platformStyle);

    // Clicking on a transaction on the overview pre-selects the transaction on the transaction history page
    connect(overviewPage, SIGNAL(transactionClicked(QModelIndex)), transactionView, SLOT(focusTransaction(QModelIndex)));
    connect(overviewPage, SIGNAL(outOfSyncWarningClicked()), this, SLOT(requestedSyncWarningInfo()));

    // Double-clicking on a transaction on the transaction history page shows details
    connect(transactionView, SIGNAL(doubleClicked(QModelIndex)), transactionView, SLOT(showDetails()));

    // Clicking on "Export" allows to export the transaction list
    connect(exportButton, SIGNAL(clicked()), transactionView, SLOT(exportClicked()));

    // Pass through messages from sendCoinsPage
    connect(sendCoinsPage, SIGNAL(message(QString,QString,unsigned int)), this, SIGNAL(message(QString,QString,unsigned int)));
    // Pass through messages from transactionView
    connect(transactionView, SIGNAL(message(QString,QString,unsigned int)), this, SIGNAL(message(QString,QString,unsigned int)));
}

WalletView::~WalletView()
{
}

void WalletView::setBitcoinGUI(BitcoinGUI *gui)
{
    if (gui)
    {
        // Clicking on a transaction on the overview page simply sends you to transaction history page
        connect(overviewPage, SIGNAL(transactionClicked(QModelIndex)), gui, SLOT(gotoHistoryPage()));

        // Receive and report messages
        connect(this, SIGNAL(message(QString,QString,unsigned int)), gui, SLOT(message(QString,QString,unsigned int)));

        // Pass through encryption status changed signals
        connect(this, SIGNAL(encryptionStatusChanged(int)), gui, SLOT(setEncryptionStatus(int)));

        // Pass through transaction notifications
        connect(this, SIGNAL(incomingTransaction(QString,int,CAmount,QString,QString,QString)), gui, SLOT(incomingTransaction(QString,int,CAmount,QString,QString,QString)));

        // Connect HD enabled state signal 
        connect(this, SIGNAL(hdEnabledStatusChanged(int)), gui, SLOT(setHDStatus(int)));
    }
}

void WalletView::setClientModel(ClientModel *_clientModel)
{
    this->clientModel = _clientModel;

    overviewPage->setClientModel(_clientModel);
    sendCoinsPage->setClientModel(_clientModel);
}

void WalletView::setWalletModel(WalletModel *_walletModel)
{
    this->walletModel = _walletModel;

    // Put transaction list in tabs
    transactionView->setModel(_walletModel);
    overviewPage->setWalletModel(_walletModel);
    receiveCoinsPage->setModel(_walletModel);
    sendCoinsPage->setModel(_walletModel);
    usedReceivingAddressesPage->setModel(_walletModel->getAddressTableModel());
    usedSendingAddressesPage->setModel(_walletModel->getAddressTableModel());

    if (_walletModel)
    {
        // Receive and pass through messages from wallet model
        connect(_walletModel, SIGNAL(message(QString,QString,unsigned int)), this, SIGNAL(message(QString,QString,unsigned int)));

        // Handle changes in encryption status
        connect(_walletModel, SIGNAL(encryptionStatusChanged(int)), this, SIGNAL(encryptionStatusChanged(int)));
        updateEncryptionStatus();

        // update HD status
        Q_EMIT hdEnabledStatusChanged(_walletModel->hdEnabled());

        // Balloon pop-up for new transaction
        connect(_walletModel->getTransactionTableModel(), SIGNAL(rowsInserted(QModelIndex,int,int)),
                this, SLOT(processNewTransaction(QModelIndex,int,int)));

        // Ask for passphrase if needed
        connect(_walletModel, SIGNAL(requireUnlock()), this, SLOT(unlockWallet()));

        // Show progress dialog
        connect(_walletModel, SIGNAL(showProgress(QString,int)), this, SLOT(showProgress(QString,int)));
    }
}

void WalletView::processNewTransaction(const QModelIndex& parent, int start, int /*end*/)
{
    // Prevent balloon-spam when initial block download is in progress
    if (!walletModel || !clientModel || clientModel->inInitialBlockDownload())
        return;

    TransactionTableModel *ttm = walletModel->getTransactionTableModel();
    if (!ttm || ttm->processingQueuedTransactions())
        return;

    QString date = ttm->index(start, TransactionTableModel::Date, parent).data().toString();
    qint64 amount = ttm->index(start, TransactionTableModel::Amount, parent).data(Qt::EditRole).toULongLong();
    QString type = ttm->index(start, TransactionTableModel::Type, parent).data().toString();
    QModelIndex index = ttm->index(start, 0, parent);
    QString address = ttm->data(index, TransactionTableModel::AddressRole).toString();
    QString label = ttm->data(index, TransactionTableModel::LabelRole).toString();

    Q_EMIT incomingTransaction(date, walletModel->getOptionsModel()->getDisplayUnit(), amount, type, address, label);
}

void WalletView::gotoOverviewPage()
{
    setCurrentWidget(overviewPage);
}

void WalletView::gotoHistoryPage()
{
    setCurrentWidget(transactionsPage);
}

void WalletView::gotoReceiveCoinsPage()
{
    setCurrentWidget(receiveCoinsPage);
}

void WalletView::gotoSendCoinsPage(QString addr)
{
    setCurrentWidget(sendCoinsPage);

    if (!addr.isEmpty())
        sendCoinsPage->setAddress(addr);
}

void WalletView::gotoSignMessageTab(QString addr)
{
    // calls show() in showTab_SM()
    SignVerifyMessageDialog *signVerifyMessageDialog = new SignVerifyMessageDialog(platformStyle, this);
    signVerifyMessageDialog->setAttribute(Qt::WA_DeleteOnClose);
    signVerifyMessageDialog->setModel(walletModel);
    signVerifyMessageDialog->showTab_SM(true);

    if (!addr.isEmpty())
        signVerifyMessageDialog->setAddress_SM(addr);
}

void WalletView::gotoVerifyMessageTab(QString addr)
{
    // calls show() in showTab_VM()
    SignVerifyMessageDialog *signVerifyMessageDialog = new SignVerifyMessageDialog(platformStyle, this);
    signVerifyMessageDialog->setAttribute(Qt::WA_DeleteOnClose);
    signVerifyMessageDialog->setModel(walletModel);
    signVerifyMessageDialog->showTab_VM(true);

    if (!addr.isEmpty())
        signVerifyMessageDialog->setAddress_VM(addr);
}

void WalletView::gotoImportKeysDialog()
{
    setCurrentWidget(importKeysDialog);
}

bool WalletView::handlePaymentRequest(const SendCoinsRecipient& recipient)
{
    return sendCoinsPage->handlePaymentRequest(recipient);
}

void WalletView::showOutOfSyncWarning(bool fShow)
{
    overviewPage->showOutOfSyncWarning(fShow);
}

void WalletView::updateEncryptionStatus()
{
    Q_EMIT encryptionStatusChanged(walletModel->getEncryptionStatus());
}

void WalletView::encryptWallet(bool status)
{
    if(!walletModel)
        return;
    AskPassphraseDialog dlg(status ? AskPassphraseDialog::Encrypt : AskPassphraseDialog::Decrypt, this);
    dlg.setModel(walletModel);
    dlg.exec();

    updateEncryptionStatus();
}

void WalletView::backupWallet()
{
    QString filename = GUIUtil::getSaveFileName(this,
        tr("Backup Wallet"), QString(),
        tr("Wallet Data (*.dat)"), NULL);

    if (filename.isEmpty())
        return;

    if (!walletModel->backupWallet(filename)) {
        Q_EMIT message(tr("Backup Failed"), tr("There was an error trying to save the wallet data to %1.").arg(filename),
            CClientUIInterface::MSG_ERROR);
        }
    else {
        Q_EMIT message(tr("Backup Successful"), tr("The wallet data was successfully saved to %1.").arg(filename),
            CClientUIInterface::MSG_INFORMATION);
    }
}

void WalletView::backupWalletEncrypted()
{
    QString walletFilename = GUIUtil::getSaveFileName(this,
        tr("Backup Wallet"), QString(),
        tr("Wallet Data (*.dat)"), NULL);

    if (walletFilename.isEmpty())
        return;

    if (!walletModel->backupWallet(walletFilename)) {
        Q_EMIT message(tr("Backup Failed"), tr("There was an error trying to save the wallet data to %1.").arg(walletFilename),
            CClientUIInterface::MSG_ERROR);
        return;
    }

    bool ok = false;
    QString passphrase = QInputDialog::getText(
        this,
        tr("PQC Envelope Passphrase"),
        tr("Enter passphrase to encrypt wallet backup:"),
        QLineEdit::Password,
        QString(),
        &ok);
    if (!ok || passphrase.isEmpty()) {
        QFile::remove(walletFilename);
        return;
    }

    QFile walletFile(walletFilename);
    if (!walletFile.open(QIODevice::ReadOnly)) {
        Q_EMIT message(tr("Backup Failed"), tr("Unable to open wallet backup file %1 for encryption.").arg(walletFilename),
            CClientUIInterface::MSG_ERROR);
        QFile::remove(walletFilename);
        return;
    }
    QByteArray plain = walletFile.readAll();
    walletFile.close();
    if (plain.isEmpty()) {
        QFile::remove(walletFilename);
        Q_EMIT message(tr("Encryption Failed"), tr("Wallet backup file %1 is empty.").arg(walletFilename),
            CClientUIInterface::MSG_ERROR);
        return;
    }

    std::vector<unsigned char> salt(WALLET_CRYPTO_SALT_SIZE);
    GetStrongRandBytes(salt.data(), WALLET_CRYPTO_SALT_SIZE);
    CCrypter crypter;
    std::string passphraseStd = passphrase.toStdString();
    SecureString pass(passphraseStd.begin(), passphraseStd.end());
    if (!passphraseStd.empty()) {
        memory_cleanse(&passphraseStd[0], passphraseStd.size());
    }
    if (!crypter.SetKeyFromPassphrase(pass, salt, 25000, 0)) {
        if (!plain.isEmpty()) {
            memory_cleanse(plain.data(), plain.size());
        }
        QFile::remove(walletFilename);
        Q_EMIT message(tr("Encryption Failed"), tr("Unable to derive encryption key for wallet backup."),
            CClientUIInterface::MSG_ERROR);
        return;
    }

    CKeyingMaterial plainMaterial(reinterpret_cast<const unsigned char*>(plain.constData()),
                                  reinterpret_cast<const unsigned char*>(plain.constData()) + plain.size());
    std::vector<unsigned char> cipher;
    if (!crypter.Encrypt(plainMaterial, cipher)) {
        if (!plain.isEmpty()) {
            memory_cleanse(plain.data(), plain.size());
        }
        QFile::remove(walletFilename);
        Q_EMIT message(tr("Encryption Failed"), tr("Unable to encrypt wallet backup."),
            CClientUIInterface::MSG_ERROR);
        return;
    }

    const QByteArray algo = "AES256-CBC+PQC-FALCON-LABEL";
    CSHA256 hasher;
    hasher.Write(reinterpret_cast<const unsigned char*>(algo.constData()), algo.size());
    hasher.Write(reinterpret_cast<const unsigned char*>(salt.data()), salt.size());
    hasher.Write(reinterpret_cast<const unsigned char*>(cipher.data()), cipher.size());
    unsigned char digest[CSHA256::OUTPUT_SIZE];
    hasher.Finalize(digest);

    QString outFilename = walletFilename + ".pqce";
    QFile outFile(outFilename);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        memory_cleanse(digest, sizeof(digest));
        if (!plain.isEmpty()) {
            memory_cleanse(plain.data(), plain.size());
        }
        QFile::remove(walletFilename);
        Q_EMIT message(tr("Encryption Failed"), tr("Unable to write encrypted wallet backup %1.").arg(outFilename),
            CClientUIInterface::MSG_ERROR);
        return;
    }

    outFile.write("DGC-PQCE1\n");
    outFile.write("ALGO:");
    outFile.write(algo);
    outFile.write("\n");
    outFile.write("SALT:");
    outFile.write(QByteArray(reinterpret_cast<const char*>(salt.data()), salt.size()).toHex());
    outFile.write("\n");
    outFile.write("HASH:");
    outFile.write(QByteArray(reinterpret_cast<const char*>(digest), sizeof(digest)).toHex());
    outFile.write("\n");
    outFile.write("DATA:");
    outFile.write(QByteArray(reinterpret_cast<const char*>(cipher.data()), cipher.size()).toBase64());
    outFile.write("\n");
    outFile.close();

    memory_cleanse(digest, sizeof(digest));
    if (!plain.isEmpty()) {
        memory_cleanse(plain.data(), plain.size());
    }
    QFile::remove(walletFilename);

    Q_EMIT message(tr("Backup Successful"), tr("The encrypted wallet backup was successfully saved to %1.").arg(outFilename),
        CClientUIInterface::MSG_INFORMATION);
}

void WalletView::changePassphrase()
{
    AskPassphraseDialog dlg(AskPassphraseDialog::ChangePass, this);
    dlg.setModel(walletModel);
    dlg.exec();
}

void WalletView::unlockWallet()
{
    if(!walletModel)
        return;
    // Unlock wallet when requested by wallet model
    if (walletModel->getEncryptionStatus() == WalletModel::Locked)
    {
        AskPassphraseDialog dlg(AskPassphraseDialog::Unlock, this);
        dlg.setModel(walletModel);
        dlg.exec();
    }
}

void WalletView::usedSendingAddresses()
{
    if(!walletModel)
        return;

    usedSendingAddressesPage->show();
    usedSendingAddressesPage->raise();
    usedSendingAddressesPage->activateWindow();
}

void WalletView::usedReceivingAddresses()
{
    if(!walletModel)
        return;

    usedReceivingAddressesPage->show();
    usedReceivingAddressesPage->raise();
    usedReceivingAddressesPage->activateWindow();
}

void WalletView::importPrivateKey()
{
    if(!walletModel)
        return;

    importKeysDialog->show();
    importKeysDialog->raise();
    importKeysDialog->activateWindow();
}

void WalletView::showProgress(const QString &title, int nProgress)
{
    if (nProgress == 0)
    {
        progressDialog = new QProgressDialog(title, "", 0, 100);
        progressDialog->setWindowModality(Qt::ApplicationModal);
        progressDialog->setMinimumDuration(0);
        progressDialog->setCancelButton(0);
        progressDialog->setAutoClose(false);
        progressDialog->setValue(0);
    }
    else if (nProgress == 100)
    {
        if (progressDialog)
        {
            progressDialog->close();
            progressDialog->deleteLater();
        }
    }
    else if (progressDialog)
        progressDialog->setValue(nProgress);
}

void WalletView::requestedSyncWarningInfo()
{
    Q_EMIT outOfSyncWarningClicked();
}

void WalletView::printPaperWallets()
{
    if(!walletModel)
        return;

    PaperWalletDialog dlg(this);
    dlg.setModel(walletModel);
    dlg.setClientModel(clientModel);
    dlg.exec();
}
