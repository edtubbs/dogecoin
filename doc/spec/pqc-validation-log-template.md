# PQC Commitment Validation Log (Template)

Use this template while validating testnet/mainnet transactions carrying PQC
commitment OP_RETURN outputs.

Quick validation from an end-to-end log file:

```
python3 contrib/pqc/verify_commitment.py --log-file /path/to/log.txt
```

The command recomputes `SHA256(pubkey || signature)`, rebuilds the canonical
script (`6a24 + tag + commitment`), and returns non-zero if `commitment_hex` or
`script_pub_key_hex` in the log do not match.

On-chain testnet checkpoint scan (starts `dogecoind -testnet -checkpoints=1`,
waits for sync, finds `txid` at `height`, and verifies computed PQC script in
the transaction outputs). You can provide either direct Core end-to-end fields
or an optional key/value `--log-file`:

```
python3 qa/rpc-tests/pqc_testnet_checkpoint_scan.py \
  --srcdir /path/to/dogecoin/src \
  --txid <txid> \
  --height <height> \
  --commitment-type FLC1 \
  --pubkey-hex <pubkey_hex> \
  --signature-hex <signature_hex> \
  --wallet-address <optional_testnet_address> \
  --output-log /path/to/core-e2e-log.txt \
  --datadir /path/to/testnet-datadir
```

`--output-log` writes a template-compatible key/value logfile for the Core-based
end-to-end run (including `match`, checkpoint data, and on-chain match flags).
If omitted, the script still writes a logfile to
`core-e2e-validation-<txid>.log` in the current working directory.
For libdogecoin E2E interoperability, CLI aliases are supported:
`--txis` (`--txid`), `--tag` (`--commitment-type`), `--pubkey`,
`--signature`, and `--dogecoin-testnet-wallet-address`.
An example captured Core run artifact is committed at
`doc/spec/core-e2e-validation-sample.txt`.

```
date_utc:
network:
txid:
height:
commitment_type: FLC1
script_pub_key_hex:
commitment_hex:
pubkey_hex:
signature_hex:
recomputed_commitment_hex:
match: true|false
notes:
```
