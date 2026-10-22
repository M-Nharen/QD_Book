// fees_a.cc -- compiled with an old copy of the header: fee is 3 bp
inline long fee_bp() { return 3; }
long fee_from_a(long notional) { return notional * fee_bp() / 10000; }
