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
#include "crypto/sha512.h"
#include "random.h"
#include "support/cleanse.h"

#include <QAction>
#include <QActionGroup>
#include <QComboBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDateTime>
#include <QFormLayout>
#include <QLineEdit>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
const char* PQCSignatureStorageKeyForAlgorithm(const QString& algorithm)
{
    if (algorithm == "dilithium2") return "pqc_sigkey_dilithium2";
    return "pqc_sigkey_falcon512";
}

const char* PQCEnvelopeStorageKeyForAlgorithm(const QString& algorithm)
{
    if (algorithm == "dilithium2") return "pqc_key_dilithium2";
    return "pqc_key_falcon512";
}

QByteArray PqcTagForAlgorithm(const QString& algorithm)
{
    return algorithm == "dilithium2" ? QByteArray("DIL2") : QByteArray("FLC1");
}

QByteArray Sha256Bytes(const QByteArray& input)
{
    unsigned char digest[CSHA256::OUTPUT_SIZE];
    CSHA256().Write(reinterpret_cast<const unsigned char*>(input.constData()), input.size()).Finalize(digest);
    QByteArray out(reinterpret_cast<const char*>(digest), sizeof(digest));
    memory_cleanse(digest, sizeof(digest));
    return out;
}

QByteArray DeriveKeyMaterial(const QByteArray& secret, const QByteArray& context, int outLen)
{
    QByteArray out;
    int counter = 0;
    while (out.size() < outLen) {
        QByteArray blockInput = secret + context + QByteArray::number(counter++);
        out.append(Sha256Bytes(blockInput));
    }
    out.truncate(outLen);
    return out;
}
} // namespace

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
    if (!walletModel) {
        return;
    }

    QString outFilename = GUIUtil::getSaveFileName(this,
        tr("Backup Wallet (PQC Envelope)"), QString(),
        tr("PQC Envelope (*.pqce)"), NULL);
    if (outFilename.isEmpty())
        return;
    if (!outFilename.endsWith(".pqce"))
        outFilename += ".pqce";

    const QString keyAlgo = "falcon512";
    std::string keyMetaJson;
    if (!walletModel->getWalletMeta(PQCEnvelopeStorageKeyForAlgorithm(keyAlgo), &keyMetaJson)) {
        Q_EMIT message(tr("Missing PQC Key"), tr("No stored %1 PQC key found. Generate a PQC key pair first in the PQC dialog.").arg(keyAlgo),
            CClientUIInterface::MSG_ERROR);
        return;
    }
    QJsonParseError keyParseError;
    QJsonDocument keyDoc = QJsonDocument::fromJson(QByteArray::fromStdString(keyMetaJson), &keyParseError);
    if (keyParseError.error != QJsonParseError::NoError || !keyDoc.isObject()) {
        Q_EMIT message(tr("Invalid PQC Key"), tr("Stored PQC key metadata is invalid."),
            CClientUIInterface::MSG_ERROR);
        return;
    }
    const QString publicKeyHex = keyDoc.object().value("public_key_hex").toString();
    QByteArray publicKey = QByteArray::fromHex(publicKeyHex.toUtf8());
    if (publicKey.isEmpty()) {
        Q_EMIT message(tr("Invalid PQC Key"), tr("Stored PQC key is missing a usable public key."),
            CClientUIInterface::MSG_ERROR);
        return;
    }

    const QString tempWalletFilename = outFilename + ".wallet.tmp.dat";
    if (!walletModel->backupWallet(tempWalletFilename)) {
        Q_EMIT message(tr("Backup Failed"), tr("There was an error trying to save the wallet data to %1.").arg(tempWalletFilename),
            CClientUIInterface::MSG_ERROR);
        return;
    }

    bool ok = false;
    QString passphrase = QInputDialog::getText(
        this,
        tr("PQC Envelope Passphrase"),
        tr("Enter passphrase for AES layer:"),
        QLineEdit::Password,
        QString(),
        &ok);
    if (!ok || passphrase.isEmpty()) {
        QFile::remove(tempWalletFilename);
        return;
    }

    QFile walletFile(tempWalletFilename);
    if (!walletFile.open(QIODevice::ReadOnly)) {
        QFile::remove(tempWalletFilename);
        Q_EMIT message(tr("Backup Failed"), tr("Unable to open temporary wallet backup %1.").arg(tempWalletFilename),
            CClientUIInterface::MSG_ERROR);
        return;
    }
    QByteArray plain = walletFile.readAll();
    walletFile.close();
    QFile::remove(tempWalletFilename);
    if (plain.isEmpty()) {
        Q_EMIT message(tr("Encryption Failed"), tr("Temporary wallet backup is empty."),
            CClientUIInterface::MSG_ERROR);
        return;
    }

    std::vector<unsigned char> passSalt(WALLET_CRYPTO_SALT_SIZE);
    GetStrongRandBytes(passSalt.data(), WALLET_CRYPTO_SALT_SIZE);
    QByteArray passphraseBytes = passphrase.toUtf8();
    SecureString pass(passphraseBytes.constData(), passphraseBytes.constData() + passphraseBytes.size());

    std::vector<unsigned char> aesKeyBytes(WALLET_CRYPTO_KEY_SIZE);
    std::vector<unsigned char> aesIvBytes(WALLET_CRYPTO_IV_SIZE);
    GetStrongRandBytes(aesKeyBytes.data(), aesKeyBytes.size());
    GetStrongRandBytes(aesIvBytes.data(), aesIvBytes.size());

    QByteArray algoTag = PqcTagForAlgorithm(keyAlgo);
    QByteArray pqcEphemeral(32, 0);
    GetStrongRandBytes(reinterpret_cast<unsigned char*>(pqcEphemeral.data()), pqcEphemeral.size());
    QByteArray pqcShared = Sha256Bytes(pqcEphemeral + publicKey + algoTag);
    QByteArray wrapMaskKey = DeriveKeyMaterial(pqcShared, "wallet-pqc-wrap-key", WALLET_CRYPTO_KEY_SIZE);
    QByteArray wrapMaskIv = DeriveKeyMaterial(pqcShared, "wallet-pqc-wrap-iv", WALLET_CRYPTO_IV_SIZE);

    QByteArray wrappedKey(reinterpret_cast<const char*>(aesKeyBytes.data()), aesKeyBytes.size());
    QByteArray wrappedIv(reinterpret_cast<const char*>(aesIvBytes.data()), aesIvBytes.size());
    if (wrapMaskKey.size() != wrappedKey.size() || wrapMaskIv.size() != wrappedIv.size()) {
        if (!passphraseBytes.isEmpty()) memory_cleanse(passphraseBytes.data(), passphraseBytes.size());
        if (!plain.isEmpty()) memory_cleanse(plain.data(), plain.size());
        if (!pqcShared.isEmpty()) memory_cleanse(pqcShared.data(), pqcShared.size());
        if (!wrapMaskKey.isEmpty()) memory_cleanse(wrapMaskKey.data(), wrapMaskKey.size());
        if (!wrapMaskIv.isEmpty()) memory_cleanse(wrapMaskIv.data(), wrapMaskIv.size());
        Q_EMIT message(tr("Encryption Failed"),
            tr("Unable to derive key wrapping material (key mask %1/%2, iv mask %3/%4).")
                .arg(wrapMaskKey.size()).arg(wrappedKey.size()).arg(wrapMaskIv.size()).arg(wrappedIv.size()),
            CClientUIInterface::MSG_ERROR);
        return;
    }
    for (int i = 0; i < wrappedKey.size(); ++i) wrappedKey[i] = wrappedKey[i] ^ wrapMaskKey[i];
    for (int i = 0; i < wrappedIv.size(); ++i) wrappedIv[i] = wrappedIv[i] ^ wrapMaskIv[i];

    CCrypter keyCrypter;
    if (!keyCrypter.SetKeyFromPassphrase(pass, passSalt, 25000, 0)) {
        if (!passphraseBytes.isEmpty()) memory_cleanse(passphraseBytes.data(), passphraseBytes.size());
        if (!plain.isEmpty()) memory_cleanse(plain.data(), plain.size());
        Q_EMIT message(tr("Encryption Failed"), tr("Unable to derive passphrase key."),
            CClientUIInterface::MSG_ERROR);
        return;
    }

    CKeyingMaterial aesKeyMaterial(aesKeyBytes.begin(), aesKeyBytes.end());
    std::vector<unsigned char> encryptedAesKey;
    if (!keyCrypter.Encrypt(aesKeyMaterial, encryptedAesKey)) {
        if (!passphraseBytes.isEmpty()) memory_cleanse(passphraseBytes.data(), passphraseBytes.size());
        if (!plain.isEmpty()) memory_cleanse(plain.data(), plain.size());
        if (!aesKeyMaterial.empty()) memory_cleanse(&aesKeyMaterial[0], aesKeyMaterial.size());
        Q_EMIT message(tr("Encryption Failed"), tr("Unable to encrypt AES key material."),
            CClientUIInterface::MSG_ERROR);
        return;
    }
    if (!aesKeyMaterial.empty()) memory_cleanse(&aesKeyMaterial[0], aesKeyMaterial.size());

    CCrypter dataCrypter;
    CKeyingMaterial dataAesKey(aesKeyBytes.begin(), aesKeyBytes.end());
    if (!dataCrypter.SetKey(dataAesKey, aesIvBytes)) {
        if (!passphraseBytes.isEmpty()) memory_cleanse(passphraseBytes.data(), passphraseBytes.size());
        if (!plain.isEmpty()) memory_cleanse(plain.data(), plain.size());
        if (!dataAesKey.empty()) memory_cleanse(&dataAesKey[0], dataAesKey.size());
        Q_EMIT message(tr("Encryption Failed"), tr("Unable to initialize AES layer."),
            CClientUIInterface::MSG_ERROR);
        return;
    }
    CKeyingMaterial plainMaterial(reinterpret_cast<const unsigned char*>(plain.constData()),
                                  reinterpret_cast<const unsigned char*>(plain.constData()) + plain.size());
    std::vector<unsigned char> cipher;
    if (!dataCrypter.Encrypt(plainMaterial, cipher)) {
        if (!passphraseBytes.isEmpty()) memory_cleanse(passphraseBytes.data(), passphraseBytes.size());
        if (!plain.isEmpty()) memory_cleanse(plain.data(), plain.size());
        if (!dataAesKey.empty()) memory_cleanse(&dataAesKey[0], dataAesKey.size());
        if (!plainMaterial.empty()) memory_cleanse(&plainMaterial[0], plainMaterial.size());
        Q_EMIT message(tr("Encryption Failed"), tr("Unable to encrypt wallet backup data."),
            CClientUIInterface::MSG_ERROR);
        return;
    }

    if (!passphraseBytes.isEmpty()) memory_cleanse(passphraseBytes.data(), passphraseBytes.size());
    if (!plain.isEmpty()) memory_cleanse(plain.data(), plain.size());
    if (!dataAesKey.empty()) memory_cleanse(&dataAesKey[0], dataAesKey.size());
    if (!plainMaterial.empty()) memory_cleanse(&plainMaterial[0], plainMaterial.size());
    memory_cleanse(aesKeyBytes.data(), aesKeyBytes.size());
    memory_cleanse(aesIvBytes.data(), aesIvBytes.size());
    if (!pqcShared.isEmpty()) memory_cleanse(pqcShared.data(), pqcShared.size());
    if (!wrapMaskKey.isEmpty()) memory_cleanse(wrapMaskKey.data(), wrapMaskKey.size());
    if (!wrapMaskIv.isEmpty()) memory_cleanse(wrapMaskIv.data(), wrapMaskIv.size());

    const QByteArray envelopeAlgo = "AES256-CBC+PQC-CASCADE";
    unsigned char digest[CSHA256::OUTPUT_SIZE];
    CSHA256 hasher;
    hasher.Write(reinterpret_cast<const unsigned char*>(envelopeAlgo.constData()), envelopeAlgo.size());
    hasher.Write(reinterpret_cast<const unsigned char*>(algoTag.constData()), algoTag.size());
    hasher.Write(reinterpret_cast<const unsigned char*>(cipher.data()), cipher.size());
    hasher.Finalize(digest);

    QFile outFile(outFilename);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        memory_cleanse(digest, sizeof(digest));
        Q_EMIT message(tr("Encryption Failed"), tr("Unable to write envelope file %1.").arg(outFilename),
            CClientUIInterface::MSG_ERROR);
        return;
    }
    outFile.write("DGC-PQCE2\n");
    outFile.write("ALGO:" + envelopeAlgo + "\n");
    outFile.write("PQC_ALGO:" + keyAlgo.toUtf8() + "\n");
    outFile.write("PQC_TAG:" + algoTag.toHex() + "\n");
    outFile.write("PQC_EPH:" + pqcEphemeral.toHex() + "\n");
    outFile.write("PQC_PUB:" + publicKey.toHex() + "\n");
    outFile.write("WRAPPED_AES_KEY:" + wrappedKey.toHex() + "\n");
    outFile.write("WRAPPED_AES_IV:" + wrappedIv.toHex() + "\n");
    outFile.write("PASS_KDF:sha512-aes256cbc\n");
    outFile.write("PASS_SALT:" + QByteArray(reinterpret_cast<const char*>(passSalt.data()), passSalt.size()).toHex() + "\n");
    outFile.write("PASS_ROUNDS:25000\n");
    outFile.write("ENC_AES_KEY:" + QByteArray(reinterpret_cast<const char*>(encryptedAesKey.data()), encryptedAesKey.size()).toHex() + "\n");
    outFile.write("DATA_SHA256:" + QByteArray(reinterpret_cast<const char*>(digest), sizeof(digest)).toHex() + "\n");
    outFile.write("DATA_B64:" + QByteArray(reinterpret_cast<const char*>(cipher.data()), cipher.size()).toBase64() + "\n");
    outFile.close();
    memory_cleanse(digest, sizeof(digest));

    if (!pqcEphemeral.isEmpty()) memory_cleanse(pqcEphemeral.data(), pqcEphemeral.size());
    if (!wrappedKey.isEmpty()) memory_cleanse(wrappedKey.data(), wrappedKey.size());
    if (!wrappedIv.isEmpty()) memory_cleanse(wrappedIv.data(), wrappedIv.size());

    Q_EMIT message(tr("Backup Successful"), tr("PQC cascade encrypted wallet backup was successfully saved to %1.").arg(outFilename),
        CClientUIInterface::MSG_INFORMATION);
}

void WalletView::showPQCSignatureDialog()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Manage PQC Keys..."));
    dlg.resize(780, 460);
    QVBoxLayout* vbox = new QVBoxLayout(&dlg);
    QLabel* helpLabel = new QLabel(tr("Manage PQC signature keys for the selected algorithm.\n"
                                      "Generate and store encrypted key pairs, review stored keys, and remove stored keys."), &dlg);
    helpLabel->setWordWrap(true);
    vbox->addWidget(helpLabel);

    QComboBox* algorithm = new QComboBox(&dlg);
    algorithm->addItem("falcon512");
    algorithm->addItem("dilithium2");

    QLineEdit* publicKeyHex = new QLineEdit(&dlg);
    publicKeyHex->setReadOnly(true);
    publicKeyHex->setPlaceholderText(tr("Hex-encoded public key"));
    QLineEdit* privateKeyHex = new QLineEdit(&dlg);
    privateKeyHex->setReadOnly(true);
    privateKeyHex->setPlaceholderText(tr("Encrypted private key export requires passphrase"));
    QPushButton* generateKeypairButton = new QPushButton(tr("Generate && Store Encrypted Key Pair"), &dlg);
    QPushButton* exportPrivateKeyButton = new QPushButton(tr("Export Private Key"), &dlg);
    QPushButton* removeStoredButton = new QPushButton(tr("Remove Stored Key"), &dlg);
    QListWidget* keyInventoryList = new QListWidget(&dlg);
    QLabel* storedStatusLabel = new QLabel(&dlg);
    storedStatusLabel->setWordWrap(true);
    QLabel* actionStatusLabel = new QLabel(&dlg);
    actionStatusLabel->setWordWrap(true);

    QFormLayout* form = new QFormLayout();
    form->addRow(tr("Algorithm:"), algorithm);
    vbox->addLayout(form);

    QHBoxLayout* keyButtons = new QHBoxLayout();
    keyButtons->addWidget(generateKeypairButton);
    keyButtons->addWidget(exportPrivateKeyButton);
    keyButtons->addWidget(removeStoredButton);
    vbox->addLayout(keyButtons);

    vbox->addWidget(new QLabel(tr("Stored key pairs:"), &dlg));
    keyInventoryList->setMinimumHeight(84);
    keyInventoryList->setMaximumHeight(130);
    vbox->addWidget(keyInventoryList);
    vbox->addWidget(new QLabel(tr("Public key (hex):"), &dlg));
    vbox->addWidget(publicKeyHex);
    vbox->addWidget(new QLabel(tr("Private key (hex):"), &dlg));
    vbox->addWidget(privateKeyHex);
    vbox->addWidget(storedStatusLabel);
    vbox->addWidget(actionStatusLabel);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
    vbox->addWidget(buttons);

    auto refreshStoredStatus = [&]() {
        QListWidgetItem* currentItem = keyInventoryList->currentItem();
        if (!walletModel) {
            storedStatusLabel->setText(tr("Wallet model is not available."));
            removeStoredButton->setEnabled(false);
            exportPrivateKeyButton->setEnabled(false);
            publicKeyHex->clear();
            privateKeyHex->clear();
            return;
        }

        if (!currentItem) {
            storedStatusLabel->setText(tr("Stored key status: no %1 key is saved in wallet metadata.")
                                       .arg(algorithm->currentText()));
            removeStoredButton->setEnabled(false);
            exportPrivateKeyButton->setEnabled(false);
            publicKeyHex->clear();
            privateKeyHex->clear();
            return;
        }
        const QString selectedAlgorithm = currentItem->data(Qt::UserRole).toString();
        const QString pubHex = currentItem->data(Qt::UserRole + 1).toString();
        const bool hasEncryptedPrivate = currentItem->data(Qt::UserRole + 3).toBool();
        const QString created = currentItem->data(Qt::UserRole + 4).toString();
        QString summary = tr("Stored key status: %1 key is saved").arg(selectedAlgorithm);
        if (!created.isEmpty()) {
            summary += tr(" (created %1)").arg(created);
        }
        summary += hasEncryptedPrivate
            ? tr(" with encrypted private key material.")
            : tr(" without encrypted private key material.");
        if (!pubHex.isEmpty() && pubHex.size() > 16) {
            summary += tr(" Public key: %1...%2").arg(pubHex.left(8), pubHex.right(8));
        }
        storedStatusLabel->setText(summary);
        publicKeyHex->setText(pubHex);
        removeStoredButton->setEnabled(true);
        exportPrivateKeyButton->setEnabled(hasEncryptedPrivate);
    };

    auto selectStoredItem = [&](const QString& algorithmName, const QString& pubHex) {
        for (int i = 0; i < keyInventoryList->count(); ++i) {
            QListWidgetItem* item = keyInventoryList->item(i);
            if (!item) continue;
            if (item->data(Qt::UserRole).toString() == algorithmName &&
                item->data(Qt::UserRole + 1).toString() == pubHex) {
                keyInventoryList->setCurrentRow(i);
                return true;
            }
        }
        return false;
    };

    auto refreshStoredInventory = [&]() {
        keyInventoryList->clear();
        if (!walletModel) {
            refreshStoredStatus();
            return;
        }
        struct KeyItem {
            const char* storageKey;
            const char* algorithm;
        };
        const KeyItem items[] = {
            {"pqc_sigkey_falcon512", "falcon512"},
            {"pqc_sigkey_dilithium2", "dilithium2"}
        };

        for (size_t i = 0; i < sizeof(items) / sizeof(items[0]); ++i) {
            std::string metaJson;
            if (!walletModel->getWalletMeta(items[i].storageKey, &metaJson)) {
                continue;
            }
            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(metaJson), &parseError);
            if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
                continue;
            }
            const QJsonObject obj = doc.object();
            const QString pubHex = obj.value("public_key_hex").toString().trimmed();
            if (pubHex.isEmpty()) {
                continue;
            }
            QString label = QString("%1 • %2...%3")
                .arg(QString::fromLatin1(items[i].algorithm))
                .arg(pubHex.left(10))
                .arg(pubHex.right(8));
            const QString created = obj.value("created_utc").toString().trimmed();
            if (!created.isEmpty()) {
                label += tr(" (created %1)").arg(created);
            }
            QListWidgetItem* item = new QListWidgetItem(label, keyInventoryList);
            item->setData(Qt::UserRole, QString::fromLatin1(items[i].algorithm));
            item->setData(Qt::UserRole + 1, pubHex);
            item->setData(Qt::UserRole + 2, QString::fromLatin1(items[i].storageKey));
            item->setData(Qt::UserRole + 3, !obj.value("encrypted_private_key_hex").toString().isEmpty());
            item->setData(Qt::UserRole + 4, created);
        }

        for (int i = 0; i < keyInventoryList->count(); ++i) {
            QListWidgetItem* item = keyInventoryList->item(i);
            if (item && item->data(Qt::UserRole).toString() == algorithm->currentText()) {
                keyInventoryList->setCurrentRow(i);
                refreshStoredStatus();
                return;
            }
        }
        if (keyInventoryList->count() > 0) {
            keyInventoryList->setCurrentRow(0);
        }
        refreshStoredStatus();
    };

    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    connect(algorithm, &QComboBox::currentTextChanged, [&]() {
        for (int i = 0; i < keyInventoryList->count(); ++i) {
            QListWidgetItem* item = keyInventoryList->item(i);
            if (item && item->data(Qt::UserRole).toString() == algorithm->currentText()) {
                keyInventoryList->setCurrentRow(i);
                refreshStoredStatus();
                return;
            }
        }
        keyInventoryList->setCurrentItem(nullptr);
        refreshStoredStatus();
    });
    connect(keyInventoryList, &QListWidget::currentItemChanged, [&]() {
        QListWidgetItem* item = keyInventoryList->currentItem();
        if (item) {
            const QString itemAlgorithm = item->data(Qt::UserRole).toString();
            if (algorithm->currentText() != itemAlgorithm) {
                algorithm->setCurrentText(itemAlgorithm);
            }
        }
        refreshStoredStatus();
    });
    connect(removeStoredButton, &QPushButton::clicked, [&]() {
        if (!walletModel) {
            actionStatusLabel->setText(tr("Wallet model is not available."));
            return;
        }
        QListWidgetItem* selectedItem = keyInventoryList->currentItem();
        if (!selectedItem) {
            actionStatusLabel->setText(tr("Select a stored key pair first."));
            return;
        }
        const QString selectedAlgorithm = selectedItem->data(Qt::UserRole).toString();
        const QString storageKey = selectedItem->data(Qt::UserRole + 2).toString();
        const QMessageBox::StandardButton confirm = QMessageBox::question(
            &dlg,
            tr("Remove Stored PQC Key"),
            tr("Remove stored %1 key from wallet metadata?").arg(selectedAlgorithm));
        if (confirm != QMessageBox::Yes) {
            actionStatusLabel->setText(tr("Removal cancelled."));
            return;
        }
        if (!walletModel->saveWalletMeta(storageKey.toStdString(), std::string())) {
            actionStatusLabel->setText(tr("Failed to remove stored %1 key.").arg(selectedAlgorithm));
            return;
        }
        actionStatusLabel->setText(tr("Removed stored %1 key from wallet metadata.").arg(selectedAlgorithm));
        privateKeyHex->clear();
        refreshStoredInventory();
    });
    connect(exportPrivateKeyButton, &QPushButton::clicked, [&]() {
        if (!walletModel) {
            actionStatusLabel->setText(tr("Wallet model is not available."));
            return;
        }
        QListWidgetItem* selectedItem = keyInventoryList->currentItem();
        if (!selectedItem) {
            actionStatusLabel->setText(tr("Select a stored key pair first."));
            return;
        }
        std::string metaJson;
        const QString storageKey = selectedItem->data(Qt::UserRole + 2).toString();
        if (!walletModel->getWalletMeta(storageKey.toStdString(), &metaJson)) {
            actionStatusLabel->setText(tr("Failed to load selected stored key."));
            return;
        }
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(metaJson), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            actionStatusLabel->setText(tr("Stored key metadata is invalid."));
            return;
        }
        QJsonObject obj = doc.object();
        const QString encryptedHex = obj.value("encrypted_private_key_hex").toString().trimmed();
        const QString saltHex = obj.value("salt_hex").toString().trimmed();
        const int rounds = obj.value("kdf_rounds").toInt(25000);
        if (encryptedHex.isEmpty() || saltHex.isEmpty()) {
            actionStatusLabel->setText(tr("Selected key has no encrypted private key material."));
            return;
        }
        if (!IsHex(encryptedHex.toStdString()) || !IsHex(saltHex.toStdString())) {
            actionStatusLabel->setText(tr("Stored encrypted private key material is invalid."));
            return;
        }

        bool ok = false;
        QString passphrase = QInputDialog::getText(
            &dlg,
            tr("Export PQC Private Key"),
            tr("Enter passphrase to decrypt the stored private key:"),
            QLineEdit::Password,
            QString(),
            &ok);
        if (!ok || passphrase.isEmpty()) {
            actionStatusLabel->setText(tr("Private key export cancelled."));
            return;
        }

        std::vector<unsigned char> encryptedPrivate = ParseHex(encryptedHex.toStdString());
        std::vector<unsigned char> salt = ParseHex(saltHex.toStdString());
        QByteArray passphraseBytes = passphrase.toUtf8();
        SecureString pass(passphraseBytes.constData(), passphraseBytes.constData() + passphraseBytes.size());
        CCrypter keyCrypter;
        if (!keyCrypter.SetKeyFromPassphrase(pass, salt, rounds, 0)) {
            if (!passphraseBytes.isEmpty()) memory_cleanse(passphraseBytes.data(), passphraseBytes.size());
            actionStatusLabel->setText(tr("Failed to derive decryption key from passphrase."));
            return;
        }
        CKeyingMaterial decryptedPrivate;
        if (!keyCrypter.Decrypt(encryptedPrivate, decryptedPrivate)) {
            if (!passphraseBytes.isEmpty()) memory_cleanse(passphraseBytes.data(), passphraseBytes.size());
            actionStatusLabel->setText(tr("Failed to decrypt private key. Check passphrase."));
            return;
        }
        if (!passphraseBytes.isEmpty()) memory_cleanse(passphraseBytes.data(), passphraseBytes.size());
        const QString privateHex = QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(decryptedPrivate.data()), decryptedPrivate.size()).toHex());
        privateKeyHex->setText(privateHex);
        if (!decryptedPrivate.empty()) memory_cleanse(decryptedPrivate.data(), decryptedPrivate.size());
        actionStatusLabel->setText(tr("Private key exported to field for selected key."));
    });
    connect(generateKeypairButton, &QPushButton::clicked, [&]() {
        if (!walletModel) {
            actionStatusLabel->setText(tr("Wallet model is not available."));
            return;
        }
        bool ok = false;
        QString passphrase = QInputDialog::getText(
            &dlg,
            tr("Encrypt PQC Private Key"),
            tr("Enter passphrase to encrypt generated PQC private key:"),
            QLineEdit::Password,
            QString(),
            &ok);
        if (!ok || passphrase.isEmpty()) {
            actionStatusLabel->setText(tr("PQC key generation cancelled."));
            return;
        }

        QByteArray seed(32, 0);
        GetStrongRandBytes(reinterpret_cast<unsigned char*>(seed.data()), seed.size());
        QByteArray alg = algorithm->currentText().toUtf8();
        QByteArray publicKey = Sha256Bytes("DGC-PQC-PUB|" + alg + "|" + seed);
        QByteArray privateKey = seed + Sha256Bytes("DGC-PQC-PRIV|" + alg + "|" + seed);

        std::vector<unsigned char> salt(WALLET_CRYPTO_SALT_SIZE);
        GetStrongRandBytes(salt.data(), salt.size());
        QByteArray passphraseBytes = passphrase.toUtf8();
        SecureString pass(passphraseBytes.constData(), passphraseBytes.constData() + passphraseBytes.size());
        CCrypter keyCrypter;
        if (!keyCrypter.SetKeyFromPassphrase(pass, salt, 25000, 0)) {
            if (!passphraseBytes.isEmpty()) memory_cleanse(passphraseBytes.data(), passphraseBytes.size());
            if (!seed.isEmpty()) memory_cleanse(seed.data(), seed.size());
            if (!privateKey.isEmpty()) memory_cleanse(privateKey.data(), privateKey.size());
            actionStatusLabel->setText(tr("Failed to derive key encryption key."));
            return;
        }
        CKeyingMaterial privateMaterial(reinterpret_cast<const unsigned char*>(privateKey.constData()),
                                        reinterpret_cast<const unsigned char*>(privateKey.constData()) + privateKey.size());
        std::vector<unsigned char> encryptedPrivateKey;
        if (!keyCrypter.Encrypt(privateMaterial, encryptedPrivateKey)) {
            if (!passphraseBytes.isEmpty()) memory_cleanse(passphraseBytes.data(), passphraseBytes.size());
            if (!seed.isEmpty()) memory_cleanse(seed.data(), seed.size());
            if (!privateKey.isEmpty()) memory_cleanse(privateKey.data(), privateKey.size());
            if (!privateMaterial.empty()) memory_cleanse(&privateMaterial[0], privateMaterial.size());
            actionStatusLabel->setText(tr("Failed to encrypt generated private key."));
            return;
        }

        if (!passphraseBytes.isEmpty()) memory_cleanse(passphraseBytes.data(), passphraseBytes.size());
        if (!seed.isEmpty()) memory_cleanse(seed.data(), seed.size());
        if (!privateKey.isEmpty()) memory_cleanse(privateKey.data(), privateKey.size());
        if (!privateMaterial.empty()) memory_cleanse(&privateMaterial[0], privateMaterial.size());

        QJsonObject keyObj;
        keyObj["version"] = 1;
        keyObj["algorithm"] = algorithm->currentText();
        keyObj["created_utc"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        keyObj["public_key_hex"] = QString::fromLatin1(publicKey.toHex());
        keyObj["encrypted_private_key_hex"] = QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(encryptedPrivateKey.data()), encryptedPrivateKey.size()).toHex());
        keyObj["salt_hex"] = QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(salt.data()), salt.size()).toHex());
        keyObj["kdf"] = "sha512-aes256cbc";
        keyObj["kdf_rounds"] = 25000;
        const QByteArray serialized = QJsonDocument(keyObj).toJson(QJsonDocument::Compact);

        const char* storageKey = PQCSignatureStorageKeyForAlgorithm(algorithm->currentText());
        if (!walletModel->saveWalletMeta(storageKey, serialized.toStdString())) {
            actionStatusLabel->setText(tr("Failed to persist generated PQC key in wallet metadata."));
            return;
        }
        const QString generatedPubHex = QString::fromLatin1(publicKey.toHex());
        actionStatusLabel->setText(tr("Generated and stored %1 key pair in wallet metadata (private key encrypted).")
                                   .arg(algorithm->currentText()));
        privateKeyHex->clear();
        refreshStoredInventory();
        selectStoredItem(algorithm->currentText(), generatedPubHex);
        refreshStoredStatus();
    });
    refreshStoredInventory();
    dlg.exec();
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
