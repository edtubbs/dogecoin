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
the transaction outputs):

```
python3 qa/rpc-tests/pqc_testnet_checkpoint_scan.py \
  --srcdir /path/to/dogecoin/src \
  --log-file /path/to/log.txt \
  --datadir /path/to/testnet-datadir
```

```
date_utc:
network:
txid:
height:
commitment_type: FLC1|DIL2
script_pub_key_hex:
commitment_hex:
pubkey_hex:
signature_hex:
recomputed_commitment_hex:
match: true|false
notes:
```
