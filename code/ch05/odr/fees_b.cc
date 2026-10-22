// fees_b.cc -- compiled with the new header: fee is 2 bp
inline long fee_bp() { return 2; }
long fee_from_b(long notional) { return notional * fee_bp() / 10000; }
