// Copyright (c) 2011-2016 The Bitcoin Core developers
// Copyright (c) 2021-2023 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sendcoinsdialog.h"
#include "ui_sendcoinsdialog.h"

#include "config/bitcoin-config.h"
#include "addresstablemodel.h"
#include "bitcoinunits.h"
#include "clientmodel.h"
#include "coincontroldialog.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "platformstyle.h"
#include "sendcoinsentry.h"
#include "walletmodel.h"

#include "base58.h"
#include "chainparams.h"
#include "dogecoin-fees.h"
#if ENABLE_LIBOQS
#include "pqc/pqc_commitment.h"
#include "support/experimental.h"
EXPERIMENTAL_FEATURE
#endif
#include "script/standard.h"
#include "utilstrencodings.h"
#include "wallet/coincontrol.h"
#include "validation.h" // mempool and minRelayTxFeeRate
#include "ui_interface.h"
#include "txmempool.h"
#include "wallet/wallet.h"
#include "crypto/sha256.h"
#include "support/cleanse.h"
#include "wallet/crypter.h"

#include <univalue.h>

#include <QFontMetrics>
#include <QFormLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QSizePolicy>
#include <QTextEdit>
#include <QTextDocument>
#include <QTimer>
#include <QVBoxLayout>

#include "rpc/protocol.h"
#include "rpc/server.h"

#define SEND_CONFIRM_DELAY   3
#define CHARACTERS_DISPLAY_LIMIT_IN_LABEL 45

namespace {

/** Build a deterministic 32-byte message for PQC signing, binding the
 *  signature to the transaction parameters (recipients + amounts).
 *  Per the BIP spec, the message SHOULD be a transaction sighash, but since
 *  the GUI generates the commitment before the transaction is assembled,
 *  we use a deterministic digest that binds to the intended tx parameters.
 *  Returns raw 32 bytes.
 */
QByteArray BuildPqcSigningMessage(const QString& algorithm,
                                  const QString& publicKeyHex,
                                  const QList<SendCoinsRecipient>& recipients)
{
    QByteArray context("DGC-PQC-SIG-V1|");
    context.append(algorithm.trimmed().toUtf8());
    context.append("|");
    context.append(publicKeyHex.trimmed().toUtf8());
    context.append("|");
    for (int i = 0; i < recipients.size(); ++i) {
        const SendCoinsRecipient& r = recipients.at(i);
        context.append(r.address.trimmed().toUtf8());
        context.append("|");
        context.append(QByteArray::number(static_cast<qint64>(r.amount)));
        context.append("|");
        context.append(r.fSubtractFeeFromAmount ? "1" : "0");
        context.append(";");
    }
    unsigned char digest[CSHA256::OUTPUT_SIZE];
    CSHA256().Write(reinterpret_cast<const unsigned char*>(context.constData()), context.size()).Finalize(digest);
    return QByteArray(reinterpret_cast<const char*>(digest), sizeof(digest));
}

} // namespace

SendCoinsDialog::SendCoinsDialog(const PlatformStyle *_platformStyle, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SendCoinsDialog),
    clientModel(0),
    model(0),
    fNewRecipientAllowed(true),
    fFeeMinimized(true),
    platformStyle(_platformStyle)
{
    ui->setupUi(this);

    if (!_platformStyle->getImagesOnButtons()) {
        ui->addButton->setIcon(QIcon());
        ui->clearButton->setIcon(QIcon());
        ui->sendButton->setIcon(QIcon());
    } else {
        ui->addButton->setIcon(_platformStyle->SingleColorIcon(":/icons/add"));
        ui->clearButton->setIcon(_platformStyle->SingleColorIcon(":/icons/remove"));
        ui->sendButton->setIcon(_platformStyle->SingleColorIcon(":/icons/send"));
    }

    GUIUtil::setupAddressWidget(ui->lineEditCoinControlChange, this);

    QFrame *pqcFrame = new QFrame(this);
    pqcFrame->setFrameShape(QFrame::StyledPanel);
    pqcFrame->setFrameShadow(QFrame::Sunken);
    QVBoxLayout *pqcLayout = new QVBoxLayout(pqcFrame);

    // Header row: title only
    QHBoxLayout *pqcHeaderLayout = new QHBoxLayout();
    QLabel *pqcHeader = new QLabel(tr("PQC Commitment"), pqcFrame);
    QFont pqcFont = pqcHeader->font();
    pqcFont.setBold(true);
    pqcHeader->setFont(pqcFont);
    pqcHeaderLayout->addWidget(pqcHeader);
    pqcHeaderLayout->addStretch();
    pqcLayout->addLayout(pqcHeaderLayout);

    // Form: logical top-to-bottom flow
    QFormLayout *pqcForm = new QFormLayout();

    // 1) Signature key pair selection
    pqcKeyPairComboBox = new QComboBox(pqcFrame);
    pqcLoadStoredKeyButton = new QPushButton(tr("Refresh keys"), pqcFrame);
    QHBoxLayout *pqcKeyRow = new QHBoxLayout();
    pqcKeyRow->addWidget(pqcKeyPairComboBox);
    pqcKeyRow->addWidget(pqcLoadStoredKeyButton);
    QWidget *pqcKeyRowWidget = new QWidget(pqcFrame);
    pqcKeyRowWidget->setLayout(pqcKeyRow);
    pqcForm->addRow(tr("Signature key pair:"), pqcKeyRowWidget);

    // 2) Carrier mode — must be selected before generating
    pqcCarrierModeCheckBox = new QCheckBox(tr("Carrier mode (P2SH data carrier for on-chain PQ verification)"), pqcFrame);
    pqcCarrierModeCheckBox->setChecked(false);
    pqcCarrierModeCheckBox->setToolTip(tr("When enabled, TX_C includes P2SH carrier output(s) alongside the OP_RETURN commitment.\n"
                                            "A separate follow-up transaction (TX_R) reveals the full PQC public key and signature on-chain and must be broadcast separately.\n"
                                            "This is the canonical Phase 1 transport for on-chain PQ verification material."));
    pqcForm->addRow(QString(), pqcCarrierModeCheckBox);

    // 3) Generate / Decode buttons
    pqcGenerateButton = new QPushButton(tr("Generate for transaction"), pqcFrame);
    pqcDecodeButton = new QPushButton(tr("Decode Commitment"), pqcFrame);
    pqcDecodeButton->setEnabled(false);
    QHBoxLayout *pqcButtonRow = new QHBoxLayout();
    pqcButtonRow->addWidget(pqcGenerateButton);
    pqcButtonRow->addWidget(pqcDecodeButton);
    pqcButtonRow->addStretch();
    QWidget *pqcButtonRowWidget = new QWidget(pqcFrame);
    pqcButtonRowWidget->setLayout(pqcButtonRow);
    pqcForm->addRow(QString(), pqcButtonRowWidget);

    // 4) Generated commitment display
    pqcCommitmentLineEdit = new QLineEdit(pqcFrame);
    pqcCommitmentLineEdit->setReadOnly(true);
    pqcCommitmentLineEdit->setPlaceholderText(tr("No commitment generated yet"));
    pqcForm->addRow(tr("Generated commitment:"), pqcCommitmentLineEdit);

    // 5) Include commitment checkbox
    pqcIncludeCommitmentCheckBox = new QCheckBox(tr("Include commitment in this transaction"), pqcFrame);
    pqcIncludeCommitmentCheckBox->setChecked(false);
    pqcForm->addRow(QString(), pqcIncludeCommitmentCheckBox);
    pqcLayout->addLayout(pqcForm);

    ui->verticalLayout->insertWidget(2, pqcFrame);

    addEntry();

    connect(ui->addButton, SIGNAL(clicked()), this, SLOT(addEntry()));
    connect(ui->clearButton, SIGNAL(clicked()), this, SLOT(clear()));
    connect(pqcLoadStoredKeyButton, SIGNAL(clicked()), this, SLOT(onUseStoredPqcKeyClicked()));
    connect(pqcGenerateButton, SIGNAL(clicked()), this, SLOT(onGeneratePqcCommitmentClicked()));
    connect(pqcDecodeButton, SIGNAL(clicked()), this, SLOT(onDecodePqcCommitmentClicked()));

    // Coin Control
    connect(ui->pushButtonCoinControl, SIGNAL(clicked()), this, SLOT(coinControlButtonClicked()));
    connect(ui->checkBoxCoinControlChange, SIGNAL(stateChanged(int)), this, SLOT(coinControlChangeChecked(int)));
    connect(ui->lineEditCoinControlChange, SIGNAL(textEdited(const QString &)), this, SLOT(coinControlChangeEdited(const QString &)));

    // Coin Control: clipboard actions
    QAction *clipboardQuantityAction = new QAction(tr("Copy quantity"), this);
    QAction *clipboardAmountAction = new QAction(tr("Copy amount"), this);
    QAction *clipboardFeeAction = new QAction(tr("Copy fee"), this);
    QAction *clipboardAfterFeeAction = new QAction(tr("Copy after fee"), this);
    QAction *clipboardBytesAction = new QAction(tr("Copy bytes"), this);
    QAction *clipboardLowOutputAction = new QAction(tr("Copy dust"), this);
    QAction *clipboardChangeAction = new QAction(tr("Copy change"), this);
    connect(clipboardQuantityAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardQuantity()));
    connect(clipboardAmountAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardAmount()));
    connect(clipboardFeeAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardFee()));
    connect(clipboardAfterFeeAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardAfterFee()));
    connect(clipboardBytesAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardBytes()));
    connect(clipboardLowOutputAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardLowOutput()));
    connect(clipboardChangeAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardChange()));
    ui->labelCoinControlQuantity->addAction(clipboardQuantityAction);
    ui->labelCoinControlAmount->addAction(clipboardAmountAction);
    ui->labelCoinControlFee->addAction(clipboardFeeAction);
    ui->labelCoinControlAfterFee->addAction(clipboardAfterFeeAction);
    ui->labelCoinControlBytes->addAction(clipboardBytesAction);
    ui->labelCoinControlLowOutput->addAction(clipboardLowOutputAction);
    ui->labelCoinControlChange->addAction(clipboardChangeAction);

    // init transaction fee section
    QSettings settings;
    if (!settings.contains("fFeeSectionMinimized"))
        settings.setValue("fFeeSectionMinimized", true);
    if (!settings.contains("nFeeRadio") && settings.contains("nTransactionFee") && settings.value("nTransactionFee").toLongLong() > 0) // compatibility
        settings.setValue("nFeeRadio", 1); // custom
    if (!settings.contains("nFeeRadio"))
        settings.setValue("nFeeRadio", 0); // recommended
    if (!settings.contains("nCustomFeeRadio") && settings.contains("nTransactionFee") && settings.value("nTransactionFee").toLongLong() > 0) // compatibility
        settings.setValue("nCustomFeeRadio", 1); // total at least
    if (!settings.contains("nCustomFeeRadio"))
        settings.setValue("nCustomFeeRadio", 0); // per kilobyte
    if (!settings.contains("nPresetFeeSliderPosition"))
        settings.setValue("nPresetFeeSliderPosition", 0);
    if (!settings.contains("nTransactionFee"))
        settings.setValue("nTransactionFee", (qint64)DEFAULT_TRANSACTION_FEE);
    if (!settings.contains("fPayOnlyMinFee"))
        settings.setValue("fPayOnlyMinFee", false);
    ui->groupFee->setId(ui->radioSmartFee, 0);
    ui->groupFee->setId(ui->radioCustomFee, 1);
    ui->groupFee->button((int)std::max(0, std::min(1, settings.value("nFeeRadio").toInt())))->setChecked(true);
    ui->groupCustomFee->setId(ui->radioCustomPerKilobyte, 0);
    ui->groupCustomFee->setId(ui->radioCustomAtLeast, 1);
    ui->groupCustomFee->button((int)std::max(0, std::min(1, settings.value("nCustomFeeRadio").toInt())))->setChecked(true);
    ui->customFee->setValue(settings.value("nTransactionFee").toLongLong());
    ui->checkBoxMinimumFee->setChecked(settings.value("fPayOnlyMinFee").toBool());
    minimizeFeeSection(settings.value("fFeeSectionMinimized").toBool());
}

void SendCoinsDialog::setClientModel(ClientModel *_clientModel)
{
    this->clientModel = _clientModel;

    if (_clientModel) {
        connect(_clientModel, SIGNAL(numBlocksChanged(int,QDateTime,double,bool)), this, SLOT(updateFeeLabel()));
    }
}

void SendCoinsDialog::setModel(WalletModel *_model)
{
    this->model = _model;

    if(_model && _model->getOptionsModel())
    {
        for(int i = 0; i < ui->entries->count(); ++i)
        {
            SendCoinsEntry *entry = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(i)->widget());
            if(entry)
            {
                entry->setModel(_model);
            }
        }

        setBalance(_model->getBalance(), _model->getUnconfirmedBalance(), _model->getImmatureBalance(),
                   _model->getWatchBalance(), _model->getWatchUnconfirmedBalance(), _model->getWatchImmatureBalance());
        connect(_model, SIGNAL(balanceChanged(CAmount,CAmount,CAmount,CAmount,CAmount,CAmount)), this, SLOT(setBalance(CAmount,CAmount,CAmount,CAmount,CAmount,CAmount)));
        connect(_model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));
        updateDisplayUnit();

        // Coin Control
        connect(_model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(coinControlUpdateLabels()));
        connect(_model->getOptionsModel(), SIGNAL(coinControlFeaturesChanged(bool)), this, SLOT(coinControlFeatureChanged(bool)));
        ui->frameCoinControl->setVisible(_model->getOptionsModel()->getCoinControlFeatures());
        coinControlUpdateLabels();

        // fee section
        connect(ui->sliderSmartFee, SIGNAL(valueChanged(int)), this, SLOT(updateFeeLabel()));
        connect(ui->sliderSmartFee, SIGNAL(valueChanged(int)), this, SLOT(updateGlobalFeeVariables()));
        connect(ui->sliderSmartFee, SIGNAL(valueChanged(int)), this, SLOT(coinControlUpdateLabels()));
        connect(ui->groupFee, SIGNAL(buttonClicked(int)), this, SLOT(updateFeeSectionControls()));
        connect(ui->groupFee, SIGNAL(buttonClicked(int)), this, SLOT(updateGlobalFeeVariables()));
        connect(ui->groupFee, SIGNAL(buttonClicked(int)), this, SLOT(coinControlUpdateLabels()));
        connect(ui->groupCustomFee, SIGNAL(buttonClicked(int)), this, SLOT(updateGlobalFeeVariables()));
        connect(ui->groupCustomFee, SIGNAL(buttonClicked(int)), this, SLOT(coinControlUpdateLabels()));
        connect(ui->customFee, SIGNAL(valueChanged()), this, SLOT(updateGlobalFeeVariables()));
        connect(ui->customFee, SIGNAL(valueChanged()), this, SLOT(coinControlUpdateLabels()));
        connect(ui->checkBoxMinimumFee, SIGNAL(stateChanged(int)), this, SLOT(setMinimumFee()));
        connect(ui->checkBoxMinimumFee, SIGNAL(stateChanged(int)), this, SLOT(updateFeeSectionControls()));
        connect(ui->checkBoxMinimumFee, SIGNAL(stateChanged(int)), this, SLOT(updateGlobalFeeVariables()));
        connect(ui->checkBoxMinimumFee, SIGNAL(stateChanged(int)), this, SLOT(coinControlUpdateLabels()));
        ui->customFee->setSingleStep(CWallet::GetRequiredFee(1000));
        updateFeeSectionControls();
        updateMinFeeLabel();
        updateFeeLabel();
        updateGlobalFeeVariables();

        // set the smartfee-sliders default value (wallets default conf.target or last stored value)
        QSettings settings;
        ui->sliderSmartFee->setValue(settings.value("nPresetFeeSliderPosition").toInt());
        refreshPqcKeyInventory();
        connect(_model, SIGNAL(walletMetaChanged(QString)), this, SLOT(refreshPqcKeyInventory()), Qt::UniqueConnection);
    }
}

SendCoinsDialog::~SendCoinsDialog()
{
    QSettings settings;
    settings.setValue("fFeeSectionMinimized", fFeeMinimized);
    settings.setValue("nFeeRadio", ui->groupFee->checkedId());
    settings.setValue("nCustomFeeRadio", ui->groupCustomFee->checkedId());
    settings.setValue("nPresetFeeSliderPosition", ui->sliderSmartFee->value());
    settings.setValue("nTransactionFee", (qint64)ui->customFee->value());
    settings.setValue("fPayOnlyMinFee", ui->checkBoxMinimumFee->isChecked());

    delete ui;
}

void SendCoinsDialog::on_sendButton_clicked()
{
    if(!model || !model->getOptionsModel())
        return;

    QList<SendCoinsRecipient> recipients;
    bool valid = true;

    for(int i = 0; i < ui->entries->count(); ++i)
    {
        SendCoinsEntry *entry = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(i)->widget());
        if(entry)
        {
            if(entry->validate())
            {
                recipients.append(entry->getValue());
            }
            else
            {
                valid = false;
            }
        }
    }

    if(!valid || recipients.isEmpty())
    {
        return;
    }

    if (pqcIncludeCommitmentCheckBox && pqcIncludeCommitmentCheckBox->isChecked()) {
        const QString commitment = pqcCommitmentLineEdit ? pqcCommitmentLineEdit->text().trimmed() : QString();
        if (commitment.isEmpty() || pqcCommitmentScriptPubKeyHex.trimmed().isEmpty()) {
            Q_EMIT message(tr("PQC Commitment"), tr("Generate a commitment before including it in the transaction."), CClientUIInterface::MSG_WARNING);
            return;
        }
        recipients[0].includePqcCommitment = true;
        recipients[0].pqcCommitmentScriptPubKey = pqcCommitmentScriptPubKeyHex.trimmed();
        recipients[0].pqcCarrierMode = (pqcCarrierModeCheckBox && pqcCarrierModeCheckBox->isChecked());
        if (recipients[0].pqcCarrierMode &&
            !pqcSelectedPublicKeyHex.isEmpty() && !pqcSelectedSignatureHex.isEmpty() &&
            IsHex(pqcSelectedPublicKeyHex.toStdString()) && IsHex(pqcSelectedSignatureHex.toStdString())) {
            size_t payloadSize = ParseHex(pqcSelectedPublicKeyHex.toStdString()).size() +
                                 ParseHex(pqcSelectedSignatureHex.toStdString()).size();
            recipients[0].pqcCarrierParts = PQCCarrierPartsNeeded(payloadSize);
            if (recipients[0].pqcCarrierParts == 0) recipients[0].pqcCarrierParts = 1;
        }
    }

    fNewRecipientAllowed = false;
    WalletModel::UnlockContext ctx(model->requestUnlock());
    if(!ctx.isValid())
    {
        // Unlock wallet was cancelled
        fNewRecipientAllowed = true;
        return;
    }

    // prepare transaction for getting txFee earlier
    WalletModelTransaction currentTransaction(recipients);
    WalletModel::SendCoinsReturn prepareStatus;

    // Always use a CCoinControl instance, use the CoinControlDialog instance if CoinControl has been enabled
    CCoinControl ctrl;
    if (model->getOptionsModel()->getCoinControlFeatures())
        ctrl = *CoinControlDialog::coinControl;
    if (ui->radioSmartFee->isChecked())
        ctrl.nPriority = static_cast<FeeRatePreset>(ui->sliderSmartFee->value());
    else
        ctrl.nPriority = MINIMUM;

    prepareStatus = model->prepareTransaction(currentTransaction, &ctrl);

    // process prepareStatus and on error generate message shown to user
    processSendCoinsReturn(prepareStatus,
        BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(), currentTransaction.getTransactionFee()));

    if(prepareStatus.status != WalletModel::OK) {
        fNewRecipientAllowed = true;
        return;
    }

    CAmount txFee = currentTransaction.getTransactionFee();

    // Format confirmation message
    QStringList formatted;
    Q_FOREACH(const SendCoinsRecipient &rcp, currentTransaction.getRecipients())
    {
        // generate bold amount string
        QString amount = "<b>" + BitcoinUnits::formatHtmlWithUnit(model->getOptionsModel()->getDisplayUnit(), rcp.amount);
        amount.append("</b>");
        // generate monospace address string
        QString address = "<span style='font-family: monospace;'>" + rcp.address;
        address.append("</span>");

        QString recipientElement;


        if(rcp.label.length() > 0) // label with address
        {
            QString displayedLabel = rcp.label;
            if (rcp.label.length() > CHARACTERS_DISPLAY_LIMIT_IN_LABEL)
            {
                displayedLabel = displayedLabel.left(CHARACTERS_DISPLAY_LIMIT_IN_LABEL).append("..."); // limit the amount of characters displayed in label
            }
            recipientElement = tr("%1 to %2").arg(amount, GUIUtil::HtmlEscape(displayedLabel));
            recipientElement.append(QString(" (%1)").arg(address));
        }
        else // just address
        {
            recipientElement = tr("%1 to %2").arg(amount, address);
        }

        formatted.append(recipientElement);
    }

    QString questionString = tr("Are you sure you want to send?");
    questionString.append("<br /><br />%1");

    if(txFee > 0)
    {
        // append fee string if a fee is required
        questionString.append("<hr /><span style='color:#aa0000;'>");
        questionString.append(BitcoinUnits::formatHtmlWithUnit(model->getOptionsModel()->getDisplayUnit(), txFee));
        questionString.append("</span> ");
        questionString.append(tr("added as transaction fee"));

        // append transaction size
        questionString.append(" (" + QString::number((double)currentTransaction.getTransactionSize() / 1000) + " kB)");
    }

    // add total amount in all subdivision units
    questionString.append("<hr />");
    CAmount totalAmount = currentTransaction.getTotalTransactionAmount() + txFee;
    QStringList alternativeUnits;
    Q_FOREACH(BitcoinUnits::Unit u, BitcoinUnits::availableUnits())
    {
        if(u != model->getOptionsModel()->getDisplayUnit())
            alternativeUnits.append(BitcoinUnits::formatHtmlWithUnit(u, totalAmount));
    }
    questionString.append(tr("Total Amount %1")
        .arg(BitcoinUnits::formatHtmlWithUnit(model->getOptionsModel()->getDisplayUnit(), totalAmount)));
    questionString.append(QString("<span style='font-size:10pt;font-weight:normal;'><br />(=%2)</span>")
        .arg(alternativeUnits.join(" " + tr("or") + "<br />")));

    bool hasPqcCarrierSend = false;
    Q_FOREACH(const SendCoinsRecipient &rcp, currentTransaction.getRecipients())
    {
        if (rcp.includePqcCommitment && rcp.pqcCarrierMode)
        {
            hasPqcCarrierSend = true;
            break;
        }
    }
    if (hasPqcCarrierSend)
    {
        questionString.append("<hr /><b>");
        questionString.append(tr("Carrier mode enabled"));
        questionString.append(":</b> ");
        questionString.append(tr("Carrier mode requires two separate transactions: TX_C (commitment) and TX_R (reveal). TX_C will be broadcast now; confirm TX_R will also be broadcast separately."));
    }

    SendConfirmationDialog confirmationDialog(tr("Confirm send coins"),
        questionString.arg(formatted.join("<br />")), SEND_CONFIRM_DELAY, this);
    confirmationDialog.exec();
    QMessageBox::StandardButton retval = (QMessageBox::StandardButton)confirmationDialog.result();

    if(retval != QMessageBox::Yes)
    {
        fNewRecipientAllowed = true;
        return;
    }

    // Store TX_C sighash (the transaction digest signed by the PQC key) for later verification
#if ENABLE_LIBOQS
    if (!pqcSigningMessageHex.isEmpty()) {
        CWalletTx *wtxPqc = currentTransaction.getTransaction();
        if (wtxPqc) {
            wtxPqc->mapValue["pqcSigningMessage"] = pqcSigningMessageHex.toStdString();
        }
    }
#endif

    // now send the prepared transaction
    WalletModel::SendCoinsReturn sendStatus = model->sendCoins(currentTransaction);
    // process sendStatus and on error generate message shown to user
    processSendCoinsReturn(sendStatus);

    if (sendStatus.status == WalletModel::OK)
    {
#if ENABLE_LIBOQS
        // If carrier mode is enabled, automatically create and broadcast TX_R
        bool carrierSent = false;
        Q_FOREACH(const SendCoinsRecipient &rcp, currentTransaction.getRecipients())
        {
            if (rcp.includePqcCommitment && rcp.pqcCarrierMode &&
                !pqcSelectedPublicKeyHex.isEmpty() && !pqcSelectedSignatureHex.isEmpty() &&
                !pqcSelectedAlgorithm.isEmpty() &&
                IsHex(pqcSelectedPublicKeyHex.toStdString()) &&
                IsHex(pqcSelectedSignatureHex.toStdString()))
            {
                PQCCommitmentType pqcType;
                if (ParsePQCCommitmentType(pqcSelectedAlgorithm.toStdString(), pqcType)) {
                    const std::vector<unsigned char> pubkeyBytes = ParseHex(pqcSelectedPublicKeyHex.toStdString());
                    const std::vector<unsigned char> sigBytes = ParseHex(pqcSelectedSignatureHex.toStdString());

                    uint256 txrTxid;
                    QString txrError;
                    const CTransaction& txc = *currentTransaction.getTransaction()->tx;
                    if (model->sendCarrierTx(txc, pqcType, pubkeyBytes, sigBytes, txrTxid, txrError)) {
                        carrierSent = true;
                        Q_EMIT message(tr("PQC Carrier"),
                            tr("TX_R (carrier reveal) broadcast successfully.\nTX_R txid: %1").arg(QString::fromStdString(txrTxid.GetHex())),
                            CClientUIInterface::MSG_INFORMATION);
                    } else {
                        Q_EMIT message(tr("PQC Carrier"),
                            tr("TX_C was sent, but TX_R (carrier reveal) failed: %1\nYou may need to manually create the TX_R.").arg(txrError),
                            CClientUIInterface::MSG_WARNING);
                    }
                }
                break;
            }
        }
#endif
        accept();
        CoinControlDialog::coinControl->UnSelectAll();
        coinControlUpdateLabels();
    }
    fNewRecipientAllowed = true;
}

void SendCoinsDialog::clear()
{
    pqcSelectedAlgorithm.clear();
    pqcSelectedPublicKeyHex.clear();
    pqcSelectedSignatureHex.clear();
    pqcSigningMessageHex.clear();
    pqcCommitmentScriptPubKeyHex.clear();
    if (pqcCommitmentLineEdit) {
        pqcCommitmentLineEdit->clear();
    }
    if (pqcIncludeCommitmentCheckBox) {
        pqcIncludeCommitmentCheckBox->setChecked(false);
    }
    if (pqcCarrierModeCheckBox) {
        pqcCarrierModeCheckBox->setChecked(false);
    }
    if (pqcDecodeButton) {
        pqcDecodeButton->setEnabled(false);
    }
    // Remove entries until only one left
    while(ui->entries->count())
    {
        ui->entries->takeAt(0)->widget()->deleteLater();
    }
    addEntry();

    updateTabsAndLabels();
}

void SendCoinsDialog::refreshPqcKeyInventory()
{
    if (!pqcKeyPairComboBox) {
        return;
    }
    pqcKeyPairComboBox->clear();
    if (!model) {
        pqcKeyPairComboBox->addItem(tr("Wallet not available"), QVariant());
        return;
    }

    struct KeyItem {
        const char* storageKey;
        const char* algorithm;
    };
    const KeyItem items[] = {
        {"pqc_sigkey_falcon512", "falcon512"},
        {"pqc_sigkey_dilithium2", "dilithium2"},
#ifdef ENABLE_LIBOQS_RACCOON
        {"pqc_sigkey_raccoong44", "raccoong44"}
#endif
    };

    for (size_t i = 0; i < sizeof(items) / sizeof(items[0]); ++i) {
        std::string metaJson;
        if (!model->getWalletMeta(items[i].storageKey, &metaJson)) {
            continue;
        }
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(metaJson), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            continue;
        }
        const QString pubHex = doc.object().value("public_key_hex").toString().trimmed();
        if (pubHex.isEmpty()) {
            continue;
        }
        const QString label = QString("%1 • %2...%3")
            .arg(QString::fromLatin1(items[i].algorithm))
            .arg(pubHex.left(10))
            .arg(pubHex.right(8));
        pqcKeyPairComboBox->addItem(label, QString::fromLatin1(items[i].algorithm) + "|" + pubHex);
    }

    if (pqcKeyPairComboBox->count() == 0) {
        pqcKeyPairComboBox->addItem(tr("No stored PQC keys. Generate keys in File > Manage PQC Keys..."), QVariant());
    }
}

void SendCoinsDialog::onUseStoredPqcKeyClicked()
{
    refreshPqcKeyInventory();
}

void SendCoinsDialog::onGeneratePqcCommitmentClicked()
{
    if (!model) {
        Q_EMIT message(tr("PQC Commitment"), tr("Wallet model is not available."), CClientUIInterface::MSG_ERROR);
        return;
    }

    if (!pqcKeyPairComboBox || !pqcKeyPairComboBox->currentData().isValid()) {
        Q_EMIT message(tr("PQC Commitment"), tr("No stored PQC signature key is selected."), CClientUIInterface::MSG_WARNING);
        return;
    }
    const QString pairData = pqcKeyPairComboBox->currentData().toString();
    const int sep = pairData.indexOf('|');
    if (sep <= 0 || sep >= pairData.size() - 1) {
        Q_EMIT message(tr("PQC Commitment"), tr("Selected PQC key entry is invalid."), CClientUIInterface::MSG_WARNING);
        return;
    }
    const QString algorithm = pairData.left(sep);
    const QString publicKeyHex = pairData.mid(sep + 1);

    QList<SendCoinsRecipient> recipients;
    for (int i = 0; i < ui->entries->count(); ++i)
    {
        SendCoinsEntry* entry = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(i)->widget());
        if (!entry) {
            continue;
        }
        const SendCoinsRecipient r = entry->getValue();
        if (!r.address.trimmed().isEmpty() && r.amount > 0) {
            recipients.append(r);
        }
    }
    if (recipients.isEmpty()) {
        Q_EMIT message(tr("PQC Commitment"), tr("Enter at least one recipient with amount before generating a transaction commitment."), CClientUIInterface::MSG_WARNING);
        return;
    }

#if ENABLE_LIBOQS
    // Parse the PQC algorithm type
    PQCCommitmentType pqcType;
    if (!ParsePQCCommitmentType(algorithm.toStdString(), pqcType)) {
        Q_EMIT message(tr("PQC Commitment"), tr("Unknown PQC algorithm: %1").arg(algorithm), CClientUIInterface::MSG_ERROR);
        return;
    }

    // Build the deterministic 32-byte signing message from tx parameters
    const QByteArray message32 = BuildPqcSigningMessage(algorithm, publicKeyHex, recipients);

    // Retrieve the stored key metadata to get the encrypted private key
    const char* storageKeys[] = {
        "pqc_sigkey_falcon512",
        "pqc_sigkey_dilithium2",
#ifdef ENABLE_LIBOQS_RACCOON
        "pqc_sigkey_raccoong44",
#endif
    };
    std::string metaJson;
    bool foundKey = false;
    for (size_t i = 0; i < sizeof(storageKeys) / sizeof(storageKeys[0]); ++i) {
        if (model->getWalletMeta(storageKeys[i], &metaJson)) {
            QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(metaJson));
            if (doc.isObject()) {
                const QString storedPubHex = doc.object().value("public_key_hex").toString().trimmed();
                if (storedPubHex == publicKeyHex) {
                    foundKey = true;
                    break;
                }
            }
        }
    }
    if (!foundKey || metaJson.empty()) {
        Q_EMIT message(tr("PQC Commitment"), tr("Could not find stored PQC key metadata for the selected public key."), CClientUIInterface::MSG_ERROR);
        return;
    }

    QJsonDocument metaDoc = QJsonDocument::fromJson(QByteArray::fromStdString(metaJson));
    QJsonObject metaObj = metaDoc.object();
    const QString encryptedHex = metaObj.value("encrypted_private_key_hex").toString().trimmed();
    const QString saltHex = metaObj.value("salt_hex").toString().trimmed();
    const int kdfRounds = metaObj.value("kdf_rounds").toInt(25000);

    if (encryptedHex.isEmpty() || saltHex.isEmpty()) {
        Q_EMIT message(tr("PQC Commitment"), tr("Stored PQC key is missing encrypted private key or salt."), CClientUIInterface::MSG_ERROR);
        return;
    }

    // Prompt user for passphrase to decrypt the PQC private key
    bool ok = false;
    QString passphrase = QInputDialog::getText(
        this,
        tr("PQC Signature — Decrypt Private Key"),
        tr("Enter passphrase to decrypt PQC private key for signing:"),
        QLineEdit::Password,
        QString(),
        &ok);
    if (!ok || passphrase.isEmpty()) {
        Q_EMIT message(tr("PQC Commitment"), tr("PQC signing cancelled."), CClientUIInterface::MSG_WARNING);
        return;
    }

    // Decrypt the private key
    std::vector<unsigned char> encryptedPrivate = ParseHex(encryptedHex.toStdString());
    std::vector<unsigned char> salt = ParseHex(saltHex.toStdString());
    QByteArray passphraseBytes = passphrase.toUtf8();
    SecureString pass(passphraseBytes.constData(), passphraseBytes.constData() + passphraseBytes.size());
    memory_cleanse(passphraseBytes.data(), passphraseBytes.size());

    CCrypter keyCrypter;
    if (!keyCrypter.SetKeyFromPassphrase(pass, salt, kdfRounds, 0)) {
        Q_EMIT message(tr("PQC Commitment"), tr("Failed to derive decryption key from passphrase."), CClientUIInterface::MSG_ERROR);
        return;
    }

    CKeyingMaterial decryptedMaterial;
    if (!keyCrypter.Decrypt(encryptedPrivate, decryptedMaterial)) {
        Q_EMIT message(tr("PQC Commitment"), tr("Failed to decrypt PQC private key. Wrong passphrase?"), CClientUIInterface::MSG_ERROR);
        return;
    }

    // Sign the message with the real PQC private key using liboqs
    std::vector<unsigned char> secretKey(decryptedMaterial.begin(), decryptedMaterial.end());
    memory_cleanse(&decryptedMaterial[0], decryptedMaterial.size());

    std::vector<unsigned char> signatureBytes;
    bool signOk = PQCSign(pqcType, secretKey,
                           reinterpret_cast<const unsigned char*>(message32.constData()),
                           message32.size(), signatureBytes);
    memory_cleanse(secretKey.data(), secretKey.size());

    if (!signOk || signatureBytes.empty()) {
        Q_EMIT message(tr("PQC Commitment"), tr("PQC signing failed. The secret key may be invalid or corrupted."), CClientUIInterface::MSG_ERROR);
        return;
    }

    const QString signatureHex = QString::fromStdString(HexStr(signatureBytes.begin(), signatureBytes.end()));
#else
    Q_EMIT message(tr("PQC Commitment"), tr("PQC signing requires liboqs. Rebuild with --with-liboqs --enable-experimental."), CClientUIInterface::MSG_ERROR);
    return;
    const QByteArray message32;
    const QString signatureHex;
#endif

    try {
        UniValue params(UniValue::VARR);
        params.push_back(UniValue(algorithm.trimmed().toStdString()));
        params.push_back(UniValue(publicKeyHex.trimmed().toStdString()));
        params.push_back(UniValue(signatureHex.trimmed().toStdString()));
        JSONRPCRequest req;
        req.strMethod = "generatepqccommitment";
        req.params = params;
        req.fHelp = false;
        UniValue out = tableRPC.execute(req);
        const UniValue& outObj = out.get_obj();
        const QString commitment = QString::fromStdString(find_value(outObj, "commitment").get_str());
        const QString scriptPubKey = QString::fromStdString(find_value(outObj, "scriptPubKey").get_str());

        pqcSelectedAlgorithm = algorithm;
        pqcSelectedPublicKeyHex = publicKeyHex;
        pqcSelectedSignatureHex = signatureHex;
        pqcSigningMessageHex = QString::fromLatin1(message32.toHex());
        pqcCommitmentScriptPubKeyHex = scriptPubKey;
        pqcCommitmentLineEdit->setText(commitment);
        pqcDecodeButton->setEnabled(!commitment.isEmpty() && !pqcCommitmentScriptPubKeyHex.isEmpty());
    } catch (const std::exception& e) {
        Q_EMIT message(tr("PQC Commitment"), tr("Error: %1").arg(QString::fromStdString(e.what())), CClientUIInterface::MSG_ERROR);
    }
}

void SendCoinsDialog::onDecodePqcCommitmentClicked()
{
    const QString commitment = pqcCommitmentLineEdit ? pqcCommitmentLineEdit->text().trimmed() : QString();
    const QString scriptPubKey = pqcCommitmentScriptPubKeyHex.trimmed();
    if (commitment.isEmpty() || scriptPubKey.isEmpty()) {
        Q_EMIT message(tr("Decode PQC Commitment"), tr("Generate a PQC commitment first."), CClientUIInterface::MSG_WARNING);
        return;
    }

    QString algorithmTag = tr("Unknown");
    QString extractedCommitment = tr("n/a");
#if ENABLE_LIBOQS
    PQCCommitmentType detectedType = PQCCommitmentType::FALCON512;
    bool commitmentExtracted = false;
    if (IsHex(scriptPubKey.toStdString())) {
        const std::vector<unsigned char> scriptBytes = ParseHex(scriptPubKey.toStdString());
        CScript script(scriptBytes.begin(), scriptBytes.end());
        PQCCommitmentType pqcType;
        uint256 extracted;
        if (PQCExtractCommitment(script, pqcType, extracted)) {
            algorithmTag = QString::fromLatin1(PQCCommitmentTypeToString(pqcType));
            extractedCommitment = QString::fromStdString(extracted.GetHex());
            detectedType = pqcType;
            commitmentExtracted = true;
        }
    }
#else
    bool commitmentExtracted = false;
#endif

    const bool carrierEnabled = pqcCarrierModeCheckBox && pqcCarrierModeCheckBox->isChecked();

    // Use the real PQC signature stored during commitment generation
    const QString pqcSignatureHex = pqcSelectedSignatureHex;

    // Build HTML matching the transaction details style (<b>Label:</b> value<br>)
    QString html;
    html += "<html><body style=\"word-break: break-all;\">";
    html += "<b>" + tr("Selected key algorithm") + ":</b> " + GUIUtil::HtmlEscape(pqcSelectedAlgorithm.isEmpty() ? tr("unknown") : pqcSelectedAlgorithm) + "<br>";
    html += "<b>" + tr("Selected public key") + ":</b> " + GUIUtil::HtmlEscape(pqcSelectedPublicKeyHex.isEmpty() ? tr("n/a") : pqcSelectedPublicKeyHex) + "<br>";
    if (!pqcSelectedPublicKeyHex.isEmpty() && IsHex(pqcSelectedPublicKeyHex.toStdString())) {
        html += "<b>" + tr("Selected public key size") + ":</b> " + QString::number(ParseHex(pqcSelectedPublicKeyHex.toStdString()).size()) + " " + tr("bytes") + "<br>";
    }
    html += "<b>" + tr("Selected PQC signature") + ":</b> " + GUIUtil::HtmlEscape(pqcSignatureHex.isEmpty() ? tr("n/a") : pqcSignatureHex) + "<br>";
    if (!pqcSignatureHex.isEmpty() && IsHex(pqcSignatureHex.toStdString())) {
        html += "<b>" + tr("Selected PQC signature size") + ":</b> " + QString::number(ParseHex(pqcSignatureHex.toStdString()).size()) + " " + tr("bytes") + "<br>";
    }
    html += "<b>" + tr("Commitment") + ":</b> " + GUIUtil::HtmlEscape(commitment) + "<br>";
    html += "<b>" + tr("OP_RETURN scriptPubKey") + ":</b> " + GUIUtil::HtmlEscape(scriptPubKey) + "<br>";
    html += "<b>" + tr("Script starts with OP_RETURN") + ":</b> " + (scriptPubKey.startsWith("6a24", Qt::CaseInsensitive) ? tr("yes") : tr("no")) + "<br>";
    html += "<b>" + tr("Algorithm tag") + ":</b> " + GUIUtil::HtmlEscape(algorithmTag) + "<br>";
    html += "<b>" + tr("Extracted commitment from script") + ":</b> " + GUIUtil::HtmlEscape(extractedCommitment) + "<br>";
    html += "<b>" + tr("Carrier mode") + ":</b> " + (carrierEnabled ? tr("enabled (TX_C + TX_R P2SH data carrier)") : tr("disabled (commitment-only)")) + "<br>";

    // Decode TX_C and TX_R carrier details when carrier mode is enabled
#if ENABLE_LIBOQS
    if (carrierEnabled && commitmentExtracted) {
        html += "<br>";
        html += "<b>" + tr("TX_C (Commitment Transaction)") + ":</b><br>";
        html += "<b>" + tr("TX_C OP_RETURN output") + ":</b> " + GUIUtil::HtmlEscape(scriptPubKey) + "<br>";

        // Compute carrier P2SH scriptPubKey
        CScript carrierScriptPubKey;
        if (PQCBuildCarrierScriptPubKey(carrierScriptPubKey)) {
            const std::string carrierSpkHex = HexStr(carrierScriptPubKey.begin(), carrierScriptPubKey.end());
            html += "<b>" + tr("TX_C carrier P2SH scriptPubKey") + ":</b> " + GUIUtil::HtmlEscape(QString::fromStdString(carrierSpkHex)) + "<br>";

            // Extract the P2SH address from the scriptPubKey
            CTxDestination dest;
            if (ExtractDestination(carrierScriptPubKey, dest)) {
                html += "<b>" + tr("TX_C carrier P2SH address") + ":</b> " + GUIUtil::HtmlEscape(QString::fromStdString(CBitcoinAddress(dest).ToString())) + "<br>";
            }
        }

        // Show carrier redeemScript
        CScript redeemScript;
        if (PQCBuildCarrierRedeemScript(redeemScript)) {
            const std::string redeemHex = HexStr(redeemScript.begin(), redeemScript.end());
            html += "<b>" + tr("TX_C carrier redeemScript") + ":</b> " + GUIUtil::HtmlEscape(QString::fromStdString(redeemHex)) + " (OP_DROP×5 OP_TRUE)<br>";
        }

        // Compute carrier parts needed based on selected public key size
        size_t pkLen = 0;
        if (!pqcSelectedPublicKeyHex.isEmpty() && IsHex(pqcSelectedPublicKeyHex.toStdString())) {
            pkLen = ParseHex(pqcSelectedPublicKeyHex.toStdString()).size();
        }
        // Use actual auto-generated signature size
        const size_t sigLen = (!pqcSignatureHex.isEmpty() && IsHex(pqcSignatureHex.toStdString()))
            ? ParseHex(pqcSignatureHex.toStdString()).size() : 0;
        const size_t payloadSize = pkLen + sigLen;
        const uint8_t partsNeeded = PQCCarrierPartsNeeded(payloadSize);

        html += "<b>" + tr("Estimated carrier payload") + ":</b> " + QString::number(payloadSize) + " " + tr("bytes") + " (" + tr("pk") + "=" + QString::number(pkLen) + " + " + tr("sig") + "=" + QString::number(sigLen) + ")<br>";
        html += "<b>" + tr("Carrier parts needed") + ":</b> " + QString::number(partsNeeded) + "<br>";

        // Determine carrier tag
        QString carrierTag;
        switch (detectedType) {
            case PQCCommitmentType::FALCON512:  carrierTag = "FLC1FULL"; break;
            case PQCCommitmentType::DILITHIUM2: carrierTag = "DIL2FULL"; break;
#ifdef ENABLE_LIBOQS_RACCOON
            case PQCCommitmentType::RACCOONG44: carrierTag = "RCG4FULL"; break;
#endif
        }
        html += "<b>" + tr("Carrier tag (8-byte)") + ":</b> " + GUIUtil::HtmlEscape(carrierTag) + "<br>";

        html += "<br>";
        html += "<b>" + tr("TX_R (Reveal Transaction)") + ":</b><br>";

        // Build actual TX_R carrier scriptSig data for each part
        if (!pqcSelectedPublicKeyHex.isEmpty() && IsHex(pqcSelectedPublicKeyHex.toStdString())
            && !pqcSignatureHex.isEmpty() && IsHex(pqcSignatureHex.toStdString())) {
            const std::vector<unsigned char> pubkeyBytes = ParseHex(pqcSelectedPublicKeyHex.toStdString());
            const std::vector<unsigned char> sigBytes = ParseHex(pqcSignatureHex.toStdString());

            for (uint8_t p = 0; p < partsNeeded; ++p) {
                CScript partScriptSig;
                if (PQCBuildCarrierPartScriptSig(detectedType, pubkeyBytes, sigBytes, p, partScriptSig)) {
                    const std::string ssHex = HexStr(partScriptSig.begin(), partScriptSig.end());
                    html += "<b>" + tr("TX_R scriptSig part %1/%2").arg(p + 1).arg(partsNeeded) + ":</b> "
                          + GUIUtil::HtmlEscape(QString::fromStdString(ssHex)) + "<br>";
                    html += "<b>" + tr("TX_R part %1 size").arg(p + 1) + ":</b> "
                          + QString::number(partScriptSig.size()) + " " + tr("bytes") + "<br>";
                }
            }

            // Show PQC public key and signature separately (for visual comparison with header)
            const std::string pubkeyHexStr = HexStr(pubkeyBytes.begin(), pubkeyBytes.end());
            const std::string sigHexStr = HexStr(sigBytes.begin(), sigBytes.end());
            html += "<b>" + tr("TX_R PQC public key") + ":</b> "
                  + GUIUtil::HtmlEscape(QString::fromStdString(pubkeyHexStr)) + "<br>";
            html += "<b>" + tr("TX_R PQC public key size") + ":</b> " + QString::number(pubkeyBytes.size()) + " " + tr("bytes") + "<br>";
            html += "<b>" + tr("TX_R PQC signature") + ":</b> "
                  + GUIUtil::HtmlEscape(QString::fromStdString(sigHexStr)) + "<br>";
            html += "<b>" + tr("TX_R PQC signature size") + ":</b> " + QString::number(sigBytes.size()) + " " + tr("bytes") + "<br>";

            // Validate TX_R public key matches selected public key
            const bool pkMatch = (pubkeyHexStr == pqcSelectedPublicKeyHex.toStdString());
            html += "<b>" + tr("TX_R public key matches selected") + ":</b> "
                  + (pkMatch ? tr("yes") : tr("NO — MISMATCH")) + "<br>";

            // Validate TX_R signature matches auto-generated signature
            const bool sigMatch = (sigHexStr == pqcSignatureHex.toStdString());
            html += "<b>" + tr("TX_R signature matches selected") + ":</b> "
                  + (sigMatch ? tr("yes") : tr("NO — MISMATCH")) + "<br>";

            // Show full payload hex (pk || sig)
            std::vector<unsigned char> fullPayload;
            fullPayload.reserve(pubkeyBytes.size() + sigBytes.size());
            fullPayload.insert(fullPayload.end(), pubkeyBytes.begin(), pubkeyBytes.end());
            fullPayload.insert(fullPayload.end(), sigBytes.begin(), sigBytes.end());
            const std::string fullPayloadHex = HexStr(fullPayload.begin(), fullPayload.end());
            html += "<b>" + tr("TX_R full payload (pk||sig)") + ":</b> "
                  + GUIUtil::HtmlEscape(QString::fromStdString(fullPayloadHex)) + "<br>";
            html += "<b>" + tr("TX_R payload size") + ":</b> " + QString::number(fullPayload.size()) + " " + tr("bytes") + "<br>";

            // Verify commitment matches SHA256(pk || sig)
            CSHA256 hasher;
            hasher.Write(fullPayload.data(), fullPayload.size());
            unsigned char hash[CSHA256::OUTPUT_SIZE];
            hasher.Finalize(hash);
            uint256 recomputedCommitment;
            memcpy(recomputedCommitment.begin(), hash, 32);
            html += "<b>" + tr("TX_R SHA256(pk||sig)") + ":</b> " + GUIUtil::HtmlEscape(QString::fromStdString(recomputedCommitment.GetHex()))
                  + " " + tr("(included in both TX_C OP_RETURN and TX_R carrier)") + "<br>";
            const bool commitmentMatch = (recomputedCommitment.GetHex() == commitment.toStdString());
            html += "<b>" + tr("Commitment matches") + ":</b> " + (commitmentMatch ? tr("yes") : tr("NO — MISMATCH")) + "<br>";

            // Overall signature validation summary
            html += "<br>";

            // Perform real cryptographic PQC signature verification using liboqs
            bool cryptoVerified = false;
            if (pkMatch && commitmentMatch && !pqcSigningMessageHex.isEmpty()
                && IsHex(pqcSigningMessageHex.toStdString())) {
                const std::vector<unsigned char> messageBytes = ParseHex(pqcSigningMessageHex.toStdString());
                cryptoVerified = PQCVerify(detectedType, pubkeyBytes,
                                           messageBytes.data(), messageBytes.size(),
                                           sigBytes);
            }

            html += "<b>" + tr("TX_C sighash (hex)") + ":</b> "
                  + GUIUtil::HtmlEscape(pqcSigningMessageHex.isEmpty() ? tr("n/a") : pqcSigningMessageHex) + "<br>";
            html += "<b>" + tr("OQS_SIG_verify() cryptographic check") + ":</b> "
                  + (cryptoVerified
                       ? "<span style=\"color:green;\">" + tr("PASSED") + "</span>"
                       : "<span style=\"color:red;\">" + tr("FAILED") + "</span>") + "<br>";

            if (pkMatch && sigMatch && commitmentMatch && cryptoVerified) {
                html += "<b>" + tr("PQC signature validation") + ":</b> <span style=\"color:green;\">"
                      + tr("PASSED — public key, signature, commitment, and cryptographic verification all verified") + "</span><br>";
            } else {
                QString failReason;
                if (!pkMatch) failReason += tr("public key mismatch") + "; ";
                if (!sigMatch) failReason += tr("signature mismatch") + "; ";
                if (!commitmentMatch) failReason += tr("commitment mismatch") + "; ";
                if (!cryptoVerified) failReason += tr("OQS_SIG_verify() failed") + "; ";
                html += "<b>" + tr("PQC signature validation") + ":</b> <span style=\"color:red;\">"
                      + tr("FAILED — %1").arg(failReason) + "</span><br>";
            }
        } else {
            html += "<b>" + tr("TX_R data") + ":</b> " + tr("public key not available for scriptSig computation") + "<br>";
        }
    }
#endif

    html += "</body></html>";

    QDialog decodeDialog(this);
    decodeDialog.setWindowTitle(tr("Decoded PQC Commitment"));
    decodeDialog.resize(820, 560);
    QVBoxLayout* layout = new QVBoxLayout(&decodeDialog);
    QLabel* label = new QLabel(tr("Decoded PQC commitment details:"), &decodeDialog);
    layout->addWidget(label);
    QTextEdit* decodedText = new QTextEdit(&decodeDialog);
    decodedText->setReadOnly(true);
    decodedText->setHtml(html);
    layout->addWidget(decodedText);
    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &decodeDialog);
    connect(buttons, &QDialogButtonBox::rejected, &decodeDialog, &QDialog::reject);
    layout->addWidget(buttons);
    decodeDialog.exec();
}

void SendCoinsDialog::reject()
{
    clear();
}

void SendCoinsDialog::accept()
{
    clear();
}

SendCoinsEntry *SendCoinsDialog::addEntry()
{
    SendCoinsEntry *entry = new SendCoinsEntry(platformStyle, this);
    entry->setModel(model);
    ui->entries->addWidget(entry);
    connect(entry, SIGNAL(removeEntry(SendCoinsEntry*)), this, SLOT(removeEntry(SendCoinsEntry*)));
    connect(entry, SIGNAL(payAmountChanged()), this, SLOT(coinControlUpdateLabels()));
    connect(entry, SIGNAL(subtractFeeFromAmountChanged()), this, SLOT(coinControlUpdateLabels()));

    // Focus the field, so that entry can start immediately
    entry->clear();
    entry->setFocus();
    ui->scrollAreaWidgetContents->resize(ui->scrollAreaWidgetContents->sizeHint());
    qApp->processEvents();
    QScrollBar* bar = ui->scrollArea->verticalScrollBar();
    if(bar)
        bar->setSliderPosition(bar->maximum());

    updateTabsAndLabels();
    return entry;
}

void SendCoinsDialog::updateTabsAndLabels()
{
    setupTabChain(0);
    coinControlUpdateLabels();
}

void SendCoinsDialog::removeEntry(SendCoinsEntry* entry)
{
    entry->hide();

    // If the last entry is about to be removed add an empty one
    if (ui->entries->count() == 1)
        addEntry();

    entry->deleteLater();

    updateTabsAndLabels();
}

QWidget *SendCoinsDialog::setupTabChain(QWidget *prev)
{
    for(int i = 0; i < ui->entries->count(); ++i)
    {
        SendCoinsEntry *entry = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(i)->widget());
        if(entry)
        {
            prev = entry->setupTabChain(prev);
        }
    }
    QWidget::setTabOrder(prev, ui->sendButton);
    QWidget::setTabOrder(ui->sendButton, ui->clearButton);
    QWidget::setTabOrder(ui->clearButton, ui->addButton);
    return ui->addButton;
}

void SendCoinsDialog::setAddress(const QString &address)
{
    SendCoinsEntry *entry = 0;
    // Replace the first entry if it is still unused
    if(ui->entries->count() == 1)
    {
        SendCoinsEntry *first = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(0)->widget());
        if(first->isClear())
        {
            entry = first;
        }
    }
    if(!entry)
    {
        entry = addEntry();
    }

    entry->setAddress(address);
}

void SendCoinsDialog::pasteEntry(const SendCoinsRecipient &rv)
{
    if(!fNewRecipientAllowed)
        return;

    SendCoinsEntry *entry = 0;
    // Replace the first entry if it is still unused
    if(ui->entries->count() == 1)
    {
        SendCoinsEntry *first = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(0)->widget());
        if(first->isClear())
        {
            entry = first;
        }
    }
    if(!entry)
    {
        entry = addEntry();
    }

    entry->setValue(rv);
    updateTabsAndLabels();
}

bool SendCoinsDialog::handlePaymentRequest(const SendCoinsRecipient &rv)
{
    // Just paste the entry, all pre-checks
    // are done in paymentserver.cpp.
    pasteEntry(rv);
    return true;
}

void SendCoinsDialog::setBalance(const CAmount& balance, const CAmount& unconfirmedBalance, const CAmount& immatureBalance,
                                 const CAmount& watchBalance, const CAmount& watchUnconfirmedBalance, const CAmount& watchImmatureBalance)
{
    Q_UNUSED(unconfirmedBalance);
    Q_UNUSED(immatureBalance);
    Q_UNUSED(watchBalance);
    Q_UNUSED(watchUnconfirmedBalance);
    Q_UNUSED(watchImmatureBalance);

    if(model && model->getOptionsModel())
    {
        ui->labelBalance->setText(BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(), balance));
    }
}

void SendCoinsDialog::updateDisplayUnit()
{
    setBalance(model->getBalance(), 0, 0, 0, 0, 0);
    ui->customFee->setDisplayUnit(model->getOptionsModel()->getDisplayUnit());
    updateMinFeeLabel();
    updateFeeLabel();
}

void SendCoinsDialog::processSendCoinsReturn(const WalletModel::SendCoinsReturn &sendCoinsReturn, const QString &msgArg)
{
    QPair<QString, CClientUIInterface::MessageBoxFlags> msgParams;
    // Default to a warning message, override if error message is needed
    msgParams.second = CClientUIInterface::MSG_WARNING;

    // This comment is specific to SendCoinsDialog usage of WalletModel::SendCoinsReturn.
    // WalletModel::TransactionCommitFailed is used only in WalletModel::sendCoins()
    // all others are used only in WalletModel::prepareTransaction()
    switch(sendCoinsReturn.status)
    {
    case WalletModel::InvalidAddress:
        msgParams.first = tr("The recipient address is not valid. Please recheck.");
        break;
    case WalletModel::InvalidAmount:
        msgParams.first = tr("The amount to pay must be larger than 0.");
        break;
    case WalletModel::AmountExceedsBalance:
        msgParams.first = tr("The amount exceeds your balance.");
        break;
    case WalletModel::AmountWithFeeExceedsBalance:
        msgParams.first = tr("The total exceeds your balance when the %1 transaction fee is included.").arg(msgArg);
        break;
    case WalletModel::DuplicateAddress:
        msgParams.first = tr("Duplicate address found: addresses should only be used once each.");
        break;
    case WalletModel::TransactionCreationFailed:
        msgParams.first = tr("Transaction creation failed!");
        msgParams.second = CClientUIInterface::MSG_ERROR;
        break;
    case WalletModel::TransactionCommitFailed:
        msgParams.first = tr("The transaction was rejected with the following reason: %1").arg(sendCoinsReturn.reasonCommitFailed);
        msgParams.second = CClientUIInterface::MSG_ERROR;
        break;
    case WalletModel::AbsurdFee:
        msgParams.first = tr("A fee higher than %1 is considered an absurdly high fee.").arg(BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(), maxTxFee));
        break;
    // included to prevent a compiler warning.
    case WalletModel::OK:
    default:
        return;
    }

    Q_EMIT message(tr("Send Coins"), msgParams.first, msgParams.second);
}

void SendCoinsDialog::minimizeFeeSection(bool fMinimize)
{
    ui->labelFeeMinimized->setVisible(fMinimize);
    ui->buttonChooseFee  ->setVisible(fMinimize);
    ui->buttonMinimizeFee->setVisible(!fMinimize);
    ui->frameFeeSelection->setVisible(!fMinimize);
    ui->horizontalLayoutSmartFee->setContentsMargins(0, (fMinimize ? 0 : 6), 0, 0);
    fFeeMinimized = fMinimize;
}

void SendCoinsDialog::on_buttonChooseFee_clicked()
{
    minimizeFeeSection(false);
}

void SendCoinsDialog::on_buttonMinimizeFee_clicked()
{
    updateFeeMinimizedLabel();
    minimizeFeeSection(true);
}

void SendCoinsDialog::setMinimumFee()
{
    ui->radioCustomPerKilobyte->setChecked(true);
    ui->customFee->setValue(CWallet::GetRequiredFee(1000));
}

void SendCoinsDialog::updateFeeSectionControls()
{
    ui->sliderSmartFee          ->setEnabled(ui->radioSmartFee->isChecked());
    ui->labelPriority           ->setEnabled(ui->radioSmartFee->isChecked());
    // Dogecoin: We don't use smart fees in the UI, so don't need to warn they're not available
    // ui->labelPriority2          ->setEnabled(ui->radioSmartFee->isChecked());
    ui->labelPriority3          ->setEnabled(ui->radioSmartFee->isChecked());
    ui->labelFeeEstimation      ->setEnabled(ui->radioSmartFee->isChecked());
    ui->labelPriorityLow        ->setEnabled(ui->radioSmartFee->isChecked());
    ui->labelPriorityHigh       ->setEnabled(ui->radioSmartFee->isChecked());
    ui->confirmationTargetLabel ->setEnabled(ui->radioSmartFee->isChecked());
    ui->checkBoxMinimumFee      ->setEnabled(ui->radioCustomFee->isChecked());
    ui->labelMinFeeWarning      ->setEnabled(ui->radioCustomFee->isChecked());
    ui->radioCustomPerKilobyte  ->setEnabled(ui->radioCustomFee->isChecked() && !ui->checkBoxMinimumFee->isChecked());
    ui->radioCustomAtLeast      ->setEnabled(ui->radioCustomFee->isChecked() && !ui->checkBoxMinimumFee->isChecked() && CoinControlDialog::coinControl->HasSelected());
    ui->customFee               ->setEnabled(ui->radioCustomFee->isChecked() && !ui->checkBoxMinimumFee->isChecked());
}

void SendCoinsDialog::updateGlobalFeeVariables()
{
    if (ui->radioSmartFee->isChecked())
    {
        int nPriority = ui->sliderSmartFee->value();
        payTxFee = CFeeRate(0);

        // set nMinimumTotalFee to 0 to not accidentally pay a custom fee
        CoinControlDialog::coinControl->nMinimumTotalFee = 0;

        // show the estimated required time for confirmation
        // Dogecoin: We manually set height well past the last hard fork here
        ui->confirmationTargetLabel->setText(GetDogecoinPriorityLabel(nPriority).c_str());
    }
    else
    {
        payTxFee = CFeeRate(ui->customFee->value());

        // if user has selected to set a minimum absolute fee, pass the value to coincontrol
        // set nMinimumTotalFee to 0 in case of user has selected that the fee is per KB
        CoinControlDialog::coinControl->nMinimumTotalFee = ui->radioCustomAtLeast->isChecked() ? ui->customFee->value() : 0;
    }
}

void SendCoinsDialog::updateFeeMinimizedLabel()
{
    if(!model || !model->getOptionsModel())
        return;

    if (ui->radioSmartFee->isChecked())
        ui->labelFeeMinimized->setText(ui->labelPriority->text());
    else {
        ui->labelFeeMinimized->setText(BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(), ui->customFee->value()) +
            ((ui->radioCustomPerKilobyte->isChecked()) ? "/kB" : ""));
    }
}

void SendCoinsDialog::updateMinFeeLabel()
{
    if (model && model->getOptionsModel())
        ui->checkBoxMinimumFee->setText(tr("Pay only the required fee of %1").arg(
            BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(), CWallet::GetRequiredFee(1000)) + "/kB")
        );
}

void SendCoinsDialog::updateFeeLabel()
{
    if(!model || !model->getOptionsModel())
        return;

    int nPriority = ui->sliderSmartFee->value();
    CFeeRate feeRate = GetDogecoinFeeRate(nPriority);
    if (feeRate <= CFeeRate(0)) // not enough data => minfee
    {
        ui->labelPriority->setText(BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(),
                                                                std::max(CWallet::fallbackFee.GetFeePerK(), CWallet::GetRequiredFee(1000))) + "/kB");
        // ui->labelPriority2->show(); // (Smart fee not initialized yet. This usually takes a few blocks...)
        ui->labelFeeEstimation->setText("");
        ui->fallbackFeeWarningLabel->setVisible(true);
        int lightness = ui->fallbackFeeWarningLabel->palette().color(QPalette::WindowText).lightness();
        QColor warning_colour(255 - (lightness / 5), 176 - (lightness / 3), 48 - (lightness / 14));
        ui->fallbackFeeWarningLabel->setStyleSheet("QLabel { color: " + warning_colour.name() + "; }");
        ui->fallbackFeeWarningLabel->setIndent(QFontMetrics(ui->fallbackFeeWarningLabel->font()).width("x"));
    }
    else
    {
        ui->labelPriority->setText(BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(),
                                                                std::max(feeRate.GetFeePerK(), CWallet::GetRequiredFee(1000))) + "/kB");
        // ui->labelPriority2->hide();
        // Dogecoin: We don't use smart fees, so we don't have the data to estimate when it will get in
        ui->labelFeeEstimation->setText("");
        // ui->labelFeeEstimation->setText(tr("Estimated to begin confirmation within %n block(s).", "", estimateFoundAtBlocks));
        ui->fallbackFeeWarningLabel->setVisible(false);
    }

    updateFeeMinimizedLabel();
}

// Coin Control: copy label "Quantity" to clipboard
void SendCoinsDialog::coinControlClipboardQuantity()
{
    GUIUtil::setClipboard(ui->labelCoinControlQuantity->text());
}

// Coin Control: copy label "Amount" to clipboard
void SendCoinsDialog::coinControlClipboardAmount()
{
    GUIUtil::setClipboard(ui->labelCoinControlAmount->text().left(ui->labelCoinControlAmount->text().indexOf(" ")));
}

// Coin Control: copy label "Fee" to clipboard
void SendCoinsDialog::coinControlClipboardFee()
{
    GUIUtil::setClipboard(ui->labelCoinControlFee->text().left(ui->labelCoinControlFee->text().indexOf(" ")).replace(ASYMP_UTF8, ""));
}

// Coin Control: copy label "After fee" to clipboard
void SendCoinsDialog::coinControlClipboardAfterFee()
{
    GUIUtil::setClipboard(ui->labelCoinControlAfterFee->text().left(ui->labelCoinControlAfterFee->text().indexOf(" ")).replace(ASYMP_UTF8, ""));
}

// Coin Control: copy label "Bytes" to clipboard
void SendCoinsDialog::coinControlClipboardBytes()
{
    GUIUtil::setClipboard(ui->labelCoinControlBytes->text().replace(ASYMP_UTF8, ""));
}

// Coin Control: copy label "Dust" to clipboard
void SendCoinsDialog::coinControlClipboardLowOutput()
{
    GUIUtil::setClipboard(ui->labelCoinControlLowOutput->text());
}

// Coin Control: copy label "Change" to clipboard
void SendCoinsDialog::coinControlClipboardChange()
{
    GUIUtil::setClipboard(ui->labelCoinControlChange->text().left(ui->labelCoinControlChange->text().indexOf(" ")).replace(ASYMP_UTF8, ""));
}

// Coin Control: settings menu - coin control enabled/disabled by user
void SendCoinsDialog::coinControlFeatureChanged(bool checked)
{
    ui->frameCoinControl->setVisible(checked);

    if (!checked && model) // coin control features disabled
        CoinControlDialog::coinControl->SetNull();

    // make sure we set back the confirmation target
    updateGlobalFeeVariables();
    coinControlUpdateLabels();
}

// Coin Control: button inputs -> show actual coin control dialog
void SendCoinsDialog::coinControlButtonClicked()
{
    CoinControlDialog dlg(platformStyle);
    dlg.setModel(model);
    dlg.exec();
    coinControlUpdateLabels();
}

// Coin Control: checkbox custom change address
void SendCoinsDialog::coinControlChangeChecked(int state)
{
    if (state == Qt::Unchecked)
    {
        CoinControlDialog::coinControl->destChange = CNoDestination();
        ui->labelCoinControlChangeLabel->clear();
    }
    else
        // use this to re-validate an already entered address
        coinControlChangeEdited(ui->lineEditCoinControlChange->text());

    ui->lineEditCoinControlChange->setEnabled((state == Qt::Checked));
}

// Coin Control: custom change address changed
void SendCoinsDialog::coinControlChangeEdited(const QString& text)
{
    if (model && model->getAddressTableModel())
    {
        // Default to no change address until verified
        CoinControlDialog::coinControl->destChange = CNoDestination();
        ui->labelCoinControlChangeLabel->setStyleSheet("QLabel{color:red;}");

        CBitcoinAddress addr = CBitcoinAddress(text.toStdString());

        if (text.isEmpty()) // Nothing entered
        {
            ui->labelCoinControlChangeLabel->setText("");
        }
        else if (!addr.IsValid()) // Invalid address
        {
            ui->labelCoinControlChangeLabel->setText(tr("Warning: Invalid Dogecoin address"));
        }
        else // Valid address
        {
            CKeyID keyid;
            addr.GetKeyID(keyid);
            if (!model->havePrivKey(keyid)) // Unknown change address
            {
                ui->labelCoinControlChangeLabel->setText(tr("Warning: Unknown change address"));

                // confirmation dialog
                QMessageBox::StandardButton btnRetVal = QMessageBox::question(this, tr("Confirm custom change address"), tr("The address you selected for change is not part of this wallet. Any or all funds in your wallet may be sent to this address. Are you sure?"),
                    QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);

                if(btnRetVal == QMessageBox::Yes)
                    CoinControlDialog::coinControl->destChange = addr.Get();
                else
                {
                    ui->lineEditCoinControlChange->setText("");
                    ui->labelCoinControlChangeLabel->setStyleSheet("QLabel{color:black;}");
                    ui->labelCoinControlChangeLabel->setText("");
                }
            }
            else // Known change address
            {
                ui->labelCoinControlChangeLabel->setStyleSheet("QLabel{color:black;}");

                // Query label
                QString associatedLabel = model->getAddressTableModel()->labelForAddress(text);
                if (!associatedLabel.isEmpty())
                    ui->labelCoinControlChangeLabel->setText(associatedLabel);
                else
                    ui->labelCoinControlChangeLabel->setText(tr("(no label)"));

                CoinControlDialog::coinControl->destChange = addr.Get();
            }
        }
    }
}

// Coin Control: update labels
void SendCoinsDialog::coinControlUpdateLabels()
{
    if (!model || !model->getOptionsModel())
        return;

    if (model->getOptionsModel()->getCoinControlFeatures())
    {
        // enable minimum absolute fee UI controls
        ui->radioCustomAtLeast->setVisible(true);

        // only enable the feature if inputs are selected
        ui->radioCustomAtLeast->setEnabled(ui->radioCustomFee->isChecked() && !ui->checkBoxMinimumFee->isChecked() &&CoinControlDialog::coinControl->HasSelected());
    }
    else
    {
        // in case coin control is disabled (=default), hide minimum absolute fee UI controls
        ui->radioCustomAtLeast->setVisible(false);
        return;
    }

    // set pay amounts
    CoinControlDialog::payAmounts.clear();
    CoinControlDialog::fSubtractFeeFromAmount = false;
    for(int i = 0; i < ui->entries->count(); ++i)
    {
        SendCoinsEntry *entry = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(i)->widget());
        if(entry && !entry->isHidden())
        {
            SendCoinsRecipient rcp = entry->getValue();
            CoinControlDialog::payAmounts.append(rcp.amount);
            if (rcp.fSubtractFeeFromAmount)
                CoinControlDialog::fSubtractFeeFromAmount = true;
        }
    }

    if (CoinControlDialog::coinControl->HasSelected())
    {
        // actual coin control calculation
        CoinControlDialog::updateLabels(model, this);

        // show coin control stats
        ui->labelCoinControlAutomaticallySelected->hide();
        ui->widgetCoinControl->show();
    }
    else
    {
        // hide coin control stats
        ui->labelCoinControlAutomaticallySelected->show();
        ui->widgetCoinControl->hide();
        ui->labelCoinControlInsuffFunds->hide();
    }
}

SendConfirmationDialog::SendConfirmationDialog(const QString &title, const QString &text, int _secDelay,
    QWidget *parent) :
    QMessageBox(QMessageBox::Question, title, text, QMessageBox::Yes | QMessageBox::Cancel, parent), secDelay(_secDelay)
{
    setDefaultButton(QMessageBox::Cancel);
    yesButton = button(QMessageBox::Yes);
    updateYesButton();
    connect(&countDownTimer, SIGNAL(timeout()), this, SLOT(countDown()));
}

int SendConfirmationDialog::exec()
{
    updateYesButton();
    countDownTimer.start(1000);
    return QMessageBox::exec();
}

void SendConfirmationDialog::countDown()
{
    secDelay--;
    updateYesButton();

    if(secDelay <= 0)
    {
        countDownTimer.stop();
    }
}

void SendConfirmationDialog::updateYesButton()
{
    if(secDelay > 0)
    {
        yesButton->setEnabled(false);
        yesButton->setText(tr("Yes") + " (" + QString::number(secDelay) + ")");
    }
    else
    {
        yesButton->setEnabled(true);
        yesButton->setText(tr("Yes"));
    }
}
