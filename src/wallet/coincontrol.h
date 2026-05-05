// Copyright (c) 2011-2016 The Bitcoin Core developers
// Copyright (c) 2021-2022 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_COINCONTROL_H
#define BITCOIN_WALLET_COINCONTROL_H

#include "primitives/transaction.h"
#include "dogecoin-fees.h"

/** Coin Control Features. */
class CCoinControl
{
public:
    CTxDestination destChange;
    //! If false, allows unselected inputs, but requires all selected inputs be used
    bool fAllowOtherInputs;
    //! Includes watch only addresses which match the ISMINE_WATCH_SOLVABLE criteria
    bool fAllowWatchOnly;
    //! Minimum absolute fee (not per kilobyte)
    CAmount nMinimumTotalFee;
    //! Override estimated feerate
    bool fOverrideFeeRate;
    //! Feerate to use if overrideFeeRate is true
    CFeeRate nFeeRate;
    //! Override the default transaction speed, 0 = use default
    FeeRatePreset nPriority;
    //! Desired change output position (-1 = random, as chosen by CreateTransaction).
    //! Used by the PQC carrier flow to pin the final TX_C layout to the same
    //! change position used when signing the TX_BASE template, so the
    //! reconstructed TX_BASE sighash32 is identical on signer and verifier.
    int nChangePosition;
    //! Desired nLockTime for the new transaction (-1 = unset, use the default
    //! GetLocktimeForNewTransaction() behavior). Used by the PQC carrier flow
    //! to pin the final TX_C's nLockTime to the same value that was used when
    //! signing the TX_BASE template, so sighash32(TX_BASE) recomputed from the
    //! on-chain TX_C matches what was actually signed. Without this, the
    //! stochastic ~10% locktime back-dating inside GetLocktimeForNewTransaction
    //! can cause the final TX_C's nLockTime to drift from the signed template
    //! and break SPV cross-verification (e.g. libdogecoin).
    int64_t nLockTime;

    CCoinControl()
    {
        SetNull();
    }

    void SetNull()
    {
        destChange = CNoDestination();
        fAllowOtherInputs = false;
        fAllowWatchOnly = false;
        setSelected.clear();
        nMinimumTotalFee = 0;
        nFeeRate = CFeeRate(0);
        fOverrideFeeRate = false;
        nPriority = MINIMUM;
        nChangePosition = -1;
        nLockTime = -1;
    }

    bool HasSelected() const
    {
        return (setSelected.size() > 0);
    }

    bool IsSelected(const COutPoint& output) const
    {
        return (setSelected.count(output) > 0);
    }

    void Select(const COutPoint& output)
    {
        setSelected.insert(output);
    }

    void UnSelect(const COutPoint& output)
    {
        setSelected.erase(output);
    }

    void UnSelectAll()
    {
        setSelected.clear();
    }

    void ListSelected(std::vector<COutPoint>& vOutpoints) const
    {
        vOutpoints.assign(setSelected.begin(), setSelected.end());
    }

private:
    std::set<COutPoint> setSelected;
};

#endif // BITCOIN_WALLET_COINCONTROL_H
