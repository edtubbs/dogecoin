// Copyright (c) 2011-2016 The Bitcoin Core developers
// Copyright (c) 2022 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "config/bitcoin-config.h"

#include "transactionrecord.h"

#include "base58.h"
#include "consensus/consensus.h"
#include "validation.h"
#include "timedata.h"
#include "wallet/wallet.h"

#if ENABLE_LIBOQS
#include "pqc/pqc_commitment.h"
#endif

#include <stdint.h>

#include <boost/foreach.hpp>

/* Return positive answer if transaction should be shown in list.
 */
bool TransactionRecord::showTransaction(const CWalletTx &wtx)
{
    if (wtx.IsCoinBase())
    {
        // Ensures we show generated coins / mined transactions at depth 1
        if (!wtx.IsInMainChain())
        {
            return false;
        }
    }
    return true;
}

/*
 * Decompose CWallet transaction to model transaction records.
 */
QList<TransactionRecord> TransactionRecord::decomposeTransaction(const CWallet *wallet, const CWalletTx &wtx)
{
    QList<TransactionRecord> parts;
    int64_t nTime = wtx.GetTxTime();
    CAmount nCredit = wtx.GetCredit(ISMINE_ALL);
    CAmount nDebit = wtx.GetDebit(ISMINE_ALL);
    CAmount nNet = nCredit - nDebit;
    uint256 hash = wtx.GetHash();
    std::map<std::string, std::string> mapValue = wtx.mapValue;

    if (nNet > 0 || wtx.IsCoinBase())
    {
        //
        // Credit
        //
        for(unsigned int i = 0; i < wtx.tx->vout.size(); i++)
        {
            const CTxOut& txout = wtx.tx->vout[i];
            isminetype mine = wallet->IsMine(txout);
            if(mine)
            {
                TransactionRecord sub(hash, nTime);
                CTxDestination address;
                sub.idx = i; // vout index
                sub.credit = txout.nValue;
                sub.involvesWatchAddress = mine & ISMINE_WATCH_ONLY;
                if (ExtractDestination(txout.scriptPubKey, address) && IsMine(*wallet, address))
                {
                    // Received by Bitcoin Address
                    sub.type = TransactionRecord::RecvWithAddress;
                    sub.address = CBitcoinAddress(address).ToString();
                }
                else
                {
                    // Received by IP connection (deprecated features), or a multisignature or other non-simple transaction
                    sub.type = TransactionRecord::RecvFromOther;
                    sub.address = mapValue["from"];
                }
                if (wtx.IsCoinBase())
                {
                    // Generated
                    sub.type = TransactionRecord::Generated;
                }

                parts.append(sub);
            }
        }
    }
    else
    {
        bool involvesWatchAddress = false;
        isminetype fAllFromMe = ISMINE_SPENDABLE;
        BOOST_FOREACH(const CTxIn& txin, wtx.tx->vin)
        {
            isminetype mine = wallet->IsMine(txin);
            if(mine & ISMINE_WATCH_ONLY) involvesWatchAddress = true;
            if(fAllFromMe > mine) fAllFromMe = mine;
        }

        isminetype fAllToMe = ISMINE_SPENDABLE;
        BOOST_FOREACH(const CTxOut& txout, wtx.tx->vout)
        {
            isminetype mine = wallet->IsMine(txout);
            if(mine & ISMINE_WATCH_ONLY) involvesWatchAddress = true;
            if(fAllToMe > mine) fAllToMe = mine;
        }

        if (fAllFromMe && fAllToMe)
        {
            // Payment to self
            CAmount nChange = wtx.GetChange();

            parts.append(TransactionRecord(hash, nTime, TransactionRecord::SendToSelf, "",
                            -(nDebit - nChange), nCredit - nChange));
            parts.last().involvesWatchAddress = involvesWatchAddress;   // maybe pass to TransactionRecord as constructor argument
        }
        else if (fAllFromMe)
        {
            //
            // Debit
            //
            CAmount nTxFee = nDebit - wtx.tx->GetValueOut();

            for (unsigned int nOut = 0; nOut < wtx.tx->vout.size(); nOut++)
            {
                const CTxOut& txout = wtx.tx->vout[nOut];
                TransactionRecord sub(hash, nTime);
                sub.idx = nOut;
                sub.involvesWatchAddress = involvesWatchAddress;

                if(wallet->IsMine(txout))
                {
                    // Ignore parts sent to self, as this is usually the change
                    // from a transaction sent back to our own address.
                    continue;
                }

                // Skip unspendable outputs (OP_RETURN) that carry no value,
                // unless this is a PQC commitment output (handled below).
#if ENABLE_LIBOQS
                {
                    PQCCommitmentType opRetType;
                    uint256 opRetCommitment;
                    if (txout.scriptPubKey.IsUnspendable() && txout.nValue == 0 &&
                        PQCExtractCommitment(txout.scriptPubKey, opRetType, opRetCommitment))
                    {
                        // PQC commitment OP_RETURN — show as a distinct entry
                        sub.type = TransactionRecord::SendToOther;
                        sub.address = opRetCommitment.GetHex();
                        sub.debit = 0;
                        sub.pqcRole = PqcTxCCommitment;
                        sub.pqcCommitmentHash = opRetCommitment.GetHex();
                        sub.pqcCommitmentAlgorithm = PQCCommitmentTypeToString(opRetType);
                        parts.append(sub);
                        continue;
                    }
                }
#endif
                CTxDestination address;
                if (ExtractDestination(txout.scriptPubKey, address))
                {
                    // Sent to Bitcoin Address
                    sub.type = TransactionRecord::SendToAddress;
                    sub.address = CBitcoinAddress(address).ToString();
                }
                else
                {
                    // Sent to IP, or other non-address transaction like OP_EVAL
                    sub.type = TransactionRecord::SendToOther;
                    sub.address = mapValue["to"];
                }

                CAmount nValue = txout.nValue;
                /* Add fee to first output */
                if (nTxFee > 0)
                {
                    nValue += nTxFee;
                    nTxFee = 0;
                }
                sub.debit = -nValue;

                parts.append(sub);
            }
        }
        else
        {
            //
            // Mixed debit transaction, can't break down payees
            //
            parts.append(TransactionRecord(hash, nTime, TransactionRecord::Other, "", nNet, 0));
            parts.last().involvesWatchAddress = involvesWatchAddress;
        }
    }

#if ENABLE_LIBOQS
    // Detect PQC transaction roles and tag the records
    {
        // Check if TX_C (has PQC commitment OP_RETURN)
        PQCCommitmentType pqcType;
        uint256 pqcCommitment;
        uint32_t pqcOutputIndex = 0;
        bool isTxC = PQCExtractCommitmentFromTx(*wtx.tx, pqcType, pqcCommitment, pqcOutputIndex);

        // Check if TX_R (has carrier scriptSig in inputs)
        bool isTxR = false;
        if (!isTxC) {
            for (uint32_t i = 0; i < wtx.tx->vin.size(); ++i) {
                PQCCommitmentType inputType;
                if (PQCDetectCarrierScriptSig(wtx.tx->vin[i].scriptSig, inputType)) {
                    isTxR = true;
                    break;
                }
            }
        }

        if (isTxC || isTxR) {
            PqcRole role = isTxC ? PqcTxC : PqcTxR;
            for (int i = 0; i < parts.size(); ++i) {
                // Don't overwrite PqcTxCCommitment — it was already set
                // for the OP_RETURN commitment output during debit processing.
                if (parts[i].pqcRole == PqcNone)
                    parts[i].pqcRole = role;
            }
        }
    }
#endif

    return parts;
}

void TransactionRecord::updateStatus(const CWalletTx &wtx)
{
    AssertLockHeld(cs_main);
    // Determine transaction status

    // Find the block the tx is in
    CBlockIndex* pindex = NULL;
    BlockMap::iterator mi = mapBlockIndex.find(wtx.hashBlock);
    if (mi != mapBlockIndex.end())
        pindex = (*mi).second;

    // Sort order, unrecorded transactions sort to the top.
    //
    // Time field: for confirmed transactions use the stable block timestamp so
    // that all transactions in the same block share an identical time field and
    // the sort-index (field 4) can cleanly order them.  For unconfirmed
    // transactions fall back to nTimeReceived.
    //
    // PQC ordering: TX_R (reveal) is the later event and should appear ABOVE
    // TX_C (commitment) in the newest-first transaction list.
    //
    // TX_R records receive a sort-index boost (+1000) so they rank higher than
    // TX_C within the same block (same stable block-time field).  For
    // unconfirmed TX_R a small time boost (+5 s) ensures TX_R sits above TX_C
    // even when both land in the wallet within the same second or when mempool
    // propagation is slightly unpredictable.
    //
    // The idx field is widened to %04d to accommodate the boost without
    // overflow (a TX_C realistically has far fewer than ~1000 sub-records).
    unsigned int sort_time = wtx.nTimeReceived;
    int sort_idx = idx;
#if ENABLE_LIBOQS
    static const int          PQC_TXR_IDX_BOOST  = 1000;
    static const unsigned int PQC_TXR_TIME_BOOST = 5;   // seconds
    if (pqcRole == PqcTxR) {
        sort_idx += PQC_TXR_IDX_BOOST;
        if (!pindex)
            sort_time += PQC_TXR_TIME_BOOST;
    }
#endif
    // Overwrite with stable block time for confirmed transactions (after the
    // ENABLE_LIBOQS block so the unconfirmed time boost only fires when !pindex).
    if (pindex)
        sort_time = (unsigned int)pindex->nTime;

    status.sortKey = strprintf("%010d-%01d-%010u-%04d",
        (pindex ? pindex->nHeight : std::numeric_limits<int>::max()),
        (wtx.IsCoinBase() ? 1 : 0),
        sort_time,
        sort_idx);
    status.countsForBalance = wtx.IsTrusted() && !(wtx.GetBlocksToMaturity() > 0);
    status.depth = wtx.GetDepthInMainChain();
    status.cur_num_blocks = chainActive.Height();

    if (!CheckFinalTx(wtx))
    {
        if (wtx.tx->nLockTime < LOCKTIME_THRESHOLD)
        {
            status.status = TransactionStatus::OpenUntilBlock;
            status.open_for = wtx.tx->nLockTime - chainActive.Height();
        }
        else
        {
            status.status = TransactionStatus::OpenUntilDate;
            status.open_for = wtx.tx->nLockTime;
        }
    }
    // For generated transactions, determine maturity
    else if(type == TransactionRecord::Generated)
    {
        if (wtx.GetBlocksToMaturity() > 0)
        {
            status.status = TransactionStatus::Immature;

            if (wtx.IsInMainChain())
            {
                status.matures_in = wtx.GetBlocksToMaturity();
            }
            else
            {
                status.status = TransactionStatus::NotAccepted;
            }
        }
        else
        {
            status.status = TransactionStatus::Confirmed;
        }
    }
    else
    {
        if (status.depth < 0)
        {
            status.status = TransactionStatus::Conflicted;
        }
        else if (status.depth == 0)
        {
            status.status = TransactionStatus::Unconfirmed;
            if (wtx.isAbandoned())
                status.status = TransactionStatus::Abandoned;
        }
        else if (status.depth < RecommendedNumConfirmations)
        {
            status.status = TransactionStatus::Confirming;
        }
        else
        {
            status.status = TransactionStatus::Confirmed;
        }
    }

}

bool TransactionRecord::statusUpdateNeeded()
{
    AssertLockHeld(cs_main);
    return status.cur_num_blocks != chainActive.Height();
}

QString TransactionRecord::getTxID() const
{
    return QString::fromStdString(hash.ToString());
}

int TransactionRecord::getOutputIndex() const
{
    return idx;
}
