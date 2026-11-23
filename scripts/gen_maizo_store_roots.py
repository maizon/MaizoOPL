#!/usr/bin/env python3
"""Generate native BearSSL CA trust anchors from a REVIEWED PEM file.

Dependency-free DER parser. Only self-issued roots with supported RSA or
NIST EC public keys are accepted. Not a general certificate path builder;
BearSSL performs the actual chain validation and hostname verification.
"""
from pathlib import Path
import base64
import re

PEM = Path("thirdparty/maizo_store_roots.pem")
HEADER = Path("include/maizo_store_roots.h")

def tlv(buf, pos):
    if pos + 2 > len(buf):
        raise ValueError("Truncated ASN.1 tag")
    tag = buf[pos]
    n = buf[pos+1]
    p = pos+2
    if n & 128:
        count = n & 127
        if count == 0 or count > 4 or p+count > len(buf):
            raise ValueError("Indefinite/oversized ASN.1 length")
        n = int.from_bytes(buf[p:p+count], "big")
        p += count
    end = p+n
    if end > len(buf) or end < p:
        raise ValueError("ASN.1 child exceeds container")
    return tag, p, end, end

def children(b, begin, end):
    out=[]
    p=begin
    while p<end:
        tag, start, finish, nxt=tlv(b,p)
        if nxt>end:
            raise ValueError("ASN.1 nested length")
        out.append((tag,p,start,finish,nxt))
        p=nxt
    return out

def decode_root(der):
    tag, a,b,_=tlv(der,0)
    if tag!=0x30 or b!=len(der):
        raise ValueError("Invalid X.509 certificate")
    parts=children(der,a,b)
    if not parts or parts[0][0]!=0x30:
        raise ValueError("TBS missing")
    tbs=children(der,parts[0][2],parts[0][3])
    if tbs[0][0]==0xa0: tbs=tbs[1:]
    if len(tbs)<6 or tbs[0][0]!=2 or tbs[2][0]!=0x30 or tbs[4][0]!=0x30:
        raise ValueError("Unexpected certificate fields")
    issuer,subject,spki=tbs[2],tbs[4],tbs[5]
    dn=der[subject[1]:subject[4]]
    if der[issuer[1]:issuer[4]]!=dn:
        raise ValueError("Not self-issued CA")
    if spki[0]!=0x30:
        raise ValueError("SPKI missing")
    spki_parts=children(der,spki[2],spki[3])
    if len(spki_parts)!=2 or spki_parts[1][0]!=3:
        raise ValueError("Invalid SPKI")
    alg=children(der,spki_parts[0][2],spki_parts[0][3])
    if not alg or alg[0][0]!=6:
        raise ValueError("SPKI OID missing")
    oid=der[alg[0][2]:alg[0][3]]
    bits=der[spki_parts[1][2]:spki_parts[1][3]]
    if not bits or bits[0]:
        raise ValueError("Unsupported padded public key")
    key=bits[1:]
    if oid==bytes.fromhex("2a864886f70d010101"):  # rsaEncryption
        tag, x,y,_=tlv(key,0)
        if tag!=0x30: raise ValueError("RSA sequence missing")
        parts=children(key,x,y)
        if len(parts)<2 or parts[0][0]!=2 or parts[1][0]!=2:
            raise ValueError("Invalid RSA key")
        n=key[parts[0][2]:parts[0][3]].lstrip(b"\x00")
        e=key[parts[1][2]:parts[1][3]].lstrip(b"\x00")
        if not (128<=len(n)<=512 and 1<=len(e)<=8):
            raise ValueError("Unexpected RSA key length")
        return ("rsa",dn,n,e)
    if oid==bytes.fromhex("2a8648ce3d0201"):   # id-ecPublicKey
        if len(alg)<2 or alg[1][0]!=6:
            raise ValueError("EC curve missing")
        curve=der[alg[1][2]:alg[1][3]]
        curves={bytes.fromhex("2a8648ce3d030107"):"BR_EC_secp256r1",
                bytes.fromhex("2b81040022"):"BR_EC_secp384r1"}
        if curve not in curves:
            raise ValueError("Unsupported EC curve: "+curve.hex())
        return ("ec",dn,key,curves[curve])
    raise ValueError("Unsupported public key OID "+oid.hex())

def carr(name,data):
    values=[f"0x{b:02x}" for b in data]
    lines=[", ".join(values[i:i+12]) for i in range(0,len(values),12)]
    return f"static unsigned char {name}[] = {{\n    "+",\n    ".join(lines)+"\n};\n"

def main():
    pem=PEM.read_text(encoding="ascii")
    blocks=re.findall(r"-----BEGIN CERTIFICATE-----([A-Za-z0-9+/=\s]+)-----END CERTIFICATE-----", pem)
    roots=[decode_root(base64.b64decode(x)) for x in blocks]
    if not 5<=len(roots)<=20:
        raise ValueError(f"Expected 5-20 pinned Mozilla trust roots, have {len(roots)}")
    h=["/* Auto-generated from reviewed Mozilla roots; DO NOT EDIT. */",
       "#ifndef MAIZO_STORE_ROOTS_H", "#define MAIZO_STORE_ROOTS_H",
       "#include <bearssl.h>"]
    entries=[]
    for i,root in enumerate(roots):
        typ,dn,k,extra=root
        h.append(carr(f"maizo_dn_{i}",dn))
        h.append(carr(f"maizo_key_{i}",k))
        if typ=="rsa":
            h.append(carr(f"maizo_exp_{i}",extra))
            pkey=f".key_type=BR_KEYTYPE_RSA, .key.rsa={{.n=maizo_key_{i},.nlen=sizeof(maizo_key_{i}),.e=maizo_exp_{i},.elen=sizeof(maizo_exp_{i})}}"
        else:
            pkey=f".key_type=BR_KEYTYPE_EC, .key.ec={{.curve={extra},.q=maizo_key_{i},.qlen=sizeof(maizo_key_{i})}}"
        entries.append(f"    {{.dn={{maizo_dn_{i}, sizeof(maizo_dn_{i})}}, .flags=BR_X509_TA_CA, .pkey={{{pkey}}}}}")
    h.append("static br_x509_trust_anchor maizo_store_roots[] = {\n"+",\n".join(entries)+"\n};")
    h.append("#define MAIZO_STORE_ROOTS_COUNT (sizeof(maizo_store_roots)/sizeof(maizo_store_roots[0]))")
    h.append("#endif")
    HEADER.write_text("\n".join(h)+"\n",encoding="ascii")
    print(f"Generated {len(roots)} BearSSL trust anchors to {HEADER}")

if __name__=="__main__":
    main()
