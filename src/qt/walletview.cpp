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

#include "utilstrencodings.h"
#include "ui_interface.h"
#include "wallet/crypter.h"

#include "crypto/sha256.h"
#include "crypto/sha512.h"
#include "random.h"
#include "support/cleanse.h"
#include "init.h"

#include <QAction>
#include <QActionGroup>
#include <QComboBox>
#include <QFileDialog>
#include <QGridLayout>
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
#include <QPlainTextEdit>
#include <QProgressDialog>
#include <QPushButton>
#include <QRegularExpression>
#include <QVBoxLayout>

namespace {
const char* PQCSignatureStorageKeyForAlgorithm(const QString& algorithm)
{
    if (algorithm == "dilithium2") return "pqc_sigkey_dilithium2";
    if (algorithm == "raccoong44") return "pqc_sigkey_raccoong44";
    return "pqc_sigkey_falcon512";
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

    const QString tempWalletFilename = outFilename + ".wallet.tmp.dat";
    if (!walletModel->backupWallet(tempWalletFilename)) {
        Q_EMIT message(tr("Backup Failed"), tr("There was an error trying to save the wallet data to %1.").arg(tempWalletFilename),
            CClientUIInterface::MSG_ERROR);
        return;
    }

    bool okAes = false;
    QString aesPassphrase = QInputDialog::getText(
        this,
        tr("AES Backup Passphrase"),
        tr("Enter passphrase for AES layer:"),
        QLineEdit::Password,
        QString(),
        &okAes);
    if (!okAes || aesPassphrase.isEmpty()) {
        QFile::remove(tempWalletFilename);
        return;
    }

    bool okPqc = false;
    QString pqcPassphrase = QInputDialog::getText(
        this,
        tr("PQC Backup Passphrase"),
        tr("Enter passphrase for PQC layer:"),
        QLineEdit::Password,
        QString(),
        &okPqc);
    if (!okPqc || pqcPassphrase.isEmpty()) {
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

    std::vector<unsigned char> aesSalt(WALLET_CRYPTO_SALT_SIZE);
    GetStrongRandBytes(aesSalt.data(), WALLET_CRYPTO_SALT_SIZE);
    QByteArray aesPassphraseBytes = aesPassphrase.toUtf8();
    SecureString aesPass(aesPassphraseBytes.constData(), aesPassphraseBytes.constData() + aesPassphraseBytes.size());

    CCrypter aesCrypter;
    if (!aesCrypter.SetKeyFromPassphrase(aesPass, aesSalt, 25000, 0)) {
        if (!aesPassphraseBytes.isEmpty()) memory_cleanse(aesPassphraseBytes.data(), aesPassphraseBytes.size());
        if (!plain.isEmpty()) memory_cleanse(plain.data(), plain.size());
        Q_EMIT message(tr("Encryption Failed"), tr("Unable to derive AES passphrase key."),
            CClientUIInterface::MSG_ERROR);
        return;
    }

    CKeyingMaterial plainMaterial(reinterpret_cast<const unsigned char*>(plain.constData()),
                                  reinterpret_cast<const unsigned char*>(plain.constData()) + plain.size());
    std::vector<unsigned char> aesCipher;
    if (!aesCrypter.Encrypt(plainMaterial, aesCipher)) {
        if (!aesPassphraseBytes.isEmpty()) memory_cleanse(aesPassphraseBytes.data(), aesPassphraseBytes.size());
        if (!plainMaterial.empty()) memory_cleanse(&plainMaterial[0], plainMaterial.size());
        if (!plain.isEmpty()) memory_cleanse(plain.data(), plain.size());
        Q_EMIT message(tr("Encryption Failed"), tr("Unable to encrypt wallet backup data with AES layer."),
            CClientUIInterface::MSG_ERROR);
        return;
    }
    if (!plainMaterial.empty()) memory_cleanse(&plainMaterial[0], plainMaterial.size());

    std::vector<unsigned char> pqcSalt(WALLET_CRYPTO_SALT_SIZE);
    GetStrongRandBytes(pqcSalt.data(), WALLET_CRYPTO_SALT_SIZE);
    QByteArray pqcPassphraseBytes = pqcPassphrase.toUtf8();
    SecureString pqcPass(pqcPassphraseBytes.constData(), pqcPassphraseBytes.constData() + pqcPassphraseBytes.size());
    const int pqcRounds = 50000;
    CCrypter pqcPassCrypter;
    if (!pqcPassCrypter.SetKeyFromPassphrase(pqcPass, pqcSalt, pqcRounds, 0)) {
        if (!aesPassphraseBytes.isEmpty()) memory_cleanse(aesPassphraseBytes.data(), aesPassphraseBytes.size());
        if (!pqcPassphraseBytes.isEmpty()) memory_cleanse(pqcPassphraseBytes.data(), pqcPassphraseBytes.size());
        if (!plain.isEmpty()) memory_cleanse(plain.data(), plain.size());
        Q_EMIT message(tr("Encryption Failed"), tr("Unable to derive PQC passphrase key."),
            CClientUIInterface::MSG_ERROR);
        return;
    }

    CKeyingMaterial aesCipherMaterial(aesCipher.begin(), aesCipher.end());
    std::vector<unsigned char> cipher;
    if (!pqcPassCrypter.Encrypt(aesCipherMaterial, cipher)) {
        if (!aesPassphraseBytes.isEmpty()) memory_cleanse(aesPassphraseBytes.data(), aesPassphraseBytes.size());
        if (!pqcPassphraseBytes.isEmpty()) memory_cleanse(pqcPassphraseBytes.data(), pqcPassphraseBytes.size());
        if (!plain.isEmpty()) memory_cleanse(plain.data(), plain.size());
        if (!aesCipherMaterial.empty()) memory_cleanse(&aesCipherMaterial[0], aesCipherMaterial.size());
        Q_EMIT message(tr("Encryption Failed"), tr("Unable to encrypt AES layer with PQC password layer."),
            CClientUIInterface::MSG_ERROR);
        return;
    }
    if (!aesCipherMaterial.empty()) memory_cleanse(&aesCipherMaterial[0], aesCipherMaterial.size());

    if (!aesPassphraseBytes.isEmpty()) memory_cleanse(aesPassphraseBytes.data(), aesPassphraseBytes.size());
    if (!pqcPassphraseBytes.isEmpty()) memory_cleanse(pqcPassphraseBytes.data(), pqcPassphraseBytes.size());
    if (!plain.isEmpty()) memory_cleanse(plain.data(), plain.size());

    const QByteArray envelopeAlgo = "AES256-CBC+PQC-PASS-CASCADE";
    unsigned char digest[CSHA256::OUTPUT_SIZE];
    CSHA256 hasher;
    hasher.Write(reinterpret_cast<const unsigned char*>(envelopeAlgo.constData()), envelopeAlgo.size());
    hasher.Write(reinterpret_cast<const unsigned char*>(cipher.data()), cipher.size());
    hasher.Finalize(digest);

    QFile outFile(outFilename);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        memory_cleanse(digest, sizeof(digest));
        Q_EMIT message(tr("Encryption Failed"), tr("Unable to write envelope file %1.").arg(outFilename),
            CClientUIInterface::MSG_ERROR);
        return;
    }
    outFile.write("DGC-PQCE3\n");
    outFile.write("ALGO:" + envelopeAlgo + "\n");
    outFile.write("AES_KDF:sha512-aes256cbc\n");
    outFile.write("AES_SALT:" + QByteArray(reinterpret_cast<const char*>(aesSalt.data()), aesSalt.size()).toHex() + "\n");
    outFile.write("AES_ROUNDS:25000\n");
    outFile.write("PQC_KDF:sha512-aes256cbc\n");
    outFile.write("PQC_SALT:" + QByteArray(reinterpret_cast<const char*>(pqcSalt.data()), pqcSalt.size()).toHex() + "\n");
    outFile.write("PQC_ROUNDS:" + QByteArray::number(pqcRounds) + "\n");
    outFile.write("DATA_SHA256:" + QByteArray(reinterpret_cast<const char*>(digest), sizeof(digest)).toHex() + "\n");
    outFile.write("DATA_B64:" + QByteArray(reinterpret_cast<const char*>(cipher.data()), cipher.size()).toBase64() + "\n");
    outFile.close();
    memory_cleanse(digest, sizeof(digest));

    Q_EMIT message(tr("Backup Successful"), tr("Double password-encrypted (AES + PQC) wallet backup was successfully saved to %1.").arg(outFilename),
        CClientUIInterface::MSG_INFORMATION);
}

void WalletView::restoreWalletEncrypted()
{
    if (!walletModel) {
        return;
    }

    QString inFilename = GUIUtil::getOpenFileName(this,
        tr("Restore Wallet (PQC Envelope)"), QString(),
        tr("PQC Envelope (*.pqce)"), NULL);
    if (inFilename.isEmpty()) {
        return;
    }

    QFile inFile(inFilename);
    if (!inFile.open(QIODevice::ReadOnly)) {
        Q_EMIT message(tr("Restore Failed"), tr("Unable to open envelope file %1.").arg(inFilename),
            CClientUIInterface::MSG_ERROR);
        return;
    }
    const QList<QByteArray> lines = inFile.readAll().split('\n');
    inFile.close();

    QMap<QByteArray, QByteArray> fields;
    QByteArray header;
    for (int i = 0; i < lines.size(); ++i) {
        QByteArray line = lines.at(i).trimmed();
        if (line.isEmpty()) continue;
        if (i == 0 && !line.contains(':')) {
            header = line;
            continue;
        }
        int sep = line.indexOf(':');
        if (sep <= 0) continue;
        fields.insert(line.left(sep), line.mid(sep + 1));
    }
    if (header != "DGC-PQCE3") {
        Q_EMIT message(tr("Restore Failed"), tr("Unsupported envelope format: %1").arg(QString::fromLatin1(header)),
            CClientUIInterface::MSG_ERROR);
        return;
    }
    if (fields.value("ALGO") != "AES256-CBC+PQC-PASS-CASCADE") {
        Q_EMIT message(tr("Restore Failed"), tr("Unsupported envelope algorithm."),
            CClientUIInterface::MSG_ERROR);
        return;
    }

    const QByteArray aesSaltHex = fields.value("AES_SALT");
    const QByteArray aesRoundsRaw = fields.value("AES_ROUNDS");
    const QByteArray pqcKdf = fields.value("PQC_KDF");
    const QByteArray pqcSaltHex = fields.value("PQC_SALT");
    const QByteArray pqcRoundsRaw = fields.value("PQC_ROUNDS");
    const QByteArray expectedDigestHex = fields.value("DATA_SHA256");
    const QByteArray dataB64 = fields.value("DATA_B64");
    if (aesSaltHex.isEmpty() || aesRoundsRaw.isEmpty() || pqcSaltHex.isEmpty() || pqcRoundsRaw.isEmpty() ||
        expectedDigestHex.isEmpty() || dataB64.isEmpty()) {
        Q_EMIT message(tr("Restore Failed"), tr("Envelope is missing required fields."),
            CClientUIInterface::MSG_ERROR);
        return;
    }
    if (pqcKdf != "sha512-aes256cbc") {
        Q_EMIT message(tr("Restore Failed"), tr("Unsupported PQC password KDF in envelope."),
            CClientUIInterface::MSG_ERROR);
        return;
    }

    const QByteArray aesSalt = QByteArray::fromHex(aesSaltHex);
    const QByteArray pqcSalt = QByteArray::fromHex(pqcSaltHex);
    bool okAesRounds = false;
    bool okPqcRounds = false;
    const int aesRounds = QString::fromLatin1(aesRoundsRaw).toInt(&okAesRounds);
    const int pqcRounds = QString::fromLatin1(pqcRoundsRaw).toInt(&okPqcRounds);
    if (!okAesRounds || !okPqcRounds || aesRounds <= 0 || pqcRounds <= 0 ||
        aesSalt.isEmpty() || pqcSalt.isEmpty()) {
        Q_EMIT message(tr("Restore Failed"), tr("Envelope KDF parameters are invalid."),
            CClientUIInterface::MSG_ERROR);
        return;
    }

    std::vector<unsigned char> outerCipher = DecodeBase64(dataB64.toStdString().c_str(), NULL);
    if (outerCipher.empty()) {
        Q_EMIT message(tr("Restore Failed"), tr("Envelope encrypted payload is invalid."),
            CClientUIInterface::MSG_ERROR);
        return;
    }

    const QByteArray envelopeAlgo = "AES256-CBC+PQC-PASS-CASCADE";
    unsigned char digest[CSHA256::OUTPUT_SIZE];
    CSHA256 hasher;
    hasher.Write(reinterpret_cast<const unsigned char*>(envelopeAlgo.constData()), envelopeAlgo.size());
    hasher.Write(reinterpret_cast<const unsigned char*>(outerCipher.data()), outerCipher.size());
    hasher.Finalize(digest);
    const QByteArray digestHex(reinterpret_cast<const char*>(digest), sizeof(digest));
    const QByteArray digestHexAscii = digestHex.toHex();
    memory_cleanse(digest, sizeof(digest));
    if (QString::compare(QString::fromLatin1(expectedDigestHex), QString::fromLatin1(digestHexAscii), Qt::CaseInsensitive) != 0) {
        Q_EMIT message(tr("Restore Failed"), tr("Envelope digest check failed."),
            CClientUIInterface::MSG_ERROR);
        return;
    }

    bool okAes = false;
    const QString aesPassphrase = QInputDialog::getText(
        this,
        tr("AES Backup Passphrase"),
        tr("Enter passphrase for AES layer:"),
        QLineEdit::Password,
        QString(),
        &okAes);
    if (!okAes || aesPassphrase.isEmpty()) {
        return;
    }
    bool okPqc = false;
    const QString pqcPassphrase = QInputDialog::getText(
        this,
        tr("PQC Backup Passphrase"),
        tr("Enter passphrase for PQC layer:"),
        QLineEdit::Password,
        QString(),
        &okPqc);
    if (!okPqc || pqcPassphrase.isEmpty()) {
        return;
    }

    QByteArray aesPassphraseBytes = aesPassphrase.toUtf8();
    QByteArray pqcPassphraseBytes = pqcPassphrase.toUtf8();
    SecureString pqcPass(pqcPassphraseBytes.constData(), pqcPassphraseBytes.constData() + pqcPassphraseBytes.size());
    CCrypter pqcPassCrypter;
    std::vector<unsigned char> pqcSaltBytes(pqcSalt.begin(), pqcSalt.end());
    if (!pqcPassCrypter.SetKeyFromPassphrase(pqcPass, pqcSaltBytes, pqcRounds, 0)) {
        if (!aesPassphraseBytes.isEmpty()) memory_cleanse(aesPassphraseBytes.data(), aesPassphraseBytes.size());
        if (!pqcPassphraseBytes.isEmpty()) memory_cleanse(pqcPassphraseBytes.data(), pqcPassphraseBytes.size());
        Q_EMIT message(tr("Restore Failed"), tr("Unable to derive PQC passphrase key."),
            CClientUIInterface::MSG_ERROR);
        return;
    }
    CKeyingMaterial aesCipherMaterial;
    if (!pqcPassCrypter.Decrypt(outerCipher, aesCipherMaterial)) {
        if (!aesPassphraseBytes.isEmpty()) memory_cleanse(aesPassphraseBytes.data(), aesPassphraseBytes.size());
        if (!pqcPassphraseBytes.isEmpty()) memory_cleanse(pqcPassphraseBytes.data(), pqcPassphraseBytes.size());
        Q_EMIT message(tr("Restore Failed"), tr("PQC password decryption failed."),
            CClientUIInterface::MSG_ERROR);
        return;
    }

    SecureString aesPass(aesPassphraseBytes.constData(), aesPassphraseBytes.constData() + aesPassphraseBytes.size());
    CCrypter aesCrypter;
    std::vector<unsigned char> aesSaltBytes(aesSalt.begin(), aesSalt.end());
    if (!aesCrypter.SetKeyFromPassphrase(aesPass, aesSaltBytes, aesRounds, 0)) {
        if (!aesPassphraseBytes.isEmpty()) memory_cleanse(aesPassphraseBytes.data(), aesPassphraseBytes.size());
        if (!pqcPassphraseBytes.isEmpty()) memory_cleanse(pqcPassphraseBytes.data(), pqcPassphraseBytes.size());
        if (!aesCipherMaterial.empty()) memory_cleanse(&aesCipherMaterial[0], aesCipherMaterial.size());
        Q_EMIT message(tr("Restore Failed"), tr("Unable to derive AES passphrase key."),
            CClientUIInterface::MSG_ERROR);
        return;
    }

    CKeyingMaterial plainMaterial;
    std::vector<unsigned char> aesCipher(aesCipherMaterial.begin(), aesCipherMaterial.end());
    if (!aesCrypter.Decrypt(aesCipher, plainMaterial)) {
        if (!aesPassphraseBytes.isEmpty()) memory_cleanse(aesPassphraseBytes.data(), aesPassphraseBytes.size());
        if (!pqcPassphraseBytes.isEmpty()) memory_cleanse(pqcPassphraseBytes.data(), pqcPassphraseBytes.size());
        if (!aesCipherMaterial.empty()) memory_cleanse(&aesCipherMaterial[0], aesCipherMaterial.size());
        Q_EMIT message(tr("Restore Failed"), tr("AES password decryption failed."),
            CClientUIInterface::MSG_ERROR);
        return;
    }

    if (!aesPassphraseBytes.isEmpty()) memory_cleanse(aesPassphraseBytes.data(), aesPassphraseBytes.size());
    if (!pqcPassphraseBytes.isEmpty()) memory_cleanse(pqcPassphraseBytes.data(), pqcPassphraseBytes.size());
    if (!aesCipherMaterial.empty()) memory_cleanse(&aesCipherMaterial[0], aesCipherMaterial.size());

    if (plainMaterial.empty()) {
        Q_EMIT message(tr("Restore Failed"), tr("Decrypted wallet backup is empty."),
            CClientUIInterface::MSG_ERROR);
        return;
    }

    const QString tempWalletOut = inFilename + ".decrypted.wallet.dat";
    QFile outWallet(tempWalletOut);
    if (!outWallet.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (!plainMaterial.empty()) memory_cleanse(&plainMaterial[0], plainMaterial.size());
        Q_EMIT message(tr("Restore Failed"), tr("Unable to write decrypted wallet data."),
            CClientUIInterface::MSG_ERROR);
        return;
    }
    outWallet.write(reinterpret_cast<const char*>(plainMaterial.data()), plainMaterial.size());
    outWallet.close();
    if (!plainMaterial.empty()) memory_cleanse(&plainMaterial[0], plainMaterial.size());

    const QMessageBox::StandardButton answer = QMessageBox::question(
        this,
        tr("Restore Wallet"),
        tr("Replace current wallet file with decrypted backup and restart now?\n\nCurrent wallet file: %1\nDecrypted backup: %2")
            .arg(walletModel->getWalletFilePath(), tempWalletOut),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (answer != QMessageBox::Yes) {
        Q_EMIT message(tr("Restore Complete"), tr("Decrypted wallet written to %1").arg(tempWalletOut),
            CClientUIInterface::MSG_INFORMATION);
        return;
    }

    const QString walletPath = walletModel->getWalletFilePath();
    if (walletPath.isEmpty()) {
        Q_EMIT message(tr("Restore Failed"), tr("Unable to resolve current wallet file path."),
            CClientUIInterface::MSG_ERROR);
        return;
    }
    if (QFile::exists(walletPath)) {
        const QString backupCurrent = walletPath + ".before-pqce-restore.bak";
        QFile::remove(backupCurrent);
        if (!QFile::rename(walletPath, backupCurrent)) {
            Q_EMIT message(tr("Restore Failed"), tr("Unable to backup current wallet file %1").arg(walletPath),
                CClientUIInterface::MSG_ERROR);
            return;
        }
    }
    if (!QFile::copy(tempWalletOut, walletPath)) {
        Q_EMIT message(tr("Restore Failed"), tr("Unable to copy decrypted wallet into active wallet file."),
            CClientUIInterface::MSG_ERROR);
        return;
    }
    QFile::remove(tempWalletOut);

    Q_EMIT message(tr("Restore Successful"), tr("Wallet file was restored from PQC envelope. Client will now restart."),
        CClientUIInterface::MSG_INFORMATION);
    StartShutdown();
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
    algorithm->addItem("raccoong44");

    QLineEdit* publicKeyHex = new QLineEdit(&dlg);
    publicKeyHex->setReadOnly(true);
    publicKeyHex->setPlaceholderText(tr("Hex-encoded public key"));
    QPlainTextEdit* privateKeyHex = new QPlainTextEdit(&dlg);
    privateKeyHex->setReadOnly(true);
    privateKeyHex->setPlaceholderText(tr("Encrypted private key export requires passphrase"));
    privateKeyHex->setLineWrapMode(QPlainTextEdit::NoWrap);
    privateKeyHex->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    privateKeyHex->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    privateKeyHex->setFixedHeight(64);
    QPushButton* generateKeypairButton = new QPushButton(tr("Generate && Store Encrypted Key Pair"), &dlg);
    QPushButton* importKeypairButton = new QPushButton(tr("Import Key Pair"), &dlg);
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

    QGridLayout* keyButtons = new QGridLayout();
    keyButtons->addWidget(generateKeypairButton, 0, 0);
    keyButtons->addWidget(importKeypairButton, 0, 1);
    keyButtons->addWidget(exportPrivateKeyButton, 1, 0);
    keyButtons->addWidget(removeStoredButton, 1, 1);
    keyButtons->setColumnStretch(0, 0);
    keyButtons->setColumnStretch(1, 0);
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
            {"pqc_sigkey_dilithium2", "dilithium2"},
            {"pqc_sigkey_raccoong44", "raccoong44"}
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
        privateKeyHex->setPlainText(privateHex);
        if (!decryptedPrivate.empty()) memory_cleanse(decryptedPrivate.data(), decryptedPrivate.size());
        actionStatusLabel->setText(tr("Private key exported to field for selected key."));
    });
    connect(importKeypairButton, &QPushButton::clicked, [&]() {
        if (!walletModel) {
            actionStatusLabel->setText(tr("Wallet model is not available."));
            return;
        }
        QDialog importDlg(&dlg);
        importDlg.setWindowTitle(tr("Import PQC Key Pair"));
        importDlg.resize(720, 320);
        QVBoxLayout* importVbox = new QVBoxLayout(&importDlg);

        QComboBox* importAlgorithm = new QComboBox(&importDlg);
        importAlgorithm->addItem("falcon512");
        importAlgorithm->addItem("dilithium2");
        importAlgorithm->addItem("raccoong44");
        importAlgorithm->setCurrentText(algorithm->currentText());

        QLineEdit* importPublicKeyHex = new QLineEdit(&importDlg);
        importPublicKeyHex->setPlaceholderText(tr("Public key hex"));
        QPlainTextEdit* importPrivateKeyHex = new QPlainTextEdit(&importDlg);
        importPrivateKeyHex->setPlaceholderText(tr("Private key hex"));
        importPrivateKeyHex->setLineWrapMode(QPlainTextEdit::NoWrap);
        importPrivateKeyHex->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        importPrivateKeyHex->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        importPrivateKeyHex->setFixedHeight(84);

        QFormLayout* importForm = new QFormLayout();
        importForm->addRow(tr("Algorithm:"), importAlgorithm);
        importForm->addRow(tr("Public key (hex):"), importPublicKeyHex);
        importForm->addRow(tr("Private key (hex):"), importPrivateKeyHex);
        importVbox->addLayout(importForm);

        QDialogButtonBox* importButtons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &importDlg);
        importVbox->addWidget(importButtons);
        connect(importButtons, &QDialogButtonBox::rejected, &importDlg, &QDialog::reject);
        connect(importButtons, &QDialogButtonBox::accepted, &importDlg, &QDialog::accept);

        if (importDlg.exec() != QDialog::Accepted) {
            actionStatusLabel->setText(tr("PQC key import cancelled."));
            return;
        }

        const QString selectedAlgorithm = importAlgorithm->currentText();
        const QString publicHex = importPublicKeyHex->text().trimmed();
        QString privateHex = importPrivateKeyHex->toPlainText().trimmed();
        privateHex.remove(QRegularExpression("\\s+"));
        if (publicHex.isEmpty() || privateHex.isEmpty()) {
            actionStatusLabel->setText(tr("Both public and private key hex are required."));
            return;
        }
        if (!IsHex(publicHex.toStdString()) || !IsHex(privateHex.toStdString())) {
            actionStatusLabel->setText(tr("Imported key hex is invalid."));
            return;
        }

        bool ok = false;
        QString passphrase = QInputDialog::getText(
            &dlg,
            tr("Encrypt Imported PQC Private Key"),
            tr("Enter passphrase to encrypt imported private key:"),
            QLineEdit::Password,
            QString(),
            &ok);
        if (!ok || passphrase.isEmpty()) {
            actionStatusLabel->setText(tr("PQC key import cancelled."));
            return;
        }

        std::vector<unsigned char> rawPrivateKey = ParseHex(privateHex.toStdString());
        if (rawPrivateKey.empty()) {
            actionStatusLabel->setText(tr("Imported private key is empty."));
            return;
        }
        std::vector<unsigned char> salt(WALLET_CRYPTO_SALT_SIZE);
        GetStrongRandBytes(salt.data(), salt.size());
        QByteArray passphraseBytes = passphrase.toUtf8();
        SecureString pass(passphraseBytes.constData(), passphraseBytes.constData() + passphraseBytes.size());
        CCrypter keyCrypter;
        if (!keyCrypter.SetKeyFromPassphrase(pass, salt, 25000, 0)) {
            if (!passphraseBytes.isEmpty()) memory_cleanse(passphraseBytes.data(), passphraseBytes.size());
            if (!rawPrivateKey.empty()) memory_cleanse(rawPrivateKey.data(), rawPrivateKey.size());
            actionStatusLabel->setText(tr("Failed to derive key encryption key."));
            return;
        }
        CKeyingMaterial privateMaterial(rawPrivateKey.begin(), rawPrivateKey.end());
        std::vector<unsigned char> encryptedPrivateKey;
        if (!keyCrypter.Encrypt(privateMaterial, encryptedPrivateKey)) {
            if (!passphraseBytes.isEmpty()) memory_cleanse(passphraseBytes.data(), passphraseBytes.size());
            if (!rawPrivateKey.empty()) memory_cleanse(rawPrivateKey.data(), rawPrivateKey.size());
            if (!privateMaterial.empty()) memory_cleanse(&privateMaterial[0], privateMaterial.size());
            actionStatusLabel->setText(tr("Failed to encrypt imported private key."));
            return;
        }

        if (!passphraseBytes.isEmpty()) memory_cleanse(passphraseBytes.data(), passphraseBytes.size());
        if (!rawPrivateKey.empty()) memory_cleanse(rawPrivateKey.data(), rawPrivateKey.size());
        if (!privateMaterial.empty()) memory_cleanse(&privateMaterial[0], privateMaterial.size());

        QJsonObject keyObj;
        keyObj["version"] = 1;
        keyObj["algorithm"] = selectedAlgorithm;
        keyObj["created_utc"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        keyObj["public_key_hex"] = publicHex;
        keyObj["encrypted_private_key_hex"] = QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(encryptedPrivateKey.data()), encryptedPrivateKey.size()).toHex());
        keyObj["salt_hex"] = QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(salt.data()), salt.size()).toHex());
        keyObj["kdf"] = "sha512-aes256cbc";
        keyObj["kdf_rounds"] = 25000;
        const QByteArray serialized = QJsonDocument(keyObj).toJson(QJsonDocument::Compact);

        const char* storageKey = PQCSignatureStorageKeyForAlgorithm(selectedAlgorithm);
        if (!walletModel->saveWalletMeta(storageKey, serialized.toStdString())) {
            actionStatusLabel->setText(tr("Failed to persist imported PQC key pair."));
            return;
        }
        actionStatusLabel->setText(tr("Imported and stored %1 key pair in wallet metadata (private key encrypted).")
                                   .arg(selectedAlgorithm));
        privateKeyHex->clear();
        refreshStoredInventory();
        selectStoredItem(selectedAlgorithm, publicHex);
        refreshStoredStatus();
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
