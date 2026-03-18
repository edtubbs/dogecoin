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
